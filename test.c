#include "hid.h"
#include "mu3io.h"

#include <stdio.h>
#include <string.h>

void print_separator(const char* title) {
  printf("\n========== %s ==========\n", title);
}

void test_api_version() {
  print_separator("API Version Test");
  uint16_t api_version = mu3_io_get_api_version();
  printf("API Version: 0x%04X (%d)\n", api_version, api_version);
}

void test_hid_path() {
  print_separator("HID Path Test");
  const char vid[9] = "VID_0CA3";
  const char pid[9] = "PID_0021";
  const char mi[6] = "MI_05";
  char path[1024];
  size_t path_size = 1024;
  
  printf("VID: %s\n", vid);
  printf("PID: %s\n", pid);
  printf("MI: %s\n", mi);
  
  HRESULT result = GetHidPathByVidPidMi(vid, pid, mi, path, &path_size);
  printf("GetHidPathByVidPidMi Result: 0x%08X\n", result);
  if (SUCCEEDED(result)) {
    printf("HID Path: %s\n", path);
    printf("Path Length: %zu\n", strlen(path));
  } else {
    printf("Failed to get HID path\n");
  }
}

void test_initialization() {
  print_separator("Initialization Test");
  
  // Test mu3_io_init
  HRESULT result = mu3_io_init();
  printf("mu3_io_init Result: 0x%08X\n", result);
  if (SUCCEEDED(result)) {
    printf("IO initialization: SUCCESS\n");
  } else {
    printf("IO initialization: FAILED\n");
  }
  
  // Test LED init
  result = mu3_io_led_init();
  printf("mu3_io_led_init Result: 0x%08X\n", result);
  if (SUCCEEDED(result)) {
    printf("LED initialization: SUCCESS\n");
  } else {
    printf("LED initialization: FAILED\n");
  }
}

void test_input_polling() {
  print_separator("Input Polling Test");
  
  printf("Polling for input data (10 times with 500ms intervals)...\n");
  
  for (int i = 0; i < 10; i++) {
    printf("\n--- Poll %d ---\n", i + 1);
    
    // Poll for new data
    HRESULT result = mu3_io_poll();
    printf("mu3_io_poll Result: 0x%08X\n", result);
    
    // Get operator buttons
    uint8_t opbtns = 0;
    mu3_io_get_opbtns(&opbtns);
    printf("Operator Buttons: 0x%02X", opbtns);
    if (opbtns) {
      printf(" (");
      if (opbtns & MU3_IO_OPBTN_TEST) printf("TEST ");
      if (opbtns & MU3_IO_OPBTN_SERVICE) printf("SERVICE ");
      if (opbtns & MU3_IO_OPBTN_COIN) printf("COIN ");
      printf(")");
    }
    printf("\n");
    
    // Get game buttons
    uint8_t left_btns = 0, right_btns = 0;
    mu3_io_get_gamebtns(&left_btns, &right_btns);
    printf("Left Game Buttons:  0x%02X", left_btns);
    if (left_btns) {
      printf(" (");
      if (left_btns & MU3_IO_GAMEBTN_1) printf("1 ");
      if (left_btns & MU3_IO_GAMEBTN_2) printf("2 ");
      if (left_btns & MU3_IO_GAMEBTN_3) printf("3 ");
      if (left_btns & MU3_IO_GAMEBTN_SIDE) printf("SIDE ");
      if (left_btns & MU3_IO_GAMEBTN_MENU) printf("MENU ");
      printf(")");
    }
    printf("\n");
    
    printf("Right Game Buttons: 0x%02X", right_btns);
    if (right_btns) {
      printf(" (");
      if (right_btns & MU3_IO_GAMEBTN_1) printf("1 ");
      if (right_btns & MU3_IO_GAMEBTN_2) printf("2 ");
      if (right_btns & MU3_IO_GAMEBTN_3) printf("3 ");
      if (right_btns & MU3_IO_GAMEBTN_SIDE) printf("SIDE ");
      if (right_btns & MU3_IO_GAMEBTN_MENU) printf("MENU ");
      printf(")");
    }
    printf("\n");
    
    // Get lever position
    int16_t lever_pos = 0;
    mu3_io_get_lever(&lever_pos);
    printf("Lever Position: 0x%04X (%d)\n", (uint16_t)lever_pos, lever_pos);
    
    Sleep(500);  // Wait 500ms between polls
  }
}

