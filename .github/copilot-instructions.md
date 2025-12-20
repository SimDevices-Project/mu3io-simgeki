# Copilot Instructions for mu3io-simgeki

## Project snapshot
- Windows-focused HID DLL for Sega Ongeki with cross-compilation from Linux via MinGW (`Makefile`).
- Primary sources: `mu3io.c` (exported API + polling), `hid.c` (device discovery fallback to registry), `util/dprintf.*` (debug logging).
- Hardware expectation: VID `0x0CA3`, PID `0x0021`, MI `0x05`; adjust constants in `mu3io.h` only if supporting new boards.

## Architecture & data flow
- `mu3_io_*` exports update global button/lever state from 64-byte HID reports parsed in `hid_on_data`; keep `HidconfigData` packing in `mu3io.h` in sync with firmware revisions.
- Polling uses overlapped I/O: `mu3_io_poll` drains all completed reads via `GetOverlappedResult`, only processes the latest packet, then re-arms `ReadFile`; always `ResetEvent` before issuing new async IO.
- The DLL never fails init: `mu3_io_init` logs and sets `usb_init_attempted`, while reconnection is handled lazily in `mu3_io_poll` (check `USB_RECONNECTION_BEHAVIOR.md` before touching timeouts or counters).
- `poll_state` gates the `SP_INPUT_GET_START` command; if you change startup messaging, ensure we still send that packet when idle.
- LED writes reuse the same HID channel via `hid_write_data`; board `0x00` expects a 183-byte RGB map (indices mirrored between left/right segments) and board `0x01` packs 6 tri-color button LEDs into on/off bits.

## Build & test workflow
- Linux cross-build: `make all` (DLL + `test.exe`), `make dll` (DLL only), `make check` (objdump export audit), `make dll-def` (emits `build/mu3io.def` for manual exports).
- Windows local build: `build.bat` (DLL) and `buildtest.bat` (test exe) rely on `gcc -m64`; ensure `-lsetupapi` is linked.
- Full regression script `test_all.sh` expects `x86_64-w64-mingw32-gcc` plus objdump; it also references `build/mu3io_stub.dll`—create or stub this artifact if you extend the script.
- CI (`.github/workflows/build.yml`) runs on `ubuntu-latest`; keep new dependencies installable via `apt` before invoking `make`.
- Manual sanity: run `build/test.exe` or `build/dll_test.exe` on Windows hardware to watch live button/LED logs.

## Conventions & pitfalls
- Logging goes through `dprintf`; keep the existing DEBUG macro guards intact and prefer `dprintf` over `printf` inside DLL code so logs route to the debugger.
- HID discovery first uses `SetupDiGetDeviceRegistryProperty`; the fallback `GetDeviceInterfaceFromRegistry` normalizes hardware IDs (uppercase, swaps path backslashes for hash symbols). Preserve this when editing enumeration logic.
- Treat `is_usb_disconnection_error` as the canonical list of transient errors; extend it there instead of sprinkling custom checks.
- API calls may originate from multiple threads; callers are expected to synchronize, but avoid introducing additional static mutable state without guards (see note in `USB_RECONNECTION_BEHAVIOR.md`).
- If you add exports, declare them in `mu3io.h`, implement in `mu3io.c`, and update the `.def` generation target so the symbol is visible to games.
- Keep report buffers at 64 bytes and reuse `REPORT_SIZE`; firmware assumes fixed-length transfers.

## File cues
- `mu3io.c` lines ~1-220 cover USB setup + packet parsing, ~220-360 handle LED plumbing; reference these sections when updating input or lighting logic.
- `hid.c` contains Windows API glue; unit-testable pieces are `GetHidPathByVidPidMi` and `GetDeviceInterfaceFromRegistry` by mocking registry paths.
- `USB_RECONNECTION_BEHAVIOR.md` documents why init never fails and how reconnection pacing works—align code comments with this doc when altering timing.
- `test.c` exercises every exported function (polling, LED sequences, raw writes) and is the quickest loop for hardware debugging.
- `.github/workflows/build.yml` mirrors the `make` targets; if you add new artifacts, upload them there so CI users can download them.

## When in doubt
- Favor small, isolated changes with logging, run `make all && make check`, and mention whether hardware testing was possible when opening PRs.
