# CLion 使用说明

工程：STM32F103C8T6（LQFP48，64 KB Flash、20 KB RAM），STM32 HAL + FreeRTOS 10.3.1，CubeMX 原工程为 Keil MDK。系统时钟沿用原配置：8 MHz HSE × 9 = 72 MHz。

## 打开与编译

1. CLion 打开本目录 `E:\chatGPT\workplace\STM32_smart_door_CLion`，加载 CMakePresets.json 中的 Debug / Release。
2. 选择 Debug，构建 smart_door。不要直接运行 smart_door.elf，它是 MCU 固件。
3. 产物位于 build/Debug 或 build/Release：smart_door.elf、smart_door.hex、smart_door.bin、smart_door.map。

配置使用本机已有的 CLion CMake/Ninja、STM32CubeCLT ARM GCC 14.3.1，以及 xPack OpenOCD。换电脑后修改 CMakePresets.json、cmake/arm-gcc.cmake、CMakeLists.txt 和 .run/ST-Link Debug.run.xml 中的安装路径。CMake 必须使用 3.25 或以上。

## ST-Link 烧录与调试

- 板端连接：SWDIO → PA13，SWCLK → PA14，GND 共地，VTref 接板端 3.3 V（是否同时供电取决于调试器型号）。开发板正常供电，BOOT0=0。当前配置不要求 NRST，SWD 为 1 MHz。
- 单独烧录：在 CLion 的 CMake 目标中构建 `flash`，执行下载、校验、复位运行。
- 断点/单步：选择共享运行配置 `ST-Link Debug`，点击 Debug。配置使用 ARM GDB、localhost:3333、下载 ALWAYS、复位 INIT。可在 main 或任务入口设断点。
- 若运行配置未自动显示：新建 OpenOCD Download & Run；Target=smart_door、Debug 配置、Board config=openocd/stlink.cfg、OpenOCD 路径见 CMakeLists.txt；Debugger 选择 cmake/arm-gcc.cmake 所列工具链 bin/arm-none-eabi-gdb.exe。
- 不要在构建前启动持续等待的调试服务。新版 CLion 可选顶部 ST-LINK 调试档案、smart_door 运行目标，再点击 Debug；也可使用 ST-Link Debug（OpenOCD）配置。

## 变更边界

所有原文件按字节复制，包括 Core、BSP、Drivers、Middlewares、.ioc、Keil 工程和既有产物。没有重新生成 CubeMX 代码。verification/original-files-sha256.csv 是原文件与副本的 SHA-256 对照。

新增 CMake、链接脚本、OpenOCD、CLion 调试配置及验证资料。GCC 使用原包已有的官方 GCC 启动文件；原包仅带 RVDS FreeRTOS 移植层，因此 cmake/freertos_gcc 新增 STM32Cube_FW_F1_V1.8.6 提供的官方 GCC ARM_CM3 port.c/portmacro.h（同为 FreeRTOS 10.3.1；SDK tasks.c 与工程 tasks.c 的 SHA-256 一致）。原 RVDS 文件保留。

链接脚本沿用原 Keil 的 0x400 栈及 0x200 C 库堆预留，并保留 app.h 的 0x0800FC00 最后 1 KB 配置区，可用固件 Flash 为 63 KB。FreeRTOS 12 KB 堆配置保持原样。编译器切换后固件二进制与 Keil 产物不会逐字节一致。

## 已验证与待验证

- Debug / Release 编译链接成功，0 错误；各有 4 条 newlib/nosys 的 `_close/_lseek/_read/_write` 未实现警告。这些为裸机文件读写接口，现有应用使用 snprintf；没有增加串口重定向或隐藏警告。
- Debug：Flash 31488 B / 63 KB（48.81%）；RAM 含堆栈预留 18728 B / 20 KB（91.45%）。Release：Flash 27832 B（43.14%）；RAM 18712 B（91.37%）。RAM 余量不足 20%，应上板观察任务栈高水位与剩余 FreeRTOS 堆；本转换不修改原资源配置。
- 验证 ARM GDB 可读取 ELF、main 源码行号以及 SVC/PendSV/SysTick 符号；ELF 启动向量及装载地址正确，固件未覆盖门锁配置页。OpenOCD 配置解析成功。
- ST-Link 尚未连接，因此实际烧录、回读校验、复位运行、断点/单步、供电接线、UART、指纹/RC522/OLED/舵机等实机功能尚未验证，CLion UI 加载也未进行交互实测。编译成功不代表硬件验证完成。

编译日志与验证结果见 verification/。
