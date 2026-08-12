# 正点原子 STM32N6：使用 STM32CubeMX、CMake 与 VS Code 开发

本文面向第一次使用正点原子 STM32N647 开发板和官方例程的用户，介绍如何在 macOS 或 Windows 上完成以下工作：

- 使用 STM32CubeMX 打开例程的 `.ioc` 文件并生成 CMake 工程；
- 使用 `atk_stm32n6_cmake_prepare.py` 完成正点原子例程到 CMake 的适配；
- 在 VS Code 中获得代码补全、跳转、Debug 编译、运行和断点调试；
- 生成地址正确的 Release HEX/BIN；
- 使用 STM32CubeProgrammer 图形界面手动烧录 Release 固件。

> 本文对应 STM32N647X0H3Q、正点原子 STM32N647/DNN647 软件包，以及脚本 3.3.0 的工作方式。脚本文件可以重命名为 `atk_stm32n6_cmake_prepare.py`，不影响功能。

## 1. 工作流概览

```text
正点原子例程 .ioc
       │
       ▼
STM32CubeMX 生成 CMake
       │
       ▼
运行 atk_stm32n6_cmake_prepare.py
       │
       ├── 修复 CubeMX 的 IAC/GTZC 生成问题
       ├── 加入正点原子 BSP 源文件和头文件
       ├── 配置 Debug/Release 链接脚本
       ├── 配置 VS Code、clangd、构建任务和调试
       └── 配置 Release HEX/BIN 生成
       │
       ├── Debug → SRAM 0x34000400 → F5 调试
       │
       └── Release → NOR 0x70100400 → CubeProgrammer 烧录
```

Debug 与 Release 的用途不同：

| 配置 | 构建目录 | 用途 | 关键地址 |
| --- | --- | --- | --- |
| Debug | `Appli/build/Debug` | 通过 ST-LINK 加载到 SRAM，直接运行或断点调试 | `0x34000400` |
| Release | `Appli/build/Release` | 生成供外部 NOR Flash 烧录的固件 | `0x70100400` |
| Release 烧录文件 | `Appli/build/flash` | CubeProgrammer 手动选择 HEX/BIN | HEX 内嵌 `0x70100400` |

## 2. macOS 与 Windows 是否使用相同流程

核心流程相同，区别主要是命令、路径格式和驱动：

| 项目 | macOS | Windows |
| --- | --- | --- |
| Python 命令 | `python3` | 优先 `py -3`，也可能是 `python` |
| 路径分隔符 | `/` | `\`，PowerShell 也通常接受 `/` |
| ST-LINK 驱动 | 通常由系统/软件处理 | 需要安装 ST-LINK USB 驱动 |
| CubeProgrammer | `.app` | `.exe` |
| `cube-cmake` | 扩展的 Darwin 工具目录 | 扩展的 Windows 工具目录或 PATH |

### 2.1 当前脚本在 Windows 上的注意事项

脚本主体使用 Python `pathlib`，工程修改逻辑可跨平台运行，但应先检查两件事：

1. 在 PowerShell 中执行：

   ```powershell
   python3 --version
   ```

   如果找不到 `python3`，尝试：

   ```powershell
   py -3 --version
   python --version
   ```

2. 脚本生成的 `.vscode/tasks.json` 默认使用 `python3`。如果 Windows 没有该命令，将两个任务中的：

   ```json
   "command": "python3"
   ```

   改成：

   ```json
   "command": "python"
   ```

   或使用 `py` 时，将命令改为：

   ```json
   "command": "py"
   ```

   并在每个任务的 `args` 最前面增加：

   ```json
   "-3"
   ```

> 如果团队同时使用 macOS 和 Windows，建议后续让脚本为任务生成 `windows` 专用命令覆盖。当前版本在已提供 `python3` 命令的 Windows 环境中可直接使用。

## 3. 安装软件

### 3.1 两个平台都需要

安装以下软件：

1. Visual Studio Code；
2. STM32CubeMX；
3. STM32CubeProgrammer；
4. Python 3；
5. 正点原子 STM32N647 开发板软件包；
6. VS Code 的 **STM32CubeIDE for Visual Studio Code** 扩展。

在 VS Code 扩展市场中搜索 `STM32`，安装 STMicroelectronics 发布的 STM32 主扩展。主扩展会带入构建、调试、CMake 和 clangd 等相关组件。ST 官方安装说明见：

- [STM32CubeIDE for VS Code：安装](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/getting_started/installation.html)
- [STM32CubeIDE for VS Code：创建和导入工程](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/getting_started/first_project_creation.html)

### 3.2 Windows 额外事项

- 安装 Python 时勾选 **Add Python to PATH**；
- 安装 ST-LINK USB 驱动；
- 在 VS Code 的 STM32Cube Resources 中安装/更新 ST-LINK 驱动和所需工具；
- 第一次连接 ST-LINK 时，如扩展提示更新固件，按提示更新。

### 3.3 安装 STM32N6 Device Pack

首次打开 N6 工程时，STM32 扩展可能提示安装类似以下设备包：

```text
STMicroelectronics.stm32n6xx_dfp
```

允许扩展同步并安装。没有设备包时，代码索引、设备选择和调试配置可能不完整。

## 4. 推荐的软件包目录结构

不要只复制单个例程的 `Appli` 文件夹。脚本需要向上寻找正点原子的共享 `Drivers/BSP`。

推荐保持：

```text
SoftwarePackage/
├── Drivers/
│   └── BSP/
├── External_Loader/
│   └── MX25UM25645G_ATK-CNN647B/
│       └── Binary/
│           └── MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr
├── Projects/
│   ├── 01_LED/
│   ├── 08_Basic_Timer/
│   └── ...
└── Tools/
    └── atk_stm32n6_cmake_prepare.py
