# Pi CLI 入门与 STM32N6 开发设置教程

> 面向第一次使用终端 AI 编程助手的读者。示例环境：macOS、VS Code、Pi CLI、STM32CubeMX/CMake、正点原子 STM32N647。
>
> 本文依据 2026-07-19 的 Pi 官方文档编写。Pi 更新后，个别菜单名称可能变化。

## 1. 先建立一个最简单的认识

Pi CLI 可以理解为“运行在终端里的 AI 编程助手”。它和普通聊天工具的主要区别是：

- 它知道自己是从哪个文件夹启动的；
- 它可以读取这个文件夹中的源代码；
- 在得到允许后，它可以运行编译命令、修改文件和查看构建日志；
- 它会自动保存对话，之后可以继续；
- 它可以加载 Skill，按照一套预先写好的工作规范处理任务。

Pi 不是编译器，也不是烧录器。你的实际工具分工是：

| 工具 | 主要职责 |
| --- | --- |
| STM32CubeMX | 配置芯片外设并生成工程代码 |
| CMake、Ninja、GNU Arm | 编译和链接程序 |
| VS Code STM32 扩展 | 提供工程入口、编译按钮、代码索引和调试界面 |
| STM32CubeProgrammer | 将固件烧录进目标板或外部 Flash |
| Pi CLI | 阅读工程、分析报错、修改文件、调用上述命令 |
| Skill | 告诉 Pi 应该遵守什么流程和规则 |

## 2. 每次都要从正确的文件夹启动 Pi

这是最重要的使用习惯。

不要先在主目录运行 `pi`，然后再告诉它工程在哪里。应当先进入具体工程，再启动 Pi：

```bash
cd /Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/03_Button2
pwd
pi --name "03_Button2-CMake"
```

`pwd` 用于确认当前目录。正常情况下，应输出 `03_Button2` 的完整路径。

`--name` 给本次对话命名。以后从历史记录中寻找时，会比一串自动生成的名称清楚很多。

如果只是继续刚才的工作，可以使用：

```bash
pi -c
```

如果想从历史会话中选择：

```bash
pi -r
```

## 3. Pi 启动后的界面怎么看

Pi 官方把界面分成四个区域：启动信息、消息区、输入区和底栏。

```text
┌──────────────── 启动信息 ────────────────┐
│ 当前加载的上下文文件、Skill、扩展、快捷键 │
├──────────────── 消息区 ──────────────────┤
│ 你的问题、Pi 的回答、命令、命令输出、错误 │
├──────────────── 输入区 ──────────────────┤
│ 在这里输入问题，Enter 发送                │
├──────────────── 底栏 ────────────────────┤
│ 当前目录、会话名、上下文占用、模型等       │
└───────────────────────────────────────────┘
```

### 3.1 启动信息

这里可能出现：

- 当前工作目录；
- 已加载的 `AGENTS.md` 或 `CLAUDE.md`；
- 已加载的 Skill 数量；
- 可用的扩展或提示词模板；
- 项目是否可信。

如果你刚安装了 Skill，却没有在启动信息中看到它，先输入：

```text
/reload
```

仍然看不到时，退出 Pi，再从工程目录重新运行 `pi`。

### 3.2 消息区

Pi 在这里显示：

- 它正在读取哪些文件；
- 它准备运行什么命令；
- 编译器输出；
- 修改了哪些文件；
- 最终结论。

命令输出太长时，可按 `Ctrl+O` 折叠或展开工具输出。

### 3.3 输入区

- `Enter`：发送；
- `Shift+Enter` 或 `Ctrl+J`：换行，不发送；
- 输入 `/`：查看命令；
- 输入 `@`：搜索并引用工程中的文件；
- `Tab`：补全文件路径或命令；
- `Ctrl+G`：用外部编辑器编写较长的问题。

在较新的 VS Code 集成终端中，`Shift+Enter` 默认可用。若没有反应，可暂时使用 `Ctrl+J` 换行。

### 3.4 底栏

常见信息包括：

- 当前工作目录；
- 当前会话名称；
- 当前模型；
- 上下文使用比例；
- Token 或费用信息；
- 缓存信息。

