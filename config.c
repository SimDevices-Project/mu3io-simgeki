#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <windows.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/dprintf.h"

#include "config.h"
#include "mu3io.h"

MU3IO_CONFIG cfg = {
  .vid_num = {'0', 'C', 'A', '3', '\0'},
  .pid_num = {'0', '0', '2', '1', '\0'},
  .mi_num = {'0', '5', '\0'},

    .keyboard_enabled = 0,

    .test_keycode = 0x70,     // F1
    .service_keycode = 0x71,  // F2
    .coin_keycode = 0x72,     // F3

    .gamebtn_L1_keycode = 0,
    .gamebtn_L2_keycode = 0,
    .gamebtn_L3_keycode = 0,
    .gamebtn_Lside_keycode = 0,
    .gamebtn_Lmenu_keycode = 0,
    .gamebtn_R1_keycode = 0,
    .gamebtn_R2_keycode = 0,
    .gamebtn_R3_keycode = 0,
    .gamebtn_Rside_keycode = 0,
    .gamebtn_Rmenu_keycode = 0,
};

static void trim_whitespace(char* str) {
  if (str == NULL) {
    return;
  }

  char* start = str;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }

  size_t len = strlen(str);
  while (len > 0 && isspace((unsigned char)str[len - 1])) {
    len--;
  }
  str[len] = '\0';
}

static void strip_comment_and_trim(char* str) {
  if (str == NULL) {
    return;
  }

  char* semi = strchr(str, ';');
  if (semi != NULL) {
    *semi = '\0';
  }
  trim_whitespace(str);
}

static bool build_ini_path(char* path, size_t path_size) {
  if (path == NULL || path_size == 0) {
    return false;
  }

  HMODULE hModule = NULL;
  if (!GetModuleHandleExA(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
          (LPCSTR)&config_load_from_ini, &hModule)) {
    return false;
  }

  DWORD len = GetModuleFileNameA(hModule, path, (DWORD)path_size);
  if (len == 0 || len >= path_size) {
    return false;
  }

  char* last_slash = strrchr(path, '\\');
  if (last_slash == NULL) {
    last_slash = strrchr(path, '/');
  }
  if (last_slash == NULL) {
    return false;
  }

  *(last_slash + 1) = '\0';
  if (strnlen(path, path_size) + strlen("simgeki_io.ini") + 1 > path_size) {
    return false;
  }

  strcat_s(path, path_size, "simgeki_io.ini");
  return true;
}

static void apply_hex_field(const char* value,
                            char* target,
                            size_t length,
                            const char* label) {
  if (value == NULL || target == NULL || label == NULL) {
    return;
  }

  if (length == 0) {
    return;
  }

  const size_t max_digits = length - 1;  // leave room for null terminator
  const char* ptr = value;
  if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
    ptr += 2;
  }

  size_t digits = strnlen(ptr, length);
  if (digits < max_digits) {
    dprintf("SimGEKI: %s in ini is too short, keep default.\n", label);
    return;
  }

  for (size_t i = 0; i < max_digits; i++) {
    target[i] = (char)toupper((unsigned char)ptr[i]);
  }
  target[max_digits] = '\0';
}

static bool read_ini_hex_field(const char* section,
                               const char* key,
                               const char* ini_path,
                               char* target,
                               size_t length,
                               const char* label) {
  if (ini_path == NULL || target == NULL || length == 0) {
    return false;
  }

  char buf[64];
  DWORD len =
      GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), ini_path);
  if (len == 0 || len >= sizeof(buf)) {
    return false;
  }

  strip_comment_and_trim(buf);
  apply_hex_field(buf, target, length, label);
  return true;
}

static bool read_ini_uint8(const char* section,
                           const char* key,
                           const char* ini_path,
                           uint8_t* out) {
  if (ini_path == NULL || out == NULL) {
    return false;
  }

  // First try the built-in integer parser for readability
  const UINT sentinel = (UINT)(-1);
  UINT val = GetPrivateProfileIntA(section, key, sentinel, ini_path);
  if (val != sentinel && val <= 0xFF) {
    *out = (uint8_t)val;
    return true;
  }

  // Fallback to manual parse (allows hex like 0x70)
  char buf[32];
  DWORD len = GetPrivateProfileStringA(section, key, "", buf, sizeof(buf),
                                       ini_path);
  if (len == 0 || len >= sizeof(buf)) {
    return false;
  }
  strip_comment_and_trim(buf);

  char* endptr = NULL;
  unsigned long parsed = strtoul(buf, &endptr, 0);
  if (endptr == buf || parsed > 0xFF) {
    return false;
  }

  *out = (uint8_t)parsed;
  return true;
}

void config_load_from_ini(void) {
  char ini_path[MAX_PATH] = {0};
  if (!build_ini_path(ini_path, sizeof(ini_path))) {
    dprintf("SimGEKI: Failed to resolve ini path, skipping overrides.\n");
    return;
  }

  DWORD attrs = GetFileAttributesA(ini_path);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    dprintf("SimGEKI: Config ini not found at %s, using defaults.\n",
            ini_path);
    return;
  }

  dprintf("SimGEKI: Loading config from %s\n", ini_path);

  read_ini_hex_field("input", "VID", ini_path, cfg.vid_num,
                     sizeof(cfg.vid_num), "VID");
  read_ini_hex_field("input", "PID", ini_path, cfg.pid_num,
                     sizeof(cfg.pid_num), "PID");
  read_ini_hex_field("input", "MI", ini_path, cfg.mi_num,
                     sizeof(cfg.mi_num), "MI");

  read_ini_uint8("input", "keyboard", ini_path, &cfg.keyboard_enabled);

  if(cfg.keyboard_enabled != 0) {
    dprintf("SimGEKI: Keyboard input enabled.\n");
  }

  read_ini_uint8("input", "test", ini_path, &cfg.test_keycode);
  read_ini_uint8("input", "service", ini_path, &cfg.service_keycode);
  read_ini_uint8("input", "coin", ini_path, &cfg.coin_keycode);

  read_ini_uint8("input", "left1", ini_path, &cfg.gamebtn_L1_keycode);
  read_ini_uint8("input", "left2", ini_path, &cfg.gamebtn_L2_keycode);
  read_ini_uint8("input", "left3", ini_path, &cfg.gamebtn_L3_keycode);
  read_ini_uint8("input", "leftSide", ini_path, &cfg.gamebtn_Lside_keycode);
  read_ini_uint8("input", "leftMenu", ini_path, &cfg.gamebtn_Lmenu_keycode);

  read_ini_uint8("input", "right1", ini_path, &cfg.gamebtn_R1_keycode);
  read_ini_uint8("input", "right2", ini_path, &cfg.gamebtn_R2_keycode);
  read_ini_uint8("input", "right3", ini_path, &cfg.gamebtn_R3_keycode);
  read_ini_uint8("input", "rightSide", ini_path,
                 &cfg.gamebtn_Rside_keycode);
  read_ini_uint8("input", "rightMenu", ini_path,
                 &cfg.gamebtn_Rmenu_keycode);
}
