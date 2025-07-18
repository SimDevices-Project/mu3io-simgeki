# mu3io-simgeki

A simulator IO library for MU3 (Ongeki) arcade games that provides HID device communication for custom controllers.

## Features

- Cross-platform Windows DLL compilation
- HID device communication via USB
- Support for game buttons, operator buttons, and lever input
- LED control for RGB lighting effects
- Comprehensive build system with Makefile

## Building

### Prerequisites

For cross-compilation to Windows on Linux:
```bash
sudo apt-get update
sudo apt-get install -y gcc mingw-w64 build-essential
```

### Using Makefile (Recommended)

Build all targets:
```bash
make all
```

Build only the DLL:
```bash
make dll
```

Build only the test executable:
```bash
make test
```

Build DLL with explicit .def file:
```bash
make dll-def
```

Check DLL exports:
```bash
make check
```

Clean build artifacts:
```bash
make clean
```

### Using Batch Files (Windows)

For Windows development:
```cmd
build.bat       # Build DLL only
buildtest.bat   # Build test executable
```

## API Functions

The library exports the following functions for MU3 compatibility:

- `mu3_io_get_api_version()` - Get API version (0x0101)
- `mu3_io_init()` - Initialize the IO system
- `mu3_io_poll()` - Poll for input updates
- `mu3_io_get_opbtns()` - Get operator button states
- `mu3_io_get_gamebtns()` - Get game button states
- `mu3_io_get_lever()` - Get lever position
- `mu3_io_led_init()` - Initialize LED system
- `mu3_io_led_set_colors()` - Set LED colors

## Hardware Support

This library is designed to work with HID devices matching:
- VID: 0x0CA3
- PID: 0x0021  
- MI: 0x05

## Configuration

The HID configuration supports:
- Button input mapping for MU3 game controls
- RGB LED control for visual feedback
- Lever/roller position reporting
- Special commands for PC DLL communication

## Development

### File Structure

- `mu3io.c/.h` - Main library implementation
- `hid.c/.h` - HID device communication
- `test.c` - Test program for verification
- `Makefile` - Cross-platform build system
- `.github/workflows/build.yml` - CI/CD pipeline

### Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Run tests: `make all && make check`
5. Submit a pull request

## License

This project is provided as-is for educational and development purposes.