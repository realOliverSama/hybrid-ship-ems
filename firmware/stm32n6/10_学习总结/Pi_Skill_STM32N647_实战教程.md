# Pi Skill 与 STM32N647 CMake 实战教程

> 面向不熟悉 Skill、CMake 和 AI 代理工作方式的读者。本文以正点原子 STM32N647、CubeMX 原生 CMake、macOS、VS Code 和 STM32CubeProgrammer 为例。

## 1. Skill 到底是什么

Skill 可以理解为“写给 AI 看的操作手册”。

一个 Skill 通常包含：

```text
某个-skill/
├── SKILL.md       # 主要操作规范
├── scripts/       # 可选的辅助脚本
└── references/    # 可选的参考资料
```

启动 Pi 时，它只读取每个 Skill 的名称和简短说明。任务符合某个 Skill 时，Pi 才读取完整的 `SKILL.md`。这叫按需加载。

Skill 本身不是：

- 编译器；
- STM32CubeMX；
- CMake；
- 调试器；
- 烧录器；
- 保证永远正确的自动修复程序。

它的作用是让 Pi 知道“遇到这种任务时，应当检查什么、运行什么、怎样报告结果”。

## 2. 你当前安装的三个 Skill 分别做什么

### 2.1 `build-cmake`

用途：

- 检测 CMake、Ninja 和工具链；
- 找到已有的 CMake 配置或构建目录；
- 执行配置和构建；
- 查找 ELF、HEX、BIN 等产物；
- 根据错误类型给出下一步建议。

局限：这是通用嵌入式 CMake Skill，不天然知道正点原子 STM32N647 的全部规则。

### 2.2 `memory-analysis`

用途：

- 分析 `.map` 文件；
- 分析 ELF；
- 查看 `.text`、`.data`、`.bss` 等段；
- 找出占用空间最大的函数和变量；
- 检查链接地址和内存使用率。

### 2.3 `serial-monitor`

用途：

- 列出 macOS 串口；
- 选择正确的串口；
- 以指定波特率抓取日志；
- 从日志中识别启动失败、断言、重复复位等现象。

该 Skill 可能需要 Python 的 `pyserial`。安装依赖前应让 Pi 解释命令和安装范围。

## 3. 你的 STM32N647 必须额外遵守的规则

通用 Skill 不知道这些板级细节，因此每次重要任务都应明确告诉 Pi：

1. 目标芯片是 STM32N647；
2. 工程来自正点原子；
3. CubeMX 生成原生 CMake；
4. 用户扩展放在 `Appli/CMakeLists.txt`；
5. 不修改 `mx-generated.cmake`；
6. 外部 BSP 位于 `SoftwarePackage/Drivers/BSP/`；
7. `GTZC_ERROR_CB_ID` 只有在头文件实际定义 `IAC_ERROR_CB_ID` 时才修复；
8. Debug 通常运行于 RAM，当前工程规则为 `0x34000400`；
9. Release 固件必须链接到外部 Flash 地址 `0x70100400`；
10. Release 应生成 Intel HEX 和 BIN；
11. HEX 文件带地址，BIN 文件本身不保存地址；
12. 烧录使用 STM32CubeProgrammer 和正点原子外部加载器；
13. 不使用通用 `flash-openocd` 自动烧录；
14. 不自动修改 Option Bytes；
15. 烧录前必须由用户确认目标、文件和地址。

这就是为什么最终最好制作一个专用的 `stm32n647-atk-development` Skill。当前的三个通用 Skill可以先用，但每次都要附加上述约束。

## 4. 确认 Skill 是否安装成功

在普通终端运行：

```bash
find ~/.agents/skills -maxdepth 3 -name SKILL.md -print
```

正常情况下应看到类似：

```text
/Users/oliverlyu/.agents/skills/build-cmake/SKILL.md
/Users/oliverlyu/.agents/skills/memory-analysis/SKILL.md
/Users/oliverlyu/.agents/skills/serial-monitor/SKILL.md
```

再检查共享文件：

```bash
ls ~/.agents/skills/shared
```

如果 `build-cmake` 的脚本缺少 `shared`，运行时可能出现 `No module named tool_config` 或 `profile_store`。

最后验证脚本：

```bash
python3 ~/.agents/skills/build-cmake/scripts/cmake_builder.py --help
```

能显示帮助内容，说明 Skill 本身和共享依赖已经连通。

## 5. 在 Pi 里怎样找到 Skill

进入工程并启动：