```

以 `08_Basic_Timer` 为例，最终工程结构大致为：

```text
08_Basic_Timer/
├── 08_Basic_Timer.ioc
├── CMakeLists.txt
├── CMakePresets.json
├── Drivers/
├── Secure_nsclib/
├── Appli/
│   ├── CMakeLists.txt
│   ├── CMakePresets.json
│   ├── Core/
│   ├── *.ld
│   └── build/
└── .vscode/
```

## 5. 从 `.ioc` 生成 CMake 工程

### 5.1 备份例程

第一次操作前，建议复制一份原始例程。不要直接在唯一一份官方例程上实验。

### 5.2 打开 `.ioc`

1. 启动 STM32CubeMX；
2. 选择 **File → Open Project**；
3. 打开例程根目录中的 `.ioc`，例如：

   ```text
   SoftwarePackage/Projects/08_Basic_Timer/08_Basic_Timer.ioc
   ```

4. 如果 CubeMX 提示升级固件包或迁移工程，先记录原版本；初次使用建议不要盲目升级到不兼容的软件包版本。

### 5.3 配置工程生成方式

进入 **Project Manager → Project**：

1. 保持 Project Name 与例程名称一致；
2. Toolchain / IDE 选择 **CMake**；
3. 工程输出目录选择当前例程根目录；
4. 对于当前工作流，至少生成 `Appli` 上下文；
5. 不要再尝试把 STM32CubeIDE 的 `.project/.cproject` 转换成 VS Code 工程。

ST 官方文档同样建议在 CubeMX 中选择 CMake 工具链，再用 VS Code 打开生成目录。CMake 工程通常会提供 Debug/Release Preset，并把不同 Preset 输出到 `build/<PresetName>`。

### 5.4 生成代码

点击 **GENERATE CODE**。完成后确认至少存在：

```text
CMakeLists.txt
CMakePresets.json
Appli/CMakeLists.txt
Appli/CMakePresets.json
```

如果没有 `Appli/CMakeLists.txt`，脚本无法继续，应回到 CubeMX 检查上下文与 CMake 生成设置。

## 6. 使用准备脚本

脚本统一放在：

```text
SoftwarePackage/Tools/atk_stm32n6_cmake_prepare.py
```

### 6.1 macOS

打开 Terminal，进入例程根目录：

```bash
cd "/Users/用户名/.../SoftwarePackage/Projects/08_Basic_Timer"
```

检查脚本版本：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --version
```

准备工程：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py
```

### 6.2 Windows PowerShell

进入例程根目录：

```powershell
cd "D:\STM32N647\SoftwarePackage\Projects\08_Basic_Timer"
```

检查版本：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py --version
```

准备工程：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py
```

如果系统提供的是 `python`，将 `py -3` 换成 `python`。

### 6.3 脚本会做什么

脚本会重复安全地执行以下工作：

- 修复 `GTZC_ERROR_CB_ID` 与 `IAC_ERROR_CB_ID` 不一致；
- 扫描 `Appli` 使用的正点原子 BSP 组件；
- 将 BSP 头文件目录和 `.c` 文件加入 `Appli/CMakeLists.txt`；
- 配置 Debug 使用 `LRUN_RAMxspi1.ld`；
- 配置 Release 使用 `ROMxspi2_RAMxspi1.ld`；
- 设置 Debug RAM 起点 `0x34000400`；
- 设置 Release NOR Flash 起点 `0x70100400`；
- 修复 Debug 链接脚本中残留的未声明 `ROM` 引用；
- 生成 Release HEX/BIN 目标；
- 生成或合并 `.vscode/settings.json`；
- 生成或合并 `.vscode/tasks.json`；
- 生成 Debug 专用 `.vscode/launch.json`；
- 配置 clangd 使用 Debug 的 `compile_commands.json`。

脚本产生的自动 CMake 块带有标记，重复运行不会不断追加相同内容。

## 7. 在 VS Code 中打开工程

必须打开具体例程根目录，例如：

```text
SoftwarePackage/Projects/08_Basic_Timer
```

不要把以下目录作为当前工程根目录：

```text
SoftwarePackage
SoftwarePackage/Projects
08_Basic_Timer/Appli
```

在 VS Code 中选择：

```text
File → Open Folder...
```

打开例程根目录后执行：

```text
Developer: Reload Window
```

首次切换到脚本工作流时，再执行一次：

```text
CMake: Delete Cache and Reconfigure
```

选择 `Debug` Preset。

如果 STM32 扩展要求 Setup/Discover Project：

1. 选择 `STM32N647X0H3Q`；
2. Core 选择 `Cortex-M55`；
3. 调试接口选择 ST-LINK/SWD；
4. 保存配置。

ST 官方导入流程也要求打开生成目录、选择 Preset，并在 STM32 扩展中发现/配置工程。

## 8. 代码补全与跳转

安装并启用 STM32Cube clangd。脚本会让 clangd 读取：

```text
Appli/build/Debug/compile_commands.json
```

第一次 Debug configure 后，clangd 才能获得完整编译参数。

如果没有补全：

1. 先构建一次 Debug；
2. 执行 `Developer: Reload Window`；
3. 查看 `Appli/build/Debug/compile_commands.json` 是否存在；
4. 检查 STM32Cube clangd 输出日志；
5. 避免同时让 Microsoft C/C++ IntelliSense 和 clangd 对同一文件提供诊断。

ST 官方说明中，用户维护的高层 `CMakeLists.txt` 可以添加 `target_sources()` 和 `target_include_directories()`；脚本正是使用这种方式补齐正点原子 BSP。参考：[STM32 CMake 说明](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/basic_concepts/cmake.html)。

## 9. Debug：编译、直接运行和断点调试

### 9.1 开发板设置

将开发板设置为 **Development boot**。按正点原子指南：

```text
BOOT1 → 3.3V
```

正确连接 ST-LINK、目标板电源和 USB。

### 9.2 命令行构建 Debug

macOS：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --build Debug
```

Windows：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py --build Debug
```

成功时应看到：

```text
Build files have been written to: .../Appli/build/Debug
Verified Debug ELF address: 0x34000400
```

Debug ELF 位于：

```text
Appli/build/Debug/<工程名>_Appli.elf
```

### 9.3 VS Code 调试

打开 **Run and Debug**，选择：

```text
STM32N6: Debug SRAM (F5 / Ctrl+F5)
```

- `F5`：断点调试；
- `Ctrl+F5`：不调试直接运行；
- 每次启动前，`preLaunchTask` 会先执行 Debug 构建；
- `launch.json` 会加载 `Appli/build/Debug/...elf`。

Debug 固件位于 SRAM，复位或断电后丢失，这是正常现象。

## 10. Release：生成用于 CubeProgrammer 的文件

### 10.1 构建 Release

macOS：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --build Release
```

