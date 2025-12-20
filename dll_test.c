#include <stdio.h>
#include <stdint.h>
#include <windows.h>

// Function pointer types for the DLL functions
typedef uint16_t (*mu3_io_get_api_version_func)(void);
typedef HRESULT (*mu3_io_init_func)(void);
typedef HRESULT (*mu3_io_poll_func)(void);
typedef void (*mu3_io_get_opbtns_func)(uint8_t* opbtn);
typedef void (*mu3_io_get_gamebtns_func)(uint8_t* left, uint8_t* right);
typedef void (*mu3_io_get_lever_func)(int16_t* pos);
typedef HRESULT (*mu3_io_led_init_func)(void);
typedef void (*mu3_io_led_set_colors_func)(uint8_t board, uint8_t* rgb);

int main() {
    printf("Testing mu3io DLL loading and basic functionality...\n");
    
    // Try to load the stub DLL first (no hardware required)
    HMODULE hDLL = LoadLibrary("build/mu3io_stub.dll");
    if (!hDLL) {
        printf("Failed to load mu3io_stub.dll, error: %lu\n", GetLastError());
        // Try the regular DLL
        hDLL = LoadLibrary("build/mu3io.dll");
        if (!hDLL) {
            printf("Failed to load mu3io.dll, error: %lu\n", GetLastError());
            return 1;
        }
        printf("Loaded mu3io.dll successfully\n");
    } else {
        printf("Loaded mu3io_stub.dll successfully\n");
    }
    
    // Get function pointers
    mu3_io_get_api_version_func get_api_version = 
        (mu3_io_get_api_version_func)GetProcAddress(hDLL, "mu3_io_get_api_version");
    mu3_io_init_func init = 
        (mu3_io_init_func)GetProcAddress(hDLL, "mu3_io_init");
    mu3_io_poll_func poll = 
        (mu3_io_poll_func)GetProcAddress(hDLL, "mu3_io_poll");
    mu3_io_get_opbtns_func get_opbtns = 
        (mu3_io_get_opbtns_func)GetProcAddress(hDLL, "mu3_io_get_opbtns");
    mu3_io_get_gamebtns_func get_gamebtns = 
        (mu3_io_get_gamebtns_func)GetProcAddress(hDLL, "mu3_io_get_gamebtns");
    mu3_io_get_lever_func get_lever = 
        (mu3_io_get_lever_func)GetProcAddress(hDLL, "mu3_io_get_lever");
    mu3_io_led_init_func led_init = 
        (mu3_io_led_init_func)GetProcAddress(hDLL, "mu3_io_led_init");
    mu3_io_led_set_colors_func led_set_colors = 
        (mu3_io_led_set_colors_func)GetProcAddress(hDLL, "mu3_io_led_set_colors");
    
    // Check if all functions were found
    if (!get_api_version || !init || !poll || !get_opbtns || !get_gamebtns || 
        !get_lever || !led_init || !led_set_colors) {
        printf("Failed to get all function pointers\n");
        FreeLibrary(hDLL);
        return 1;
    }
    
    printf("All function pointers obtained successfully\n");
    
    // Test API version
    uint16_t version = get_api_version();
    printf("API Version: 0x%04X\n", version);
    
    // Test initialization
    HRESULT result = init();
    printf("Initialization result: 0x%08lX (%s)\n", result, SUCCEEDED(result) ? "SUCCESS" : "FAILED");
    
    if (SUCCEEDED(result)) {
        // Test LED initialization
        result = led_init();
        printf("LED initialization result: 0x%08lX (%s)\n", result, SUCCEEDED(result) ? "SUCCESS" : "FAILED");
        
        // Test polling and input reading
        printf("Testing polling and input reading...\n");
        for (int i = 0; i < 10; i++) {
            poll();
            
            uint8_t opbtn = 0;
            uint8_t left_btn = 0, right_btn = 0;
            int16_t lever = 0;
            
            get_opbtns(&opbtn);
            get_gamebtns(&left_btn, &right_btn);
            get_lever(&lever);
            
            printf("Poll %d: OP=0x%02X, L=0x%02X, R=0x%02X, Lever=%d\n", 
                   i, opbtn, left_btn, right_btn, lever);
            
            Sleep(100);  // 100ms delay
        }
        
        // Test LED colors
        uint8_t test_colors[18] = {
            255, 0, 0,    // Red
            0, 255, 0,    // Green  
            0, 0, 255,    // Blue
            255, 255, 0,  // Yellow
            255, 0, 255,  // Magenta
            0, 255, 255   // Cyan
        };
        
        printf("Testing LED colors...\n");
        led_set_colors(0, test_colors);  // Board 0
        led_set_colors(1, test_colors);  // Board 1
        
        printf("DLL functionality test completed successfully!\n");
    } else {
        printf("Initialization failed, skipping further tests\n");
    }
    
    // Cleanup
    FreeLibrary(hDLL);
    printf("DLL unloaded successfully\n");
    
    return SUCCEEDED(result) ? 0 : 1;
}