```bash
cd /Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/03_Button2
pi --name "03_Button2-Skill练习"
```

输入：

```text
/skill:
```

不要立即按 Enter，先观察自动补全列表。应该能看到：

```text
/skill:build-cmake
/skill:memory-analysis
/skill:serial-monitor
```

如果看不到：

```text
/reload
```

再次输入 `/skill:`。仍然没有时，退出 Pi 后重新启动。

Pi 默认启用 Skill 命令。如果被关闭，可在 `/settings` 中开启，或确认全局设置中存在：

```json
{
  "enableSkillCommands": true
}
```

## 6. 调用 Skill 的正确写法

最基本格式：

```text
/skill:skill名称 具体任务
```

例如：

```text
/skill:build-cmake 检查当前工程的 CMake 配置，但不要修改文件，也不要执行烧录。
```

Pi 会把两部分结合起来：

- `build-cmake` Skill 提供通用 CMake 工作规范；
- 你后面的文字提供本次工程目标和限制。

虽然 Pi 可能自动判断并加载 Skill，但初学阶段建议显式写 `/skill:build-cmake`。这样更容易确认它采用了正确流程。

## 7. 一条高质量 Skill 指令的结构

推荐使用下面模板：

```text
/skill:build-cmake

工程信息：这是正点原子 STM32N647 的 CubeMX 原生 CMake 工程。
目标：……
允许：……
禁止：……
验证：……
```

示例：

```text
/skill:build-cmake

工程信息：这是正点原子 STM32N647 的 CubeMX 原生 CMake 工程。
目标：检查当前 Release 构建配置。
允许：读取整个工程和已有构建日志。
禁止：不要修改任何文件，不要删除 build 目录，不要烧录，不要使用 OpenOCD。
验证：告诉我当前使用哪个链接脚本、ROM 起始地址，以及预期生成哪些产物。
```

## 8. 完整实战：从新例程到成功编译

以下流程可以用于 `01_LED`、`03_Button2` 和后续类似例程。

### 阶段 A：进入正确工程

```bash
cd /完整路径/SoftwarePackage/Projects/03_Button2
pwd
ls
pi --name "03_Button2-首次迁移"
```

不要在 `SoftwarePackage`、`Projects` 或主目录启动后，让 Pi 猜测具体例程。

### 阶段 B：只读检查

进入 Pi 后输入：

```text
/skill:build-cmake

这是正点原子 STM32N647 工程。先只读检查，不要修改或生成任何文件。
请确认：
1. .ioc 文件；
2. 顶层和 Appli 的 CMakeLists.txt；
3. Debug 和 Release 构建目录；
4. 当前链接脚本；
5. main.c 引用的工程外 BSP 模块；
6. 是否存在 secure_nsc.c 的 GTZC/IAC 不一致。
不要使用 OpenOCD，不要烧录。
```

你应当检查 Pi 的回答是否明确提到了当前例程路径，而不是上一个例程。

### 阶段 C：预览自动修复

如果工程根目录放有 `atk_stm32n6_cmake_prepare.py`，输入：

```text
先运行下面命令的 dry-run，只显示计划修改，不要真正写文件：

python3 ./atk_stm32n6_cmake_prepare.py . --dry-run

逐项解释它计划修改的文件和原因。不要烧录。
```

如果脚本放在统一工具目录，应提供明确的绝对路径，例如：

```text
python3 /Users/oliverlyu/Developer/stm32-tools/atk_stm32n6_cmake_prepare.py . --dry-run
```

不要让 Pi 自己猜脚本位置。

### 阶段 D：应用修复

确认 dry-run 内容正确后输入：

```text
允许执行刚才的准备脚本，但只允许它完成以下任务：
1. 补充当前 main.c 实际引用的正点原子 BSP 头文件和源文件；
2. 在头文件确实使用 IAC_ERROR_CB_ID 时修复 secure_nsc.c；
3. 配置 Debug 与 Release 链接脚本；
4. Release ROM 起始地址必须是 0x70100400；
5. 添加 Release HEX/BIN 生成步骤。

执行后显示被修改的文件和摘要，不要烧录。
```

### 阶段 E：构建 Debug

```text
/skill:build-cmake

构建当前工程的 Debug 配置。优先复用 VS Code/STM32Cube 扩展已经生成的构建目录和工具链，不要擅自更换编译器。
不要删除整个 build 目录，不要烧录。
如果失败，只分析第一次真正导致停止的错误。
成功后列出 ELF 和 MAP 路径。
```