Windows：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py --build Release
```

也可以在 VS Code 中执行：

```text
Tasks: Run Task → STM32N6: Build Release
```

成功时应看到：

```text
Verified Release ELF address: 0x70100400
Verified Release HEX address: 0x70100400
```

生成文件固定为：

```text
Appli/build/flash/<工程名>_Appli.hex
Appli/build/flash/<工程名>_Appli.bin
```

推荐使用 HEX，因为 HEX 文件内嵌目标地址。BIN 不携带地址，如果手动烧录 BIN，必须指定：

```text
0x70100400
```

## 11. 使用 STM32CubeProgrammer 手动烧录

本脚本不会自动烧录。

### 11.1 准备开发板

烧录前设置为 Development boot：

```text
BOOT1 → 3.3V
```

连接 ST-LINK 并给开发板供电。

### 11.2 选择 External Loader

打开 STM32CubeProgrammer：

1. 接口选择 ST-LINK；
2. 点击 Connect；
3. 打开 **External loaders**；
4. 选择正点原子提供的：

   ```text
   MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr
   ```

它通常位于：

```text
SoftwarePackage/External_Loader/MX25UM25645G_ATK-CNN647B/Binary/
```

### 11.3 烧录 Appli

在 Erasing & Programming 中选择：

```text
Appli/build/flash/<工程名>_Appli.hex
```

HEX 已包含 `0x70100400` 地址。开始编程并启用校验。

### 11.4 FSBL 注意事项

STM32N6 的 Appli 由 FSBL 启动。空白开发板或 FSBL 已被擦除时，只烧录 Appli 不会在 Flash boot 下运行。应先按照正点原子资料烧录匹配的 FSBL，再烧录 Appli。

### 11.5 切换到 Flash boot

烧录成功后设置：

```text
BOOT0 → GND
BOOT1 → GND
```

重新上电或复位。正点原子完整启动与烧录说明见：[STM32N647 开发流程 Step-by-Step](https://wiki.alientek.com/docs/Boards/STM32/DNN647/start-guide/start-development/step-by-step/)。

## 12. 修改 `.ioc` 后重新生成

每次使用 CubeMX 重新生成代码后，按以下顺序操作：

1. 在 CubeMX 中保存 `.ioc`；
2. 点击 GENERATE CODE；
3. 回到工程根目录；
4. 重新运行准备脚本；
5. 重新构建 Debug 或 Release；
6. 执行 `Developer: Reload Window`，让 clangd 更新索引。

macOS：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --build Debug
```

