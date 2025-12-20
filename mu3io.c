#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#ifndef MU3IO_EXPORTS
#define MU3IO_EXPORTS
#endif

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "util/dprintf.h"

#include "mu3io.h"
#include "hid.h"

#define REPORT_SIZE 64  // 1B ReportID + 63B 数据

// USB reconnection and timeout constants
#define USB_RECONNECT_POLL_INTERVAL 60  // Polls between reconnection attempts
#define USB_WRITE_TIMEOUT_MS 1000       // Write operation timeout in milliseconds

static uint8_t mu3_opbtn;
static uint8_t mu3_left_btn;
static uint8_t mu3_right_btn;
static int16_t mu3_lever_pos;

static char hid_path[1024];
static size_t hid_path_size = 1024;

static char hid_read_buf[REPORT_SIZE];

static uint8_t poll_state = 0;
static bool usb_connected = false;
static bool usb_init_attempted = false;

HANDLE hid_handle = NULL;
OVERLAPPED ov_read = {0};
OVERLAPPED ov_write = {0};  // 新增写用 OVERLAPPED

// #define DEBUG
// #define DEBUG_TEXT_ONLY

// Check if error code indicates USB device disconnection
static bool is_usb_disconnection_error(DWORD error) {
  return (error == ERROR_BAD_COMMAND || 
          error == ERROR_NOT_READY || 
          error == ERROR_DEVICE_NOT_CONNECTED || 
          error == ERROR_GEN_FAILURE ||
          error == ERROR_OPERATION_ABORTED);
}

HRESULT hid_on_data(char* dat, size_t length) {
  HidconfigData* data = (HidconfigData*)dat;
  if (length == 64) {
#ifdef DEBUG
    for (size_t i = 0; i < length; i++) {
      printf("%02X ", (unsigned char)dat[i]);
    }
    printf("\n");
    printf("SimGEKI: HID data received, reportID: %02X, command: %02X\n",
           data->reportID, data->command);
#endif  // DEBUG
    if (data->reportID == HIDCONFIG_REPORT_ID) {
      switch (data->command) {
        case SP_INPUT_GET:  // 获取输入状态
          mu3_opbtn = 0;
          if (data->input_status & BT_COIN) {
            mu3_opbtn |= MU3_IO_OPBTN_COIN;  // 硬币投币按钮
          }
          if (data->input_status & BT_TEST) {
            mu3_opbtn |= MU3_IO_OPBTN_TEST;  // 测试按钮
          }
          if (data->input_status & BT_SERVICE) {
            mu3_opbtn |= MU3_IO_OPBTN_SERVICE;  // 服务按钮
          }
          // 读取游戏按钮状态
          mu3_left_btn = 0;
          mu3_right_btn = 0;
          if (data->input_status & BT_R_A) {
            mu3_right_btn |= MU3_IO_GAMEBTN_1;  // 右侧按钮1
          }
          if (data->input_status & BT_R_B) {
            mu3_right_btn |= MU3_IO_GAMEBTN_2;  // 右侧按钮2
          }
          if (data->input_status & BT_R_C) {
            mu3_right_btn |= MU3_IO_GAMEBTN_3;  // 右侧按钮3
          }
          if (data->input_status & BT_L_A) {
            mu3_left_btn |= MU3_IO_GAMEBTN_1;  // 左侧按钮1
          }
          if (data->input_status & BT_L_B) {
            mu3_left_btn |= MU3_IO_GAMEBTN_2;  // 左侧按钮2
          }
          if (data->input_status & BT_L_C) {
            mu3_left_btn |= MU3_IO_GAMEBTN_3;  // 左侧按钮3
          }
          if (data->input_status & BT_LSIDE) {
            mu3_left_btn |= MU3_IO_GAMEBTN_SIDE;  // 左侧侧键
          }
          if (data->input_status & BT_RSIDE) {
            mu3_right_btn |= MU3_IO_GAMEBTN_SIDE;  // 右侧侧键
          }
          if (data->input_status & BT_RMENU) {
            mu3_right_btn |= MU3_IO_GAMEBTN_MENU;  // 右侧菜单键
          }
          if (data->input_status & BT_LMENU) {
            mu3_left_btn |= MU3_IO_GAMEBTN_MENU;  // 左侧菜单键
          }
          mu3_left_btn ^= MU3_IO_GAMEBTN_SIDE;
          mu3_right_btn ^= MU3_IO_GAMEBTN_SIDE;
          // 读取摇杆位置
          uint16_t lever_pos = 0;
          lever_pos = data->roller_value_sp;
          mu3_lever_pos = ((int32_t)lever_pos) - 0x8000;
#ifdef DEBUG
          dprintf("SimGEKI: Lever position: %04X\n", lever_pos);
          dprintf("SimGEKI: Operator buttons: %02X\n", mu3_opbtn);
          dprintf("SimGEKI: Left game buttons: %02X\n", mu3_left_btn);
          dprintf("SimGEKI: Right game buttons: %02X\n", mu3_right_btn);
#endif  // DEBUG
          break;
        case SP_LED_SET:  // 设置LED状态
          // 这里可以处理LED数据，如果需要的话
          // 目前不需要处理LED数据
          break;
        case SP_INPUT_GET_START:
          poll_state = 1;
          dprintf("SimGEKI: Start poll listening\n");
          break;
        case SP_INPUT_GET_END:
          poll_state = 0;
          dprintf("SimGEKI: Stop poll listening\n");
          break;
        default:
          dprintf("SimGEKI: Unknown HID command: %02X\n", data->command);
          return E_FAIL;
      }
    }
  }
  return S_OK;
}