Debug 的主要目的通常是 RAM 运行和调试。不要因为 Debug 地址不是 `0x70100400` 就认为错误。

### 阶段 F：构建 Release

```text
/skill:build-cmake

构建当前工程的 Release 配置。必须满足：
- 使用外部 Flash Release 链接脚本；
- ROM ORIGIN = 0x70100400；
- 生成 ELF、MAP、Intel HEX 和 BIN；
- 不要烧录。

完成后报告每个产物的绝对路径，并检查 HEX 中的最低数据地址。
```

注意：

- `.hex` 包含地址信息，适合在 CubeProgrammer 中按文件地址烧录；
- `.bin` 只是连续字节，不包含 `0x70100400`，烧录 BIN 时必须另外填写地址。

### 阶段 G：检查内存与地址

```text
/skill:memory-analysis

分析当前 Release 的 ELF 和 MAP：
1. 确认主要代码段位于 0x70100400 所属区域；
2. 列出 ROM、RAM、EXTRAM 使用量；
3. 列出最大的 20 个函数或变量；
4. 检查是否有段落落入意外地址。
只分析，不修改文件，不烧录。
```

### 阶段 H：烧录

当前不建议交给通用 Skill 自动完成。请使用 STM32CubeProgrammer 图形界面，并人工确认：

- 已连接正确的开发板；
- 已选择正点原子要求的外部加载器，例如 `ATK-CNN647B`；
- 选择的是当前例程的 Release HEX；
- HEX 内地址正确；
- 如果使用 BIN，手动地址是 `0x70100400`；
- 没有误选其他例程的旧文件。

## 9. 针对常见报错的 Skill 提问方式

### 9.1 `No such file or directory`，缺少头文件

```text
/skill:build-cmake

分析这次编译中的第一个 No such file or directory 错误。
先找到头文件在工程中的真实位置，再检查 target_include_directories。
不要通过复制头文件解决，不要修改 mx-generated.cmake。
给出最小 CMake 修改并解释相对路径以哪个 CMakeLists.txt 为基准。
```

### 9.2 `undefined reference`

```text
/skill:build-cmake

分析第一个 undefined reference。
先搜索函数实现位于哪个 .c 文件，再确认该文件是否进入 target_sources。
如果属于正点原子 Drivers/BSP，只修改 Appli/CMakeLists.txt 的用户维护区域。
不要修改函数声明来掩盖链接问题。
```

### 9.3 `undeclared`，例如 GTZC/IAC

```text
/skill:build-cmake

分析 undeclared 错误。先检查相关头文件实际定义了哪个枚举或宏，不要直接按名字猜测。
如果 secure_nsc.h 只有 IAC_ERROR_CB_ID，而 secure_nsc.c 使用 GTZC_ERROR_CB_ID，再做最小替换。
说明该文件重新由 CubeMX 生成后是否可能被覆盖。
```

### 9.4 链接地址或 relocation 错误

```text
/skill:build-cmake

这是 STM32N647 链接错误。请检查：
1. 函数实现是否真的被编译；
2. 目标文件架构是否为 Cortex-M55 Thumb；
3. 当前 Debug/Release 链接脚本；
4. ROM/RAM/EXTRAM 地址；
5. 是否混入旧构建目录中的对象文件。

先诊断，不要删除整个工程，不要烧录。
```

## 10. 怎样判断 Pi 真的使用了 Skill

以下迹象说明它大概率已经加载：

- 你输入的命令被识别为 `/skill:build-cmake`，而不是普通文字；
- Pi 读取了 Skill 的 `SKILL.md` 或引用资料；
- 它使用 Skill 自带的脚本；
- 输出中包含构建目录、生成器、产物路径和失败分类；
- 工程根目录可能出现该 Skill 使用的 `.em_skill.json` 状态文件。

如果 Pi 完全没有按照 Skill 工作，可以明确说：

```text
请先读取并遵守 build-cmake Skill，再处理后面的任务。不要跳过 Skill 中的检查步骤。
```

## 11. Skill 询问执行权限时怎么看

Skill 可以指导 Pi 运行脚本，但不代表所有命令都应该同意。

### 通常安全的只读命令

```text
pwd
find
rg
ls
cmake --version
arm-none-eabi-size
arm-none-eabi-readelf
arm-none-eabi-objdump
python3 ... --dry-run
```

### 会修改工程，但通常可恢复

```text
运行工程准备脚本
修改 Appli/CMakeLists.txt
修改用户维护的链接脚本
生成 build 目录
```