“上下文”可以理解为 Pi 当前能同时记住的对话和文件内容。接近上限时，Pi 会自动压缩较早内容。长工程不要把所有无关文件一次性都交给它读取。

## 4. 第一次启动时常见的选择

### 4.1 Project Trust：是否信任工程

如果 Pi 发现项目内包含 `.pi/settings.json`、扩展或项目级 Skill，可能询问是否信任项目。

外行可以按下面的原则选择：

- 自己下载并检查过的正点原子例程：可以信任；
- 自己创建的工程：可以信任；
- 来源不明的压缩包或 Git 仓库：先不要信任；
- 不确定时：选择暂不信任，检查后再用 `/trust`。

全局建议保持 `ask`，不要设成自动信任所有项目。

### 4.2 Provider：模型服务从哪里来

Provider 是提供 AI 模型和计费/登录服务的平台，例如 ChatGPT、Claude、GitHub Copilot 或其他 API 服务。

登录使用：

```text
/login
```

退出某个平台登录：

```text
/logout
```

不要在聊天消息或源代码里粘贴 API Key。优先通过 `/login` 保存凭据。

### 4.3 Model：实际使用哪个 AI 模型

输入：

```text
/model
```

然后用方向键选择，按 Enter 确认。模型影响代码能力、速度和费用，但不会改变 CMake 的 Debug/Release 配置。

初学阶段不需要频繁更换模型。选择一个代码能力较强、可稳定使用的模型即可。

### 4.4 Thinking Level：Pi 在回答前思考多少

/settings/Thinking Level

可用等级一般包括 `off`、`minimal`、`low`、`medium`、`high`、`xhigh`、`max`。按 `Shift+Tab` 可以循环切换。

针对你的工作流，建议：

| 任务 | 建议等级 |
| --- | --- |
| 查找文件、解释一段 HAL 代码 | `low` 或 `medium` |
| 修改普通 GPIO、UART 代码 | `medium` |
| CMake、链接脚本、Undefined reference | `high` |
| HardFault、TrustZone、缓存、XSPI 启动问题 | `high` 或 `xhigh` |
| 简单问答 | 不需要 `max` |

默认设为 `medium` 比较合适。遇到复杂问题时临时切到 `high`，完成后再切回来。

## 5. `/settings` 中常见选项是什么意思

在 Pi 输入：

```text
/settings
```

方向键移动，Enter 修改，Esc 退出。不同版本显示的项目可能略有差别。

| 设置 | 白话解释 | STM32 工作流建议 |
| --- | --- | --- |
| Thinking level | 默认思考强度 | `medium` |
| Theme | 深色或浅色外观 | 按个人喜好 |
| Hide thinking block | 是否隐藏思考过程显示 | 初学可隐藏，界面更清楚 |
| Project trust | 是否自动信任项目配置 | `ask` |
| Steering mode | Pi 工作时，你的新消息如何插入 | `one-at-a-time` |
| Follow-up mode | 排队消息如何依次发送 | `one-at-a-time` |
| Transport | 模型通信方式 | `auto` |
| Skill commands | 是否启用 `/skill:name` | 开启 |
| Auto compaction | 上下文将满时是否自动压缩 | 开启 |
| Retry | 网络暂时失败时是否自动重试 | 开启，3 次 |
| External editor | `Ctrl+G` 打开什么编辑器 | `code --wait` |
| Install telemetry | 是否发送匿名安装/更新信息 | 可关闭，不影响基本功能 |
| Show images | 终端是否显示图片 | 开启即可 |

`Steering` 与 `Follow-up` 的区别：

- Pi 还在工作时直接按 Enter 发送，新消息是 steering，表示“调整它当前正在做的事”；
- `Alt+Enter` 发送的是 follow-up，表示“当前任务结束后再处理这条消息”。

初学阶段两者都用 `one-at-a-time`，最不容易出现多条指令混在一起。

## 6. 推荐的全局设置文件

Pi 的全局设置位于：

```text
~/.pi/agent/settings.json
```

建议先在普通终端中运行：

```bash
mkdir -p ~/.pi/agent
code --wait ~/.pi/agent/settings.json
```

如果 `code` 命令不可用，可以使用：

```bash
nano ~/.pi/agent/settings.json
```

