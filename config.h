#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  char vid_num[5];
  char pid_num[5];
  char mi_num[3];

  uint8_t keyboard_enabled;
  uint8_t test_keycode;
  uint8_t service_keycode;
  uint8_t coin_keycode;

  uint8_t gamebtn_L1_keycode;
  uint8_t gamebtn_L2_keycode;
  uint8_t gamebtn_L3_keycode;
  uint8_t gamebtn_Lside_keycode;
  uint8_t gamebtn_Lmenu_keycode;

  uint8_t gamebtn_R1_keycode;
  uint8_t gamebtn_R2_keycode;
  uint8_t gamebtn_R3_keycode;
  uint8_t gamebtn_Rside_keycode;
  uint8_t gamebtn_Rmenu_keycode;

} MU3IO_CONFIG;

extern MU3IO_CONFIG cfg;

void config_load_from_ini(void);

#ifdef __cplusplus
}
#endif
