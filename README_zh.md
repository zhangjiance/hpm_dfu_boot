# hpm_dfu_boot

基于 HPMicro RISC-V MCU 的 **USB DFU 引导程序（DFU-only Bootloader）**。它利用 HPM SDK 与 CherryUSB 的 USB DFU 类（DfuSe 协议），让用户无需调试器即可通过 USB 对设备固件进行升级。

> English overview: [README.md](./README.md). 编译指导： [README_build.md](./README_build.md)（英文）/ [README_build_zh.md](./README_build_zh.md)（中文）。

---

## 工程作用

本工程是一个驻留在芯片 Flash 前 128K 区域的二级引导程序（bootloader），设备上电后由它决定是启动用户应用程序，还是进入 DFU 下载模式。

### 启动流程

上电 / 复位后，`main()` 会依次检查以下条件（`hpm_dfu_trigger.c`）：

1. **Boot 引脚**：若配置了该引脚且处于激活电平，则停留在 bootloader（同时清除可能存在的旧 DFU 触发标记，确保下次复位能正常启动 APP）。
2. **保留寄存器触发（BGPR / PDGO）**：应用程序可通过写入保留寄存器，并调用 ROM 的 `run_bootloader` API（`API_BOOT_SRC_PRIMARY`，即回到位于 `0x80000000` 的 DFU bootloader）重新进入引导流程，请求进入 DFU 模式（`hpm_reboot_to_boot()`）。相比整片软件复位，该方式直接走 ROM 引导流程；仅当 `run_bootloader` 意外返回时才回退为软件复位。
3. **APP 有效性**：检查 APP 起始地址（`USBD_DFU_APP_DEFAULT_ADD = 0x80020000`）处的 4 字节 DFU 签名（`BOARD_DFU_SIGNATURE`）。若签名有效，则关闭中断、失效缓存并跳转到应用程序。

如果以上均未满足（无有效 APP 或显式请求），bootloader 初始化 USB0 并进入 **DFU 模式**，等待主机通过 DFU 工具（如 `dfu-util`、DfuSe 工具）下载固件。

### 关键特性

- 基于 **CherryUSB** 的 DFU 类（`usbd_dfu`），走 SDK 的 DfuSe 路径（`hpm_dfu_port.c` 处理 Flash 擦写回调）。
- 描述符在 `dfu_desc.c` 中定义，运行时根据 Flash 布局生成 DfuSe 内存描述字符串（`@Internal Flash /...`）。
- 支持多款 HPM 系列开发板（见 `boards/` 与 `CMakePresets.json`）。
- 编译期可在 Release 下关闭 `BOOT_PRINTF` 与 UART 控制台以减小体积。

### 内存布局

| 项目 | 值 |
| --- | --- |
| Bootloader Flash 大小 | 128K（`_flash_size=128K`） |
| 非缓存区 | 256K（`_noncacheable_size=256K`） |
| APP 默认起始地址 | `0x80020000` |
| APP 签名 | 位于 APP 起始地址的 4 字节 `BOARD_DFU_SIGNATURE` |

---

## 目录结构

```
hpm_dfu_boot/
├── CMakeLists.txt          # 工程构建脚本（依赖 HPM SDK + CherryUSB）
├── CMakePresets.json       # 各 board 的 Release/Debug 预设
├── boards/                 # 各目标板的板级配置（BOARD_SEARCH_PATH）
├── src/
│   ├── main.c              # 引导入口与启动流程
│   ├── hpm_dfu_trigger.c   # 触发检测、跳转、复位到 DFU
│   ├── dfu_desc.c          # USB DFU 描述符与初始化
│   └── boot_port_board_hpm.c # 板级初始化（时钟 / USB / Boot 引脚）
├── README.md               # 工程说明（英文）
├── README_zh.md            # 工程说明（中文，本文件）
├── README_build.md         # 编译指导（英文）
└── README_build_zh.md      # 编译指导（中文）
```

---

## 如何编译

详见 **[README_build.md](./README_build.md)**（英文）/ **[README_build_zh.md](./README_build_zh.md)**（中文）。

简而言之，需先准备 HPM SDK 与 RISC-V 工具链并设置环境变量，然后使用 CMake preset：

```bash
cmake --preset hpm5301evklite-release
cmake --build --preset hpm5301evklite-release
```

产物位于 `build/<preset>/output/hpm-dfu-boot.elf` 与 `hpm-dfu-boot.hex`。

---

## 典型使用场景

1. 首次烧录：通过调试器 / 烧录器将 `hpm-dfu-boot.hex` 写入 Flash 起始位置。
2. 后续升级：设备上电时按住 Boot 引脚（或 APP 调用 `hpm_reboot_to_boot()`），进入 DFU 模式后用 DFU 工具下载新的 APP 固件。
3. APP 启动：若 Flash 中已存在带有效签名的 APP 且未触发 DFU，bootloader 会自动跳转执行。