写入下面内容。JSON 不能包含注释，也不能在最后一项后面多写逗号。

```json
{
  "defaultThinkingLevel": "medium",
  "theme": "dark",
  "externalEditor": "code --wait",
  "defaultProjectTrust": "ask",
  "hideThinkingBlock": true,
  "doubleEscapeAction": "tree",
  "steeringMode": "one-at-a-time",
  "followUpMode": "one-at-a-time",
  "transport": "auto",
  "enableSkillCommands": true,
  "enableInstallTelemetry": false,
  "compaction": {
    "enabled": true,
    "reserveTokens": 16384,
    "keepRecentTokens": 20000
  },
  "retry": {
    "enabled": true,
    "maxRetries": 3,
    "baseDelayMs": 2000
  },
  "terminal": {
    "showImages": true
  },
  "images": {
    "autoResize": true
  }
}
```

这里故意没有写死 `defaultProvider` 和 `defaultModel`。先通过 `/login` 和 `/model` 选择可用模型，更不容易因为模型名称升级而失效。

保存后重新启动 Pi。也可以在 Pi 中输入 `/reload`，但全局行为发生异常时，完全退出再启动更稳妥。

## 7. 最值得记住的快捷键

| 操作 | 快捷键 |
| --- | --- |
| 发送消息 | `Enter` |
| 输入换行 | `Shift+Enter` 或 `Ctrl+J` |
| 中止当前工作 | `Esc` |
| 清空尚未发送的输入 | `Ctrl+C` |
| 输入框为空时退出 Pi | `Ctrl+D` |
| 选择模型 | `Ctrl+L` 或 `/model` |
| 切换思考等级 | `Shift+Tab` |
| 折叠/展开命令输出 | `Ctrl+O` |
| 复制 Pi 上一次回答 | `Ctrl+X` |
| 打开外部编辑器 | `Ctrl+G` |
| 查看全部快捷键 | `/hotkeys` |

如果 Pi 正在准备做你不希望发生的操作，按 `Esc` 中止。不要连续乱按 `Ctrl+C`，因为它主要用于清空输入，并不总等同于终止代理。

## 8. 最常用的斜杠命令

| 命令 | 用途 |
| --- | --- |
| `/login` | 登录模型服务 |
| `/model` | 选择模型 |
| `/settings` | 打开常用设置 |
| `/name 名称` | 给当前会话命名 |
| `/session` | 查看当前会话信息 |
| `/new` | 开始一个全新会话 |
| `/resume` | 选择以前的会话 |
| `/tree` | 回到对话中的某个节点并从那里继续 |
| `/compact` | 主动压缩较早上下文 |
| `/reload` | 重新加载 Skill、扩展和设置 |
| `/skill:名称` | 显式调用一个 Skill |
| `/quit` | 退出 Pi |

初学阶段不必立即学习 `/fork`、`/clone` 和复杂的会话树。先掌握 `/new`、`/resume`、`/name` 和 `/compact` 即可。

## 9. Pi 运行 Shell 命令的两种方式

在输入框中以 `!` 开头：

```text
!pwd
```

命令输出会同时显示给你并发送给 AI，Pi 可以继续分析。

以两个感叹号开头：

```text
!!clear
```

命令照常运行，但输出不会加入 AI 上下文。清屏、查看个人信息等不需要 AI 分析的命令可用这种方式。

STM32 编译错误通常应该使用一个感叹号，或者直接让 Pi 运行构建，因为需要把错误日志交给模型分析。

## 10. 怎样提问，Pi 才不容易改错工程

一个可靠的请求最好包含四部分：

1. 目标：希望最终得到什么；
2. 范围：允许读取或修改哪些文件；
3. 限制：明确不能做什么；
4. 验证：完成后如何证明正确。

例如：

```text
目标：编译当前 STM32N647 工程的 Release 版本。
范围：可以读取整个工程，只允许修改 Appli/CMakeLists.txt 和用户维护的链接脚本。
限制：不要修改 mx-generated.cmake，不要烧录，不要使用 OpenOCD。
验证：构建成功后列出 ELF、MAP、HEX、BIN 路径，并确认 HEX 的代码地址从 0x70100400 开始。
```

