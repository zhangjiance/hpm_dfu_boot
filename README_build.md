# hpm_dfu_boot Build Guide

This repository is a DFU bootloader (DFU-only) for HPMicro RISC-V MCUs, built on top of the HPM SDK and CherryUSB's USB DFU class. This document explains how to set up the environment and build the project using CMake presets.

> 中文版见 [README_build_zh.md](./README_build_zh.md)。Project overview: [README.md](./README.md) (English) / [README_zh.md](./README_zh.md) (中文).

---

## 1. Prerequisites (required)

Before building, you **must** do the following two things:

1. Download and prepare the HPM SDK source tree;
2. Download the RISC-V cross-compilation toolchain and set the relevant environment variables.

> You may choose a **different toolchain version** than the example below — the specific version is not mandatory. However, both the HPM SDK and the toolchain are indispensable, and the environment variables must point to them correctly.

### 1.1 Get the HPM SDK

Clone or download the HPM SDK, for example:

```bash
git clone https://github.com/hpmicro/hpm_sdk.git ~/Code/SDK/hpm_sdk
```

### 1.2 Get the RISC-V toolchain

This project uses the `riscv-none-elf` toolchain (specified by `CUSTOM_TARGET_TRIPLET` in `CMakePresets.json`). You can download a matching version from xpack, e.g.:

```bash
# Example: xpack riscv-none-elf-gcc 15.2.0-1
# Extract to ~/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1
```

The toolchain directory should contain `bin/riscv-none-elf-gcc` and the other executables.

---

## 2. Environment Variables

Add the following to `~/.bashrc` (or the config file for your shell), then `source ~/.bashrc` to apply:

```bash
# Add the toolchain bin to PATH (replace with your actual extracted directory)
export PATH="$HOME/.local/bin:$HOME/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1/bin:$PATH"

# Point to your local HPM SDK root
export HPM_SDK_BASE="$HOME/Code/SDK/hpm_sdk"

# Point to the toolchain root, used by HPM SDK to locate the compiler
export GNURISCV_TOOLCHAIN_PATH="$HOME/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1"
```

Meaning of each variable:

| Variable | Purpose |
| --- | --- |
| `PATH` | Makes `cmake` / `ninja` and the `riscv-none-elf-*` compilers directly callable. |
| `HPM_SDK_BASE` | The `CMakeLists.txt` locates the SDK via `find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})`. |
| `GNURISCV_TOOLCHAIN_PATH` | Used internally by HPM SDK to find the RISC-V toolchain. |

> You can also `source <hpm_sdk>/env.sh` to set `HPM_SDK_BASE` and `OPENOCD_SCRIPTS` automatically, but the toolchain-related variables still need to be set manually.

Verify the environment:

```bash
echo $HPM_SDK_BASE
echo $GNURISCV_TOOLCHAIN_PATH
which riscv-none-elf-gcc cmake ninja
```

---

## 3. Build with CMake Presets (recommended)

This project provides `CMakePresets.json`. Each board has two presets: `Release` and `Debug`, named `<board>-release` / `<board>-debug`.

### 3.1 Configure and build (example: HPM5301EVKLite)

```bash
# Configure
cmake --preset hpm5301evklite-release

# Build
cmake --build --preset hpm5301evklite-release
```

Or build directly with `ninja` and a job count:

```bash
ninja -C build/hpm5301evklite-release -j8
```

### 3.2 Supported Boards and Presets

`CMakePresets.json` contains the following buildable presets (the `BOARD` is shown in parentheses):

| Board | Release preset | Debug preset |
| --- | --- | --- |
| HPM5300EVK | `hpm5300evk-release` | `hpm5300evk-debug` |
| HPM5321 USB2CAN | `hpm5321_usb2can-release` | `hpm5321_usb2can-debug` |
| HSCanT | `hscant-release` | `hscant-debug` |
| HPM5301EVKLite | `hpm5301evklite-release` | `hpm5301evklite-debug` |
| HPM5E00EVK | `hpm5e00evk-release` | `hpm5e00evk-debug` |
| HPM6200EVK | `hpm6200evk-release` | `hpm6200evk-debug` |
| HPM6300EVK | `hpm6300evk-release` | `hpm6300evk-debug` |
| HPM6750EVK2 | `hpm6750evk2-release` | `hpm6750evk2-debug` |
| HPM6750EVKMini | `hpm6750evkmini-release` | `hpm6750evkmini-debug` |
| HPM6800EVK | `hpm6800evk-release` | `hpm6800evk-debug` |
| HPM6E00EVK | `hpm6e00evk-release` | `hpm6e00evk-debug` |
| HPM6E00 Full Port | `hpm6e00_full_port-release` | `hpm6e00_full_port-debug` |
| HPM6P00EVK | `hpm6p00evk-release` | `hpm6p00evk-debug` |
| HPM6P41DEV | `hpm6p41dev-release` | `hpm6p41dev-debug` |
| HSLink Lite | `hslinklite-release` | `hslinklite-debug` |
| HSLink Pro | `hslinkpro-release` | `hslinkpro-debug` |

> List all presets: `cmake --list-presets`.

---

## 4. Build Artifacts

After a successful build, the output is located at:

```
build/<preset>/output/hpm-dfu-boot.elf
build/<preset>/output/hpm-dfu-boot.hex
```

- `.elf`: executable image, for debugging / OpenOCD flashing;
- `.hex`: Intel HEX format, convenient for writing to Flash via DFU / flashing tools.

Key build configuration (see `CMakeLists.txt`):
- Architecture `rv32imac_zicsr_zifencei`, ABI `ilp32`;
- Flash size 128K (`_flash_size=128K`), non-cacheable region 256K (`_noncacheable_size=256K`);
- Release builds disable `BOOT_PRINTF` and the UART console (`-DNDEBUG -DCONFIG_NDEBUG_CONSOLE=1`) to reduce size.

---

## 5. Manual Build without Presets

If you want to specify parameters manually instead of using a preset, pass `BOARD` and the toolchain-related variables explicitly:

```bash
cmake -B build/manual \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBOARD=hpm5301evklite \
  -DHPM_BUILD_TYPE=flash_xip \
  -DCUSTOM_TARGET_TRIPLET=riscv-none-elf \
  -DCMAKE_TOOLCHAIN_FILE=$HPM_SDK_BASE/cmake/toolchain/riscv_none_elf_gcc.cmake

cmake --build build/manual -j8
```

Note: the `HPM_SDK_BASE` and `GNURISCV_TOOLCHAIN_PATH` environment variables from Section 2 are still required for a manual build.

---

## 6. FAQ

- **`HPM SDK not found` / `find_package(hpm-sdk) failed`**
  Check that `HPM_SDK_BASE` is exported and points to the correct SDK root.

- **`riscv-none-elf-gcc` not found**
  Confirm the toolchain `bin` directory is on `PATH` and `GNURISCV_TOOLCHAIN_PATH` is set correctly.

- **`BOARD is not set`**
  No preset was selected, or `-DBOARD=<board>` was not specified.