void test_led_control() {
  print_separator("LED Control Test");
  
  printf("Testing LED controls...\n");
  
  // Test Side LEDs (board 0x00)
  printf("\nTesting Side LEDs (Board 0x00):\n");
  uint8_t side_rgb[183] = {0}; // Initialize all to 0
  
  // Set left side to red
  side_rgb[0] = 255; side_rgb[1] = 0; side_rgb[2] = 0;
  side_rgb[3] = 255; side_rgb[4] = 0; side_rgb[5] = 0;
  
  // Set right side to blue  
  side_rgb[180] = 0; side_rgb[181] = 0; side_rgb[182] = 255;
  side_rgb[177] = 0; side_rgb[178] = 0; side_rgb[179] = 255;
  
  mu3_io_led_set_colors(0x00, side_rgb);
  printf("Set Side LEDs: Left=Red, Right=Blue\n");
  Sleep(2000);
  
  // Turn off side LEDs
  memset(side_rgb, 0, sizeof(side_rgb));
  mu3_io_led_set_colors(0x00, side_rgb);
  printf("Turned off Side LEDs\n");
  Sleep(1000);
  
  // Test 7C LEDs (board 0x01)
  printf("\nTesting 7C LEDs (Board 0x01):\n");
  uint8_t c7_rgb[18] = {0}; // 6 LEDs * 3 colors
  
  // Set all 7C LEDs to green
  for (int i = 0; i < 6; i++) {
    c7_rgb[i * 3 + 0] = 0;   // R
    c7_rgb[i * 3 + 1] = 255; // G
    c7_rgb[i * 3 + 2] = 0;   // B
  }
  
  mu3_io_led_set_colors(0x01, c7_rgb);
  printf("Set 7C LEDs to Green\n");
  Sleep(2000);
  
  // Turn off 7C LEDs
  memset(c7_rgb, 0, sizeof(c7_rgb));
  mu3_io_led_set_colors(0x01, c7_rgb);
  printf("Turned off 7C LEDs\n");
  Sleep(1000);
  
  // Test invalid board ID
  printf("\nTesting invalid board ID (0xFF):\n");
  mu3_io_led_set_colors(0xFF, NULL);
  printf("Called with invalid board ID - should show error message\n");
}

void test_raw_hid_write() {
  print_separator("Raw HID Write Test");
  
  printf("Testing raw HID data write...\n");
  uint8_t dat[64] = {0};
  dat[0] = 0xaa;  // reportID
  dat[1] = 0x01;  // symbol
  dat[2] = 0xe1;  // command (SP_INPUT_GET)
  dat[3] = 0x00;  // padding
  
  HRESULT result = hid_write_data((const char*)dat, sizeof(dat));
  printf("hid_write_data Result: 0x%08X\n", result);
  if (SUCCEEDED(result)) {
    printf("Raw HID write: SUCCESS\n");
  } else {
    printf("Raw HID write: FAILED\n");
  }
}

int main() {
  printf("========================================\n");
  printf("      SimGEKI mu3io Test Program\n");
  printf("========================================\n");
  
  // Test 1: API Version
  test_api_version();
  
  // Test 2: HID Path Detection  
  test_hid_path();
  
  // Test 3: Initialization
  test_initialization();
  
  // Test 4: Raw HID Write
  test_raw_hid_write();
  
  // Test 5: LED Control
  test_led_control();
  
  // Test 6: Input Polling (main test)
  test_input_polling();
  
  print_separator("Test Complete");
  printf("All tests completed. Press any key to continue with infinite polling...\n");
  getchar();
  
  // Infinite polling for manual testing
  print_separator("Infinite Polling Mode");
  printf("Entering infinite polling mode. Press Ctrl+C to exit.\n\n");
  
  int poll_count = 0;
  while (1) {
    mu3_io_poll();
    
    if (poll_count % 20 == 0) {  // Print status every 20 polls (about 1 second)
      uint8_t opbtns = 0;
      uint8_t left_btns = 0, right_btns = 0;
      int16_t lever_pos = 0;
      
      mu3_io_get_opbtns(&opbtns);
      mu3_io_get_gamebtns(&left_btns, &right_btns);
      mu3_io_get_lever(&lever_pos);
      
      printf("[%d] OP:0x%02X L:0x%02X R:0x%02X Lever:0x%04X\n", 
             poll_count/20, opbtns, left_btns, right_btns, (uint16_t)lever_pos);
    }
    
    poll_count++;
    Sleep(50);  // 50ms polling interval
  }
  
  return 0;
}