比“帮我编译一下”安全得多。

第一次接触新例程时，先要求只读检查：

```text
先只检查工程结构和现有配置，不要修改任何文件，不要运行烧录命令。告诉我你准备修改什么以及原因。
```

确认方案后再说：

```text
按刚才列出的方案实施。修改前保留现有用户代码，完成后显示改动摘要并执行构建验证。
```

## 11. 推荐的 STM32 日常会话方式

每个例程单独使用一个会话：

```bash
cd /完整路径/Projects/03_Button2
pi --name "03_Button2-Release"
```

进入后依次做：

1. 输入 `/model`，确认模型；
2. 用 `Shift+Tab` 将思考等级设为 `medium`；
3. 输入 `/skill:build-cmake`，让它只读检查；
4. 确认它识别了 STM32N647、CMake、链接脚本和 BSP；
5. 先执行修复脚本的 `--dry-run`；
6. 查看计划修改内容；
7. 再允许应用修改；
8. 构建 Debug 或 Release；
9. 检查 ELF、MAP、HEX、BIN；
10. 最后由你决定是否在 CubeProgrammer 中烧录。

不要把 `01_LED`、`03_Button2` 和其他例程都混在同一个长会话里，否则 Pi 可能把旧工程路径误用到新工程。

## 12. 什么时候应该同意 Pi 的操作

一般可以同意：

- 读取当前工程文件；
- 执行 `cmake`、`cube-cmake`、`ninja`；
- 执行 `arm-none-eabi-size`、`objdump`、`readelf`；
- 修改你明确允许的 `CMakeLists.txt`；
- 运行修复脚本的 `--dry-run`。

需要仔细确认：

- 写入或替换链接脚本；
- 修改 CubeMX 自动生成文件；
- 安装新软件或 npm/Python 包；
- 访问工程目录外的文件；
- 运行 `git clean`、删除文件或覆盖大量文件；
- 自动烧录、擦除外部 Flash 或修改 Option Bytes。

看到不理解的命令时，可以拒绝并问：

```text
先不要执行。请逐项解释这条命令会读取、修改或删除什么，以及失败时如何恢复。
```

## 13. 常见问题

### 13.1 `pi: command not found`

先检查 NVM：

```bash
nvm --version
command -v pi
```

如果新终端没有加载 NVM，确认 `~/.zshrc` 中存在：

```bash
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"
```

### 13.2 Pi 在错误的目录

退出 Pi：输入 `/quit`，然后：

```bash
cd /正确工程路径
pwd
pi
```

### 13.3 Skill 已安装但命令不存在

输入：

```text
/reload
```

然后输入 `/skill:`，查看自动补全。如果仍不存在，检查：

```bash
find ~/.agents/skills -maxdepth 3 -name SKILL.md -print
```

### 13.4 对话太长，Pi 开始混淆

先输入：

```text
/compact 请保留当前工程路径、已修改文件、构建命令、剩余错误和禁止使用 OpenOCD 的约束。
```

如果已经混淆严重，使用 `/new` 开新会话，并重新提供当前状态。

### 13.5 Pi 正在做错误的事

按 `Esc` 中止，然后明确说明：

```text
停止当前操作。不要撤销用户已有修改。先重新确认当前目录、目标工程和允许修改的文件。
```

## 14. 一页速查

```text
启动：     cd 工程目录 && pi --name "任务名"
继续：     pi -c
历史：     pi -r
模型：     /model
设置：     /settings
技能：     /skill:build-cmake
新会话：   /new
压缩上下文：/compact
重新加载： /reload
中止：     Esc
退出：     /quit 或空输入时 Ctrl+D
```

## 参考资料

- [Pi Quickstart](https://pi.dev/docs/latest/quickstart)
- [Using Pi](https://pi.dev/docs/latest/usage)
- [Pi Settings](https://pi.dev/docs/latest/settings)
- [Pi Keybindings](https://pi.dev/docs/latest/keybindings)
- [Pi Sessions](https://pi.dev/docs/latest/sessions)
- [Pi Terminal Setup](https://pi.dev/docs/latest/terminal-setup)
- [Pi Providers](https://pi.dev/docs/latest/providers)