执行前应要求 Pi 列出目标文件，并避免覆盖用户已有修改。

### 必须人工确认

```text
删除或清空目录
git reset / git clean
覆盖大量自动生成文件
擦除芯片或外部 Flash
修改 Option Bytes
自动运行 CubeProgrammer 烧录
```

## 12. 常见 Skill 故障

### 12.1 只看到 `embed-ai-tool`

这是仓库根 Skill 被发现、子 Skill 没有被自动发现造成的。使用仓库自己的 `install.py --skills ...` 安装选定 Skill，并将它们暴露到 `~/.agents/skills/`。

### 12.2 `/skill:build-cmake` 不存在

检查：

```bash
find ~/.agents/skills -maxdepth 3 -name SKILL.md -print
```

然后在 Pi 中运行：

```text
/reload
```

### 12.3 `No module named tool_config`

说明 `shared` 目录没有一同安装或链接。检查：

```bash
ls ~/.agents/skills/shared/tool_config.py
ls ~/.agents/skills/shared/profile_store.py
```

### 12.4 Skill 选择了 `RelWithDebInfo`

通用 CMake Skill 可能把非调试构建默认理解为 `RelWithDebInfo`。你的工程使用明确的 `Debug` 和 `Release`，所以指令中应写清楚：

```text
使用当前工程已经定义的 Release 配置，不要改成 RelWithDebInfo。
```

### 12.5 Skill 推荐 OpenOCD

这是通用 Skill 的下游建议，不代表适合 STM32N647 外部 Flash。直接拒绝，并说明：

```text
不要调用 flash-openocd 或 debug-gdb-openocd 完成下载。当前工程烧录必须由 STM32CubeProgrammer 配合正点原子外部加载器完成。
```

## 13. 在 Kimi Code 中共用相同 Skill

Pi 和 Kimi Code 都能扫描：

```text
~/.agents/skills/
```

因此同一份 Skill 不需要复制两套。在 Kimi Code 中也可以输入：

```text
/skill:build-cmake 检查当前 STM32N647 Release 构建，不要使用 OpenOCD。
```

建议 Pi 和 Kimi 不要同时修改同一个工程文件。更安全的方式是：一个负责执行修改，另一个只负责复核构建日志或 diff。

## 14. 进阶方向：制作真正适合你的专用 Skill

最终可以建立：

```text
~/.agents/skills/stm32n647-atk-development/
├── SKILL.md
├── scripts/
│   └── atk_stm32n6_cmake_prepare.py
└── references/
    ├── memory-map.md
    ├── cmake-rules.md
    └── flashing-checklist.md
```

这样可以把以下内容固化一次：

- 正点原子 BSP 自动探测规则；
- GTZC/IAC 修复条件；
- Debug/Release 链接脚本选择；
- `0x70100400` Release 地址；
- HEX/BIN 生成；
- STM32CubeProgrammer 外部加载器检查清单；
- 禁止通用 OpenOCD 自动烧录；
- 重新生成 CubeMX 后的复查流程。

届时日常命令可以缩短为：

```text
/skill:stm32n647-atk-development 准备并编译当前例程的 Release 版本，只构建和验证，不烧录。
```

## 15. 一页实战速查

```text
1. cd 到具体例程目录
2. pi --name "例程名-任务"
3. /skill:build-cmake 先只读检查，不修改、不烧录
4. 运行 atk_stm32n6_cmake_prepare.py --dry-run
5. 确认后应用准备脚本
6. /skill:build-cmake 构建 Debug 或 Release
7. /skill:memory-analysis 检查 ELF、MAP 和地址
8. 确认 Release HEX 位于 0x70100400
9. 退出 Pi
10. 人工使用 CubeProgrammer + 正点原子外部加载器烧录
```

每次都附加下面四条约束：

```text
不要修改 mx-generated.cmake。
不要使用 OpenOCD 烧录。
Release ROM 起始地址必须是 0x70100400。
烧录前必须由用户人工确认。
```

## 参考资料

- [Pi Skills 官方文档](https://pi.dev/docs/latest/skills)
- [Pi 日常使用文档](https://pi.dev/docs/latest/usage)
- [Pi 设置文档](https://pi.dev/docs/latest/settings)
- [embed-ai-tool 仓库](https://github.com/LeoKemp223/embed-ai-tool)
- [正点原子 STM32N647 开发指南](https://wiki.alientek.com/docs/Boards/STM32/DNN647/start-guide/start-development/step-by-step)