// Clean up USB resources
static void usb_cleanup(void) {
  if (ov_write.hEvent != NULL) {
    CloseHandle(ov_write.hEvent);
    ov_write.hEvent = NULL;
  }
  if (ov_read.hEvent != NULL) {
    CloseHandle(ov_read.hEvent);
    ov_read.hEvent = NULL;
  }
  if (hid_handle != NULL && hid_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(hid_handle);
    hid_handle = NULL;
  }
  usb_connected = false;
  poll_state = 0;
}

// Initialize USB device
static HRESULT usb_init(void) {
  // Clean up any existing connection first
  usb_cleanup();
  
  dprintf("SimGEKI: Attempting to connect USB device...\n");
  
  // Try to get HID device path
  if (GetHidPathByVidPidMi(VID, PID, MI, hid_path, &hid_path_size) != S_OK) {
    dprintf("SimGEKI: USB device not found.\n");
    return S_FALSE;
  }
  
  dprintf("SimGEKI: HID Path: %s\n", hid_path);
  
  // Try to open the HID device
  hid_handle = CreateFileA(hid_path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (hid_handle == INVALID_HANDLE_VALUE) {
    dprintf("SimGEKI: Failed to open HID device.\n");
    hid_handle = NULL;
    return S_FALSE;
  }
  
  dprintf("SimGEKI: HID device opened successfully.\n");
  
  // Create event for async read
  ov_read.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!ov_read.hEvent) {
    CloseHandle(hid_handle);
    hid_handle = NULL;
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  // Create event for async write
  ov_write.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!ov_write.hEvent) {
    CloseHandle(ov_read.hEvent);
    ov_read.hEvent = NULL;
    CloseHandle(hid_handle);
    hid_handle = NULL;
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  // Start first async read
  ResetEvent(ov_read.hEvent);
  if (!ReadFile(hid_handle, hid_read_buf, REPORT_SIZE, NULL, &ov_read) &&
      GetLastError() != ERROR_IO_PENDING) {
    usb_cleanup();
    return HRESULT_FROM_WIN32(GetLastError());
  }
  
  usb_connected = true;
  dprintf("SimGEKI: USB device initialized successfully.\n");
  return S_OK;
}

HRESULT hid_write_data(const char* dat, size_t length) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: HID write data.\n");
  return S_OK;
#endif  // DEBUG_TEXT_ONLY
  if (!usb_connected || hid_handle == NULL || hid_handle == INVALID_HANDLE_VALUE) {
    return S_FALSE;
  }

  // 异步写：重置写事件并发起 WriteFile
  ResetEvent(ov_write.hEvent);
  DWORD written;
  if (!WriteFile(hid_handle, dat, (DWORD)length, &written, &ov_write) &&
      GetLastError() != ERROR_IO_PENDING) {
    DWORD error = GetLastError();
    dprintf("SimGEKI: WriteFile failed: %lu\n", (unsigned long)error);
    // Check if device disconnected
    if (is_usb_disconnection_error(error)) {
      dprintf("SimGEKI: USB device appears to be disconnected.\n");
      usb_cleanup();
    }
    return HRESULT_FROM_WIN32(error);
  }

  // 等待写操作完成
  DWORD waitResult = WaitForSingleObject(ov_write.hEvent, USB_WRITE_TIMEOUT_MS);
  if (waitResult != WAIT_OBJECT_0) {
    dprintf("SimGEKI: Write operation timeout or failed.\n");
    return E_FAIL;
  }
  
  if (!GetOverlappedResult(hid_handle, &ov_write, &written, FALSE)) {
    DWORD error = GetLastError();
    dprintf("SimGEKI: Overlapped write failed: %lu\n", (unsigned long)error);
    // Check if device disconnected
    if (is_usb_disconnection_error(error)) {
      dprintf("SimGEKI: USB device appears to be disconnected.\n");
      usb_cleanup();
    }
    return E_FAIL;
  }

  return S_OK;
}

