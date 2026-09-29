# hpm_dfu_boot 编译指导

本仓库是基于 HPM SDK 的 DFU 引导程序（DFU-only bootloader），通过 CherryUSB 的 USB DFU 类实现固件下载。本文档说明如何配置环境并使用 CMake preset 完成编译。

> English version: [README_build.md](./README_build.md). 工程说明： [README.md](./README.md)（英文）/ [README_zh.md](./README_zh.md)（中文）。

---

## 1. 前置准备（必要步骤）

在编译之前，**必须**完成以下两件事：

1. 提前下载并准备好 HPM SDK 源码；
2. 下载 RISC-V 交叉编译工具链，并设置好相关环境变量。

> 工具链的**具体版本可以自行选择**，不强制与下文示例一致。但 HPM SDK 与工具链二者缺一不可，且环境变量需要正确指向它们。

### 1.1 获取 HPM SDK

克隆或下载 HPM SDK 到本地，例如：

```bash
git clone https://github.com/hpmicro/hpm_sdk.git ~/Code/SDK/hpm_sdk
```

### 1.2 获取 RISC-V 工具链

本工程使用 `riscv-none-elf` 工具链（由 `CMakePresets.json` 中的 `CUSTOM_TARGET_TRIPLET` 指定）。可以从 xpack 下载对应版本，例如：

```bash
# 示例：xpack riscv-none-elf-gcc 15.2.0-1
# 解压到 ~/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1
```

工具链目录结构应至少包含 `bin/riscv-none-elf-gcc` 等可执行文件。

---

## 2. 环境变量设置

将以下内容加入 `~/.bashrc`（或对应 shell 的配置文件），然后 `source ~/.bashrc` 使其生效：

```bash
# 将工具链 bin 加入 PATH（请替换为你实际解压的目录）
export PATH="$HOME/.local/bin:$HOME/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1/bin:$PATH"

# 指向你本地的 HPM SDK 根目录
export HPM_SDK_BASE="$HOME/Code/SDK/hpm_sdk"

# 指向工具链根目录，HPM SDK 在查找编译器时会用到
export GNURISCV_TOOLCHAIN_PATH="$HOME/Code/tools/xpack-riscv-none-elf-gcc-15.2.0-1"
```

各变量含义：

| 变量 | 作用 |
| --- | --- |
| `PATH` | 让 `cmake` / `ninja` 以及 `riscv-none-elf-*` 编译器可被直接调用。 |
| `HPM_SDK_BASE` | 工程的 `CMakeLists.txt` 通过 `find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})` 定位 SDK。 |
| `GNURISCV_TOOLCHAIN_PATH` | HPM SDK 内部据此寻找 RISC-V 工具链。 |

> 也可 `source <hpm_sdk>/env.sh` 自动设置 `HPM_SDK_BASE` 与 `OPENOCD_SCRIPTS`，但工具链相关变量仍需手动设置。

验证环境：

```bash
echo $HPM_SDK_BASE
echo $GNURISCV_TOOLCHAIN_PATH
which riscv-none-elf-gcc cmake ninja
```

---

## 3. 使用 CMake Preset 编译（推荐）

本工程提供 `CMakePresets.json`，每个 board 对应 `Release` 与 `Debug` 两套预设，命名规则为 `<board>-release` / `<board>-debug`。

### 3.1 配置并构建（以 HPM5301EVKLite 为例）

```bash
# 配置
cmake --preset hpm5301evklite-release

# 构建
cmake --build --preset hpm5301evklite-release
```

或使用 `ninja` 直接指定 jobs：

```bash
ninja -C build/hpm5301evklite-release -j8
```

### 3.2 支持的 Board 与 Preset

`CMakePresets.json` 中包含以下可构建预设（括号内为对应 `BOARD`）：

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

> 查看全部 preset：`cmake --list-presets`。

---

## 4. 编译产物

构建完成后，输出位于：

```
build/<preset>/output/hpm-dfu-boot.elf
build/<preset>/output/hpm-dfu-boot.hex
```

- `.elf`：可执行镜像，可用于调试 / OpenOCD 烧录；
- `.hex`：Intel HEX 格式，便于通过 DFU / 烧录工具写入 Flash。

工程配置要点（见 `CMakeLists.txt`）：
- 架构 `rv32imac_zicsr_zifencei`，ABI `ilp32`；
- Flash 大小 128K（`_flash_size=128K`），非缓存区 256K（`_noncacheable_size=256K`）；
- Release 构建会禁用 `BOOT_PRINTF` 与 UART 控制台（`-DNDEBUG -DCONFIG_NDEBUG_CONSOLE=1`）以减小体积。

---

## 5. 不使用 Preset 的手动编译

如果希望手动指定参数而不使用 preset，可显式传入 `BOARD` 及工具链相关变量：

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

注意：手动编译时同样需要第 2 节中的 `HPM_SDK_BASE` 与 `GNURISCV_TOOLCHAIN_PATH` 环境变量。

---

## 6. 常见问题

- **`HPM SDK not found` / `find_package(hpm-sdk) failed`**
  检查 `HPM_SDK_BASE` 是否已导出且指向正确的 SDK 根目录。

- **找不到 `riscv-none-elf-gcc`**
  确认工具链 `bin` 目录已加入 `PATH`，且 `GNURISCV_TOOLCHAIN_PATH` 设置正确。

- **提示 `BOARD is not set`**
  未选用任何 preset，或未通过 `-DBOARD=<board>` 指定目标板。
