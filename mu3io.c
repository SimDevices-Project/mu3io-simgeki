#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#define MU3IO_EXPORTS

#include <windows.h>
#include <setupapi.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdio.h>

#include "mu3io.h"
#include "hid.h"

#define REPORT_SIZE 64  // 1B ReportID + 63B 数据

static uint8_t mu3_opbtn;
static uint8_t mu3_left_btn;
static uint8_t mu3_right_btn;
static int16_t mu3_lever_pos;

static char hid_path[1024];
static size_t hid_path_size = 1024;

static char hid_read_buf[REPORT_SIZE];

HANDLE hid_handle = NULL;
OVERLAPPED ov_read = {0};

#define VID ("VID_0CA3")
#define PID ("PID_0021")
#define MI ("MI_05")

HRESULT hid_on_data(char* dat, size_t length) {
  HidconfigData* data = (HidconfigData*)dat;
  if (length == 64) {
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
          // 读取摇杆位置
          mu3_lever_pos = 0;
          mu3_lever_pos = data->roller_value_sp;
          break;
        case SP_LED_SET:  // 设置LED状态
          // 这里可以处理LED数据，如果需要的话
          // 目前不需要处理LED数据
          break;
        default:
          printf("SimGEKI: Unknown HID command: %02X\n", data->command);
          return E_FAIL;
      }
    }
  }
}

HRESULT hid_write_data(const char* dat, size_t length) {
  if (hid_handle == INVALID_HANDLE_VALUE)
    return E_FAIL;
  DWORD written;
  // 同步写：传 NULL OVERLAPPED
  if (!WriteFile(hid_handle, dat, (DWORD)length, &written, NULL)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return S_OK;
}

uint16_t mu3_io_get_api_version(void) {
  return 0x0101;
}

HRESULT mu3_io_init(void) {
  printf("SimGEKI: --- Begin configuration ---\n");
  printf("SimGEKI: mu3io init...\n");
  if (GetHidPathByVidPidMi(VID, PID, MI, hid_path, &hid_path_size) == S_OK) {
    printf("SimGEKI: HID Path: %s\n", hid_path);
  } else {
    printf("SimGEKI: Failed to get HID Path.\n");
    return E_FAIL;
  }
  hid_handle = CreateFileA(hid_path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
  if (hid_handle == INVALID_HANDLE_VALUE) {
    printf("SimGEKI: Failed to open HID device.\n");
    return E_FAIL;
  } else {
    printf("SimGEKI: HID device opened successfully.\n");
  }

  if (!ov_read.hEvent) {
    printf("SimGEKI: Failed to create read event: %u\n", GetLastError());
    CloseHandle(hid_handle);
    return E_FAIL;
  }

  // 创建用于异步读的事件
  ov_read.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (!ov_read.hEvent) {
    CloseHandle(hid_handle);
    return HRESULT_FROM_WIN32(GetLastError());
  }

  // 发起第一轮异步读
  ResetEvent(ov_read.hEvent);
  if (!ReadFile(hid_handle, hid_read_buf, REPORT_SIZE, NULL, &ov_read) &&
      GetLastError() != ERROR_IO_PENDING) {
    CloseHandle(ov_read.hEvent);
    CloseHandle(hid_handle);
    return HRESULT_FROM_WIN32(GetLastError());
  }

  printf("SimGEKI: ---  End  configuration ---\n");
  return S_OK;
}

// Update input state
HRESULT mu3_io_poll(void) {
  DWORD bytes = 0;
  // 非阻塞检查异步读是否完成
  if (GetOverlappedResult(hid_handle, &ov_read, &bytes, FALSE)) {
    // 交给回调处理原始数据
    hid_on_data(hid_read_buf, bytes);

    // 发起下一次异步读
    ResetEvent(ov_read.hEvent);
    ReadFile(hid_handle, hid_read_buf, REPORT_SIZE, NULL, &ov_read);
  }
  // 发送HID数据要求设备上报
  HidconfigData data = {0};
  data.reportID = HIDCONFIG_REPORT_ID;
  data.symbol = 0x01;
  data.command = SP_INPUT_GET;
  hid_write_data((const char*)&data, sizeof(data));
  return S_OK;
}

void mu3_io_get_opbtns(uint8_t* opbtn) {
  if (opbtn != NULL) {
    *opbtn = mu3_opbtn;
  }
}

void mu3_io_get_gamebtns(uint8_t* left, uint8_t* right) {
  if (left != NULL) {
    *left = mu3_left_btn;
  }

  if (right != NULL) {
    *right = mu3_right_btn;
  }
}

void mu3_io_get_lever(int16_t* pos) {
  if (pos != NULL) {
    *pos = mu3_lever_pos;
  }
}

HRESULT mu3_io_led_init(void) {
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
        if (rgb[i * 3]) {
          data.led_7c[i] |= 1 << 0;  // R
        }
        if (rgb[i * 3 + 1]) {
          data.led_7c[i] |= 1 << 1;  // G
        }
        if (rgb[i * 3 + 2]) {
          data.led_7c[i] |= 1 << 2;  // B
        }
      }
      break;
    default:
      printf("SimGEKI: mu3_io_led_set_colors: Invalid board ID: %02X\n", board);
      return;
      break;
  }
  hid_write_data((const char*)&data, sizeof(data));
}