Windows：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py --build Debug
```

对于 CubeMX 生成的 C 文件，用户代码尽量放在：

```c
/* USER CODE BEGIN ... */
/* USER CODE END ... */
```

否则重新生成时可能被覆盖。

## 13. 常见问题

### 13.1 `multi-context and multi-core projects are not supported yet`

这是 STM32CubeIDE Eclipse 工程转换器的限制。不要继续转换 `.project/.cproject`；打开 `.ioc`，由 CubeMX 重新生成 CMake 工程。

### 13.2 `./SYS/sys.h: No such file or directory`

说明 BSP include 路径没有加入 CMake。重新运行准备脚本，并检查脚本是否检测到 `SYS`。

### 13.3 `undefined reference to HyperRAM_EnableMemoryMappedMode`

头文件找到了，但实现函数的 BSP `.c` 文件没有加入目标。重新运行脚本，检查输出是否检测到 `HyperRAM`。

### 13.4 `GTZC_ERROR_CB_ID undeclared`

这是生成代码与 STM32N6 IAC 枚举不一致。脚本会在条件匹配时将其修复为 `IAC_ERROR_CB_ID`。CubeMX 重新生成后应再次运行脚本。

### 13.5 `memory region ROM not declared`

Debug LRUN 脚本仍有 Section 指向不存在的 `ROM`。当前脚本会把 Debug 中这些残留引用重定向到 `RAM`。重新运行脚本并执行一次 Debug clean rebuild。

### 13.6 `ninja: no work to do`，但没有新 HEX

先确认实际构建目录：

```text
Debug   → Appli/build/Debug
Release → Appli/build/Release
```

不要依赖外层：

```text
<工程根目录>/build/Release
```

推荐直接运行脚本的 `--build Release` 或 VS Code 的 `STM32N6: Build Release` 任务。

### 13.7 `Image file ... not found`

检查 `.vscode/launch.json` 中不能出现：

```text
${CMAKE_PROJECT_NAME}
```

VS Code 不会展开 CMake 变量。正确路径应包含真实目标名，例如：

```text
${workspaceFolder}/Appli/build/Debug/08_Basic_Timer_Appli.elf
```

### 13.8 `preLaunchTask ... exited with code 2`

Problems 面板可能为空，因为 Python 错误不一定被 `$gcc` matcher 识别。直接运行：

```bash
python3 ../../Tools/atk_stm32n6_cmake_prepare.py --build Debug
```

或 Windows：

```powershell
py -3 ..\..\Tools\atk_stm32n6_cmake_prepare.py --build Debug
```

查看终端最后一条 `Error:`。

### 13.9 `cube-cmake: command not found`

先确认 STM32 VS Code 构建扩展已安装并激活。可先在 VS Code 中执行一次 CMake Configure，让扩展加载工具环境。

macOS 上脚本能够搜索 STM32 构建扩展的 Darwin 工具目录；Windows 上当前脚本主要依赖 `cube-cmake.exe` 已进入任务的 PATH。如果仍找不到，可从 STM32 扩展安装目录定位 `cube-cmake.exe`，或者为脚本增加 Windows 后备搜索路径。

### 13.10 Windows 找不到 `python3`

使用：

```powershell
py -3
```

或：

```powershell
python
```

同时调整 `.vscode/tasks.json` 中的 Python 命令。

### 13.11 Debug 出现 `LOAD segment with RWX permissions`

Debug LRUN 将代码和可写数据放在同一块 SRAM，因此 GNU ld 可能报告 RWX 段警告。如果地址校验显示：

```text
Verified Debug ELF address: 0x34000400
```

且没有其他链接错误，该警告通常可以接受。

## 14. 建议的日常操作

### 开始开发

```text
打开例程根目录 → 运行准备脚本 → 构建 Debug → F5
```

### 修改普通 C 代码后

```text
直接 F5；preLaunchTask 会先构建 Debug
```

### 修改 `.ioc` 后

```text
CubeMX Generate Code → 再运行准备脚本 → Debug 构建 → F5
```

### 准备部署版本

```text
Build Release → 检查 0x70100400 → CubeProgrammer 烧录 HEX
```

## 15. 应纳入版本控制的文件

建议提交：

```text
*.ioc
Appli/Core/
Appli/CMakeLists.txt
Appli/*.ld
.vscode/settings.json
.vscode/tasks.json
.vscode/launch.json
atk_stm32n6_cmake_prepare.py
```

通常不提交：

```text
Appli/build/
build/
*.elf
*.bin
*.hex
```

如果团队希望保留可发布固件，可以把经过验证的 HEX 复制到专门的 `Releases/` 目录，并记录源码版本、构建日期和脚本版本。

## 16. 参考资料

- [正点原子 STM32N647 开发流程 Step-by-Step](https://wiki.alientek.com/docs/Boards/STM32/DNN647/start-guide/start-development/step-by-step/)
- [ST：STM32CubeIDE for Visual Studio Code 安装](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/getting_started/installation.html)
- [ST：创建/导入 STM32CubeMX 工程](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/getting_started/first_project_creation.html)
- [ST：STM32 CMake 基础与生成文件](https://dev.st.com/stm32cube-docs/stm32cubeide-vscode/latest/en/docs/markup/basic_concepts/cmake.html)

