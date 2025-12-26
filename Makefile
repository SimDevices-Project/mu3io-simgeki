# Makefile for mu3io-simgeki
# Cross-platform build system for Windows DLL

# Detect the host system
UNAME_S := $(shell uname -s)

# Default compiler settings for cross-compilation to Windows
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
LDFLAGS = -lsetupapi

# Directories
SRCDIR = .
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

# Source files
SOURCES = mu3io.c config.c hid.c util/dprintf.c
HEADERS = mu3io.h config.h hid.h util/dprintf.h
TEST_SOURCES = test.c

# Object files
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:%.c=$(OBJDIR)/%.o)

# Output files
DLL_TARGET = $(BUILDDIR)/simgeki_io.dll
TEST_TARGET = $(BUILDDIR)/test.exe
DEF_FILE = $(BUILDDIR)/simgeki_io.def

# Phony targets
.PHONY: all clean dll test install check help

# Default target
all: dll test

# Create directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/util

# Object file compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HEADERS) | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DMU3IO_EXPORTS -c $< -o $@

# DLL target
dll: $(DLL_TARGET)

$(DLL_TARGET): $(OBJECTS) | $(BUILDDIR)
	$(CC) -shared -o $@ $(OBJECTS) $(LDFLAGS) -DMU3IO_EXPORTS
	@echo "Built DLL: $@"

# Test executable target
test: $(TEST_TARGET)

$(TEST_TARGET): $(OBJECTS) $(TEST_OBJECTS) | $(BUILDDIR)
	$(CC) -o $@ $(OBJECTS) $(TEST_OBJECTS) $(LDFLAGS)
	@echo "Built test executable: $@"

# Generate .def file for explicit exports
$(DEF_FILE): | $(BUILDDIR)
	@echo "EXPORTS" > $@
	@echo "mu3_io_get_api_version" >> $@
	@echo "mu3_io_init" >> $@
	@echo "mu3_io_poll" >> $@
	@echo "mu3_io_get_opbtns" >> $@
	@echo "mu3_io_get_gamebtns" >> $@
	@echo "mu3_io_get_lever" >> $@
	@echo "mu3_io_led_init" >> $@
	@echo "mu3_io_led_set_colors" >> $@
	@echo "Generated .def file: $@"

# DLL with explicit .def file
dll-def: $(DLL_TARGET)-def

$(DLL_TARGET)-def: $(OBJECTS) $(DEF_FILE) | $(BUILDDIR)
	$(CC) -shared -o $(BUILDDIR)/simgeki_io-def.dll $(OBJECTS) $(LDFLAGS) -DMU3IO_EXPORTS $(DEF_FILE)
	@echo "Built DLL with .def file: $(BUILDDIR)/simgeki_io-def.dll"

# Install target (copy to common locations)
install: dll
	@echo "Installing simgeki_io.dll..."
	@if [ -d "/usr/local/lib" ]; then \
		cp $(DLL_TARGET) /usr/local/lib/; \
		echo "Installed to /usr/local/lib/"; \
	else \
		echo "No suitable installation directory found"; \
	fi

# Check exports in the built DLL
check: dll
	@echo "Checking DLL exports..."
	@if command -v x86_64-w64-mingw32-objdump >/dev/null 2>&1; then \
		x86_64-w64-mingw32-objdump -p $(DLL_TARGET) | grep -A 20 "Export Table" || true; \
	else \
		echo "objdump not available for checking exports"; \
	fi

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)
	@echo "Cleaned build directory"

# Help target
help:
	@echo "Available targets:"
	@echo "  all      - Build both DLL and test executable (default)"
	@echo "  dll      - Build the simgeki_io.dll"
	@echo "  test     - Build the test executable"
	@echo "  dll-def  - Build DLL with explicit .def file"
	@echo "  check    - Check DLL exports"
	@echo "  install  - Install the DLL"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Variables:"
	@echo "  CC       - Compiler (default: x86_64-w64-mingw32-gcc)"
	@echo "  CFLAGS   - Compiler flags"
	@echo "  LDFLAGS  - Linker flags"