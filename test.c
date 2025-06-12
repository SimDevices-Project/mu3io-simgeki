#include "hid.h"
#include "mu3io.h"

#include <stdio.h>

int main() {
  printf("SimGEKI: mu3io test program\n");
  uint16_t api_version = mu3_io_get_api_version();
  printf("API Version: %d\n", api_version);
  HRESULT result;
  char vid[9] = "VID_0CA3";
  char pid[9] = "PID_0021";
  char mi[6] = "MI_05";
  char path[1024];
  size_t path_size = 1024;
  printf("VID: %s\n", vid);
  printf("PID: %s\n", pid);
  printf("MI: %s\n", mi);
  result = GetHidPathByVidPidMi(vid, pid, mi, path, &path_size);
  printf("Result: %d\n", result);
  printf("Path: %s\n", path);
  result = mu3_io_init();
  printf("Initialization Result: %d\n", result);
  mu3_io_led_init();
  uint8_t dat[64] = {0xaa, 0x01, 0xe1, 0x00};
  result = hid_write_data((const char*)dat, sizeof(dat));
  printf("Write Data Result: %d\n", result);

  Sleep(5);
  mu3_io_poll();
  Sleep(5);
  mu3_io_poll();

  Sleep(INFINITE);
  return 0;
}
