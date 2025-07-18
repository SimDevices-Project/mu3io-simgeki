#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#ifndef MU3IO_EXPORTS
#define MU3IO_EXPORTS
#endif

#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#include "mu3io.h"

// Simple stub implementation for testing DLL loading and basic functionality
static uint8_t mu3_opbtn = 0;
static uint8_t mu3_left_btn = 0;
static uint8_t mu3_right_btn = 0;
static int16_t mu3_lever_pos = 0;
static bool initialized = false;

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            printf("SimGEKI: DLL_PROCESS_ATTACH\n");
            break;
        case DLL_PROCESS_DETACH:
            printf("SimGEKI: DLL_PROCESS_DETACH\n");
            break;
        case DLL_THREAD_ATTACH:
            printf("SimGEKI: DLL_THREAD_ATTACH\n");
            break;
        case DLL_THREAD_DETACH:
            printf("SimGEKI: DLL_THREAD_DETACH\n");
            break;
    }
    return TRUE;
}

uint16_t mu3_io_get_api_version(void) {
    printf("SimGEKI: mu3_io_get_api_version called\n");
    return 0x0101;
}

HRESULT mu3_io_init(void) {
    printf("SimGEKI: mu3_io_init called\n");
    
    if (initialized) {
        printf("SimGEKI: Already initialized\n");
        return S_OK;
    }
    
    printf("SimGEKI: --- Begin configuration ---\n");
    printf("SimGEKI: Stub IO init (no hardware required)\n");
    
    // Simulate successful initialization without hardware
    initialized = true;
    
    printf("SimGEKI: Initialization successful\n");
    printf("SimGEKI: ---  End  configuration ---\n");
    return S_OK;
}

HRESULT mu3_io_poll(void) {
    // Stub implementation - could simulate some test inputs
    static int counter = 0;
    counter++;
    
    // Simulate some button presses every 100 polls for testing
    if (counter % 200 == 0) {
        mu3_opbtn ^= MU3_IO_OPBTN_TEST;
    }
    if (counter % 150 == 0) {
        mu3_left_btn ^= MU3_IO_GAMEBTN_1;
    }
    
    // Simulate lever movement
    mu3_lever_pos = (int16_t)(0x3000 * sin(counter * 0.1));
    
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
    printf("SimGEKI: mu3_io_led_init called\n");
    return S_OK;
}

void mu3_io_led_set_colors(uint8_t board, uint8_t* rgb) {
    // Stub implementation - just log the call
    printf("SimGEKI: LED set colors called for board %d\n", board);
    (void)rgb;  // Unused parameter
}