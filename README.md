# hpm_dfu_boot

A **USB DFU bootloader (DFU-only)** for HPMicro RISC-V MCUs. Built on the HPM SDK and CherryUSB's USB DFU class (DfuSe protocol), it lets you upgrade device firmware over USB without a debugger.

> 中文说明见 [README_zh.md](./README_zh.md)。Build instructions: [README_build.md](./README_build.md) (English) / [README_build_zh.md](./README_build_zh.md) (中文).

---

## What This Project Does

This project is a second-stage bootloader resident in the first 128K of on-chip Flash. On power-on it decides whether to launch the user application or enter DFU download mode.

### Boot Sequence

After power-on / reset, `main()` checks the following conditions in order (see `hpm_dfu_trigger.c`):

1. **Boot pin**: If a boot pin is configured and active, the device stays in the bootloader (and clears any stale DFU trigger so the next reset boots the APP normally).
2. **Retention-register trigger (BGPR / PDGO)**: An application can request DFU mode on the next boot by writing a magic value to a retention register and re-entering the ROM boot flow via the `run_bootloader` API (`API_BOOT_SRC_PRIMARY`, back to the DFU bootloader at `0x80000000`) instead of a full software reset (`hpm_reboot_to_boot()`). A software reset is kept only as a fallback if `run_bootloader` unexpectedly returns.
3. **APP validity**: Checks the 4-byte DFU signature (`BOARD_DFU_SIGNATURE`) at the APP start address (`USBD_DFU_APP_DEFAULT_ADD = 0x80020000`). If valid, it disables interrupts, invalidates the cache, and jumps to the application.

If none of the above is satisfied (no valid APP or an explicit DFU request), the bootloader initializes USB0 and enters **DFU mode**, waiting for a host DFU tool (e.g. `dfu-util`, DfuSe tools) to download firmware.

### Key Features

- Based on CherryUSB's DFU class (`usbd_dfu`), using the SDK's DfuSe path (`hpm_dfu_port.c` handles flash erase/write callbacks).
- Descriptors are defined in `dfu_desc.c`; the DfuSe memory layout string (`@Internal Flash /...`) is generated at runtime from the Flash layout.
- Supports multiple HPM-series boards (see `boards/` and `CMakePresets.json`).
- In Release builds, `BOOT_PRINTF` and the UART console can be disabled to reduce size.

### Memory Layout

| Item | Value |
| --- | --- |
| Bootloader Flash size | 128K (`_flash_size=128K`) |
| Non-cacheable region | 256K (`_noncacheable_size=256K`) |
| APP default start address | `0x80020000` |
| APP signature | 4-byte `BOARD_DFU_SIGNATURE` at the APP start address |

---

## Directory Structure

```
hpm_dfu_boot/
├── CMakeLists.txt          # Build script (depends on HPM SDK + CherryUSB)
├── CMakePresets.json       # Release/Debug presets per board
├── boards/                 # Per-board configuration (BOARD_SEARCH_PATH)
├── src/
│   ├── main.c              # Boot entry and startup flow
│   ├── hpm_dfu_trigger.c   # Trigger detection, jump, reboot-to-DFU
│   ├── dfu_desc.c          # USB DFU descriptors and initialization
│   └── boot_port_board_hpm.c # Board init (clock / USB / boot pin)
├── README.md               # Project overview (English, this file)
├── README_zh.md            # Project overview (中文)
├── README_build.md         # Build guide (English)
└── README_build_zh.md      # Build guide (中文)
```

---

## How to Build

See **[README_build.md](./README_build.md)** (English) / **[README_build_zh.md](./README_build_zh.md)** (中文) for details.

In short: prepare the HPM SDK and the RISC-V toolchain, set the environment variables, then use a CMake preset:

```bash
cmake --preset hpm5301evklite-release
cmake --build --preset hpm5301evklite-release
```

Artifacts are located at `build/<preset>/output/hpm-dfu-boot.elf` and `hpm-dfu-boot.hex`.

---

## Typical Usage

1. **First flash**: Write `hpm-dfu-boot.hex` to the start of Flash using a debugger / flasher.
2. **Subsequent upgrades**: Hold the boot pin at power-on (or have the APP call `hpm_reboot_to_boot()`) to enter DFU mode, then download new APP firmware with a DFU tool.
3. **APP launch**: If a valid signed APP exists in Flash and no DFU trigger is set, the bootloader automatically jumps to it.
