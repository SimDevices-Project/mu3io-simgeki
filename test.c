#include "hid.h"
#include "mu3io.h"

#include <stdio.h>

int main() {
  uint16_t api_version = mu3_io_get_api_version();
  printf("API Version: %d\n", api_version);
  HRESULT result = mu3_io_led_init();
  printf("Result: %d\n", result);
  int16_t pos;
  mu3_io_get_lever(&pos);
  printf("Position: %d\n", pos);
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

  return 0;
}
