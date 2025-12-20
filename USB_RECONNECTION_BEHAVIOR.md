# USB Reconnection Behavior

This document describes the USB reconnection logic implemented in mu3io.dll.

## Features

### 1. Graceful Initialization
- `mu3_io_init()` always succeeds, even if USB device is not connected
- USB initialization is separated from DLL initialization
- Error messages are logged but don't cause initialization failure

### 2. Automatic Connection
- When USB device is connected, it automatically initializes during the first poll
- No manual intervention required

### 3. Graceful Disconnection Handling
- When USB device disconnects unexpectedly, the DLL:
  - Detects disconnection during read/write operations
  - Cleans up USB resources safely
  - Logs disconnection event
  - Continues running without crashing

### 4. Automatic Reconnection
- When USB device reconnects after disconnection:
  - DLL automatically attempts reconnection during polling
  - Reconnection is attempted every 60 polls to avoid excessive retry attempts
  - Once reconnected, data flow resumes automatically

## Implementation Details

### State Tracking
Two boolean flags track USB connection state:
- `usb_connected`: True when USB device is successfully connected and initialized
- `usb_init_attempted`: True after first initialization attempt

### USB Cleanup Function
`usb_cleanup()` safely releases all USB resources:
- Closes write event handle
- Closes read event handle
- Closes HID device handle
- Resets connection state flags

### USB Initialization Function
`usb_init()` handles USB device connection:
- Calls `usb_cleanup()` first to ensure clean state
- Attempts to find USB device by VID/PID/MI
- Opens HID device with overlapped I/O
- Creates async read/write event handles
- Starts first async read operation
- Returns S_OK on success, S_FALSE if device not found

### Polling Behavior
`mu3_io_poll()` handles automatic reconnection:
- If USB not connected, attempts reconnection every 60 polls
- If USB connected, polls for data as normal
- Detects disconnection during read operations
- Handles disconnection errors gracefully

**Note on Thread Safety**: The `reconnect_counter` in `mu3_io_poll()` is a static variable. According to the MU3 IO API documentation, all API calls may originate from arbitrary threads after initialization. If multiple threads call `mu3_io_poll()` concurrently, proper external synchronization should be used by the caller to ensure thread-safe operation.

### Write Behavior
`hid_write_data()` handles disconnection during writes:
- Returns S_FALSE immediately if USB not connected (no error logged)
- Uses 1000ms timeout instead of INFINITE wait
- Detects disconnection errors during write operations
- Calls `usb_cleanup()` if disconnection detected

## Error Codes Handled

The following Windows error codes are treated as disconnection events:
- `ERROR_BAD_COMMAND`: Device not responding
- `ERROR_NOT_READY`: Device not ready
- `ERROR_DEVICE_NOT_CONNECTED`: Device physically disconnected
- `ERROR_GEN_FAILURE`: General device failure
- `ERROR_OPERATION_ABORTED`: Operation aborted (usually due to device removal)

## Testing the Feature

To test USB reconnection:

1. Start the application with USB device connected
   - Should connect and work normally

2. Start the application with USB device disconnected
   - Should start successfully with log: "USB device not connected, will retry during polling"
   - Application continues running

3. Disconnect USB device while application is running
   - Should detect disconnection and log: "USB device disconnected"
   - Application continues running with no data

4. Reconnect USB device while application is running
   - Should automatically reconnect and log: "USB device reconnected successfully"
   - Data flow resumes automatically

## Log Messages

Key log messages to watch for:

**Initial Connection:**
```
SimGEKI: Attempting to connect USB device...
SimGEKI: HID Path: \\?\HID#...
SimGEKI: HID device opened successfully.
SimGEKI: USB device initialized successfully.
```

**No Device at Startup:**
```
SimGEKI: Attempting to connect USB device...
SimGEKI: USB device not found.
SimGEKI: USB device not connected, will retry during polling.
```

**Disconnection:**
```
SimGEKI: ReadFile failed: <error code>
SimGEKI: USB device disconnected.
```

**Reconnection:**
```
SimGEKI: Attempting to connect USB device...
SimGEKI: HID Path: \\?\HID#...
SimGEKI: HID device opened successfully.
SimGEKI: USB device initialized successfully.
SimGEKI: USB device reconnected successfully.
```