uint16_t mu3_io_get_api_version(void) {
  return 0x0101;
}

HRESULT mu3_io_init(void) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: MU3 IO Init.\n");
  return S_OK;
#endif  // DEBUG_TEXT_ONLY
  dprintf("SimGEKI: --- Begin configuration ---\n");
  dprintf("SimGEKI: IO init...\n");
  
  // Initialize USB device, but don't fail if it's not connected
  HRESULT hr = usb_init();
  if (SUCCEEDED(hr)) {
    dprintf("SimGEKI: USB device connected and initialized.\n");
  } else {
    dprintf("SimGEKI: USB device not connected, will retry during polling.\n");
  }
  
  usb_init_attempted = true;
  dprintf("SimGEKI: ---  End  configuration ---\n");
  return S_OK;
}

// Update input state
HRESULT mu3_io_poll(void) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: MU3 IO Poll.\n");
  return S_OK;
#endif  // DEBUG_TEXT_ONLY
#ifdef DEBUG
  dprintf("SimGEKI: MU3 IO Polling\n");
#endif  // DEBUG

  // If USB is not connected, try to connect
  if (!usb_connected) {
    // Only try to reconnect every ~60 polls to avoid spamming
    static int reconnect_counter = 0;
    reconnect_counter++;
    if (reconnect_counter >= USB_RECONNECT_POLL_INTERVAL) {
      reconnect_counter = 0;
      HRESULT hr = usb_init();
      if (SUCCEEDED(hr)) {
        dprintf("SimGEKI: USB device reconnected successfully.\n");
      }
    }
    return S_OK;
  }

  DWORD bytes = 0;
  int packet_count = 0;
  char last_packet[REPORT_SIZE];
  size_t last_packet_size = 0;

  // 循环读取所有可用的包，只保留最后一个
  while (GetOverlappedResult(hid_handle, &ov_read, &bytes, FALSE)) {
    packet_count++;

    // 保存当前包作为最后一个包
    memcpy(last_packet, hid_read_buf, bytes);
    last_packet_size = bytes;

    // 立即发起下一次异步读
    ResetEvent(ov_read.hEvent);
    if (!ReadFile(hid_handle, hid_read_buf, REPORT_SIZE, NULL, &ov_read)) {
      DWORD error = GetLastError();
      if (error != ERROR_IO_PENDING) {
        dprintf("SimGEKI: ReadFile failed: %lu\n", (unsigned long)error);
        // Check if device disconnected
        if (is_usb_disconnection_error(error)) {
          dprintf("SimGEKI: USB device disconnected.\n");
          usb_cleanup();
          return S_OK;
        }
        break;
      }
    }
  }

  // Check if the last GetOverlappedResult failed due to disconnection
  if (packet_count == 0) {
    DWORD error = GetLastError();
    if (error != ERROR_IO_INCOMPLETE && error != ERROR_SUCCESS) {
      // Check if device disconnected
      if (is_usb_disconnection_error(error)) {
        dprintf("SimGEKI: USB device disconnected (error: %lu).\n", (unsigned long)error);
        usb_cleanup();
        return S_OK;
      }
    }
  }

// 如果有包被丢弃，打印日志
#ifdef DEBUG
  if (packet_count > 1) {
    dprintf("SimGEKI: Discarded %d packets, processing only the latest one\n",
            packet_count - 1);
  }
#endif

  // 只处理最后一个包
  if (packet_count > 0) {
    hid_on_data(last_packet, last_packet_size);
  }

  // 发送HID数据要求设备开始持续上报
  if (poll_state == 0) {
    HidconfigData data = {0};
    data.reportID = HIDCONFIG_REPORT_ID;
    data.symbol = 0x01;
    data.command = SP_INPUT_GET_START;
    hid_write_data((const char*)&data, sizeof(data));
  }

  return S_OK;
}

void mu3_io_get_opbtns(uint8_t* opbtn) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: MU3 IO Get Operator Buttons.\n");
  if (opbtn != NULL) {
    *opbtn = 0;
  }
  return;
#endif  // DEBUG_TEXT_ONLY
#ifdef DEBUG
  dprintf("SimGEKI: MU3 IO Get Operator Buttons\n");
#endif  // DEBUG
  static uint8_t prevent_mu3_opbtn = 0;
  if (opbtn != NULL) {
    *opbtn = 0;
    *opbtn = mu3_opbtn;
    if ((mu3_opbtn & MU3_IO_OPBTN_COIN) &&
        (prevent_mu3_opbtn & MU3_IO_OPBTN_COIN)) {
      *opbtn &= ~MU3_IO_OPBTN_COIN;  // 禁止重复投币
    }
  }
  prevent_mu3_opbtn = mu3_opbtn;
}

void mu3_io_get_gamebtns(uint8_t* left, uint8_t* right) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: MU3 IO Get Game Buttons.\n");
  if (left != NULL) {
    *left = 0;
  }
  if (right != NULL) {
    *right = 0;
  }
  return;
#endif  // DEBUG_TEXT_ONLY
#ifdef DEBUG
  dprintf("SimGEKI: MU3 IO Get Game Buttons\n");
#endif  // DEBUG
  if (left != NULL) {
    *left = mu3_left_btn;
  }

  if (right != NULL) {
    *right = mu3_right_btn;
  }
}

void mu3_io_get_lever(int16_t* pos) {
#ifdef DEBUG_TEXT_ONLY
  dprintf("SimGEKI: MU3 IO Get Lever Position.\n");
  if (pos != NULL) {
    *pos = 0;
  }
  return;
#endif  // DEBUG_TEXT_ONLY
#ifdef DEBUG
  dprintf("SimGEKI: MU3 IO Get Lever Position\n");
#endif  // DEBUG
  if (pos != NULL) {
    *pos = mu3_lever_pos;
  }
}

HRESULT mu3_io_led_init(void) {
  dprintf("SimGEKI: MU3 IO LED init...\n");
  return S_OK;
}

void mu3_io_led_set_colors(uint8_t board, uint8_t* rgb) {
  // 发送HID数据要求设备更新LED
  HidconfigData data = {0};
  data.reportID = HIDCONFIG_REPORT_ID;
  data.symbol = 0x02;
  data.command = SP_LED_SET;
  // Only handled Side LEDs
  switch (board) {
    // Side
    case 0x00:
      data.board_id = 0x00;
      if (rgb == NULL) {
        return;
      }
      data.led_rgb_left[0][0] = rgb[0];
      data.led_rgb_left[0][1] = rgb[1];
      data.led_rgb_left[0][2] = rgb[2];
      data.led_rgb_left[1][0] = rgb[0];
      data.led_rgb_left[1][1] = rgb[1];
      data.led_rgb_left[1][2] = rgb[2];
      data.led_rgb_left[2][0] = rgb[0];
      data.led_rgb_left[2][1] = rgb[1];
      data.led_rgb_left[2][2] = rgb[2];
      data.led_rgb_left[3][0] = rgb[3];
      data.led_rgb_left[3][1] = rgb[4];
      data.led_rgb_left[3][2] = rgb[5];
      data.led_rgb_left[4][0] = rgb[3];
      data.led_rgb_left[4][1] = rgb[4];
      data.led_rgb_left[4][2] = rgb[5];
      data.led_rgb_left[5][0] = rgb[3];
      data.led_rgb_left[5][1] = rgb[4];
      data.led_rgb_left[5][2] = rgb[5];

      data.led_rgb_right[0][0] = rgb[180];
      data.led_rgb_right[0][1] = rgb[181];
      data.led_rgb_right[0][2] = rgb[182];
      data.led_rgb_right[1][0] = rgb[180];
      data.led_rgb_right[1][1] = rgb[181];
      data.led_rgb_right[1][2] = rgb[182];
      data.led_rgb_right[2][0] = rgb[180];
      data.led_rgb_right[2][1] = rgb[181];
      data.led_rgb_right[2][2] = rgb[182];
      data.led_rgb_right[3][0] = rgb[177];
      data.led_rgb_right[3][1] = rgb[178];
      data.led_rgb_right[3][2] = rgb[179];
      data.led_rgb_right[4][0] = rgb[177];
      data.led_rgb_right[4][1] = rgb[178];
      data.led_rgb_right[4][2] = rgb[179];
      data.led_rgb_right[5][0] = rgb[177];
      data.led_rgb_right[5][1] = rgb[178];
      data.led_rgb_right[5][2] = rgb[179];
      break;
    // 7C
    case 0x01:
      data.board_id = 0x01;
      if (rgb == NULL) {
        return;
      }
      for (size_t i = 0; i < 6; i++) {
        data.led_7c[i] = 0;
        data.led_7c[i] |= rgb[i * 3 + 0] > 0 ? 1 << 0 : 0;  // R
        data.led_7c[i] |= rgb[i * 3 + 1] > 0 ? 1 << 1 : 0;  // G
        data.led_7c[i] |= rgb[i * 3 + 2] > 0 ? 1 << 2 : 0;  // B
      }
      break;
    default:
      dprintf("SimGEKI: mu3_io_led_set_colors: Invalid board ID: %02X\n",
              board);
      return;
      break;
  }
  hid_write_data((const char*)&data, sizeof(data));
}
