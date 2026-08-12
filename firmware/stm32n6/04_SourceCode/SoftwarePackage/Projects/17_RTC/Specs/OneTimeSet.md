# Feature Specification

Status: Approved

Feature: 按一下 KEY0 进入"一次性完成全部设置"的初始化模式（OneTimeSet）
Project: Projects/17_RTC (STM32N647, ALIENTEK ATK-CNN647B)

## 1. Objective

在现有 17_RTC 例程（串口 + RGBLCD 显示 RTC 时间/日期/星期 + LED 翻转 + USMART）基础上增加：

按一下 KEY0 后进入一次性初始化模式：

1. 串口和屏幕同时显示 `Initializing...` 及输入说明（串口：`请输入当前日期，格式为YY,MM,DD,WW`）；
2. 用户通过串口输入数字串并发送（`\r\n` 结束）后，串口打印 `您输入的是20xx 年 xx 月 xx 日 xx 周`（回显解析结果）；
3. 按相应逻辑完成 RTC 时间（日期）与闹钟的设置；
4. 闹钟设置完毕后，串口和屏幕打印 `Initializing Succeed!`，随后恢复主循环正常运行。

## 2. Non-goals

- 不修改 `.ioc`、链接脚本、CMake 生成块（CMake 变更只通过重新运行 `atk_stm32n6_cmake_prepare.py` 产生）。
- 不修改共享 BSP 源文件（`Drivers/BSP/**`）与共享 Middlewares。
- 不做按键菜单/多步交互、不做屏幕中文显示（当前字体不支持，见 §9）。
- 不改变现有 RTC 周期唤醒（`rtc_set_wakeup`）、USMART、LED 行为的既有逻辑（初始化模式执行期间主循环暂停属预期）。
- 不引入新的串口协议；复用 BSP UART 现有 `\r\n` 帧格式。
- 本阶段不实现、不构建、不烧录。

## 3. Current Project State

- 工程：`Projects/17_RTC`，CubeMX 6.18.0 生成，CMake 工程，`Appli` 上下文；prepare 脚本 v3.4.0 已接入 BSP（HyperRAM/LED/RGBLCD/RTC/SYS/UART）与 USMART 中间件；Debug(SRAM `0x34000400`) 与 Release(XSPI2 `0x70100400`) 均可构建。
- `Appli/Core/Src/main.c`（711 行）主循环当前行为：
  - 每 100 ms 刷新屏幕 Time/Date/Week（`rtc_get_time` / `rtc_get_date` + `rgblcd_show_string`）；
  - 每 200 ms `LED0_TOGGLE()`；`HAL_Delay(10)` 节拍；
  - **当前不包含任何按键处理**（未 include `key.h`，未调用 `key_scan`）。
- 回调（USER CODE 4）：`HAL_TIM_PeriodElapsedCallback`(TIM18→`usmart_dev.scan()`)、`HAL_RTC_AlarmAEventCallback`(打印 `Alarm A!\r\n`)、`HAL_RTCEx_WakeUpTimerEventCallback`(`LED1_TOGGLE()`)。
- KEY BSP 已存在于共享库 `Drivers/BSP/KEY/`，但尚未被本工程 CMake 引用（prepare 检测依赖 `#include "./KEY/key.h"` 出现后再运行脚本即可自动接入）。
- KEY0 引脚 PC6 未在本工程 `MX_GPIO_Init()` 中显式配置（`.ioc` 仅有 RIF 属性项，无 GPIO 模式配置）；BSP `key_init()` 为空函数，引脚依赖复位默认输入态 + 板载上拉。

## 4. Confirmed Interfaces

以下均经源码确认（未编造）：

**KEY**（`Drivers/BSP/KEY/key.h` / `key.c`）
- `uint8_t key_scan(uint8_t mode)`；`mode=0` 不支持连按（内部 10 ms 消抖 + 释放检测）。
- 键值：`NONE_PRES=0`、`WKUP_PRES=1`、`KEY0_PRES=2`、`KEY1_PRES=3`、`KEY2_PRES=4`。
- KEY0 = GPIOC PIN_6，低电平有效（宏 `KEY0==0` 表示按下）。

**UART**（`Drivers/BSP/UART/uart.h` / `uart.c`，USART1 115200 8N1）
- `uart_init(115200)` 内已启动 `HAL_UART_Receive_IT`。
- 接收帧：以 `0x0D 0x0A`（CR LF）结束；完成时 `g_uart_rx_sta & 0x8000` 置位，数据在 `g_uart_rx_buf`，长度 `g_uart_rx_sta & 0x3FFF`，上限 `UART_REC_LEN=200`。
- `printf` 链路：`syscalls.c::_write` → `uart.c::__io_putchar` → USART1（已确认）。

**RTC**（`Drivers/BSP/RTC/rtc.h` / `rtc.c`）
- `rtc_set_date(uint8_t year, uint8_t month, uint8_t date, uint8_t week)`（BCD 年 0-99，week 1-7；`rtc_init` 默认 `(25,1,13,1)` = 2025-01-13 周一，可佐证 WW=1 为周一）。
- `rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second, uint8_t ampm)`（24 小时制，`ampm` 直传 `RTC_HOURFORMAT12_AM` 语义，现有代码传 0）。
- `rtc_set_alarm(uint8_t week, uint8_t hour, uint8_t minute, uint8_t second)`：Alarm A、按星期匹配（`RTC_ALARMDATEWEEKDAYSEL_WEEKDAY`）、`RTC_ALARMMASK_NONE`，触发后回调打印 `Alarm A!`。
- `rtc_get_time` / `rtc_get_date` 用于运行态显示。

**RGBLCD**（`Drivers/BSP/RGBLCD/rgblcd.h`，480×272）
- `rgblcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)`；`rgblcd_clear(uint16_t color)`；颜色宏 `RED/WHITE/BLACK/BLUE…`。
- 字体 `rgblcdfont.h` 仅 ASCII（`asc2_1206/1608/2412/3216`，各 95 字符），**无中文字模**。

**构建集成**
- `Tools/atk_stm32n6_cmake_prepare.py` v3.4.0：`#include "./KEY/key.h"` 出现在 Appli 源码后重跑脚本，KEY 组件会被自动检测并写入 `Appli/CMakeLists.txt` 的 AUTO BSP 块（幂等、marker 保护）。

## 5. Required Behavior

主循环增加一个初始化模式状态机（默认态 = 现有运行行为）：

1. **触发**：运行态每轮循环调用 `key_scan(0)`；返回 `KEY0_PRES` 时进入初始化模式。
2. **进入提示**：
   - 串口打印 `Initializing...` 与 `请输入当前日期，格式为YY,MM,DD,WW`；
   - 屏幕同时显示 `Initializing...` 及输入说明（屏幕用英文等价文案，见 §9 字体限制）。
3. **接收**：等待 UART 完整帧（`g_uart_rx_sta & 0x8000`），解析 `YY,MM,DD,WW` 四个逗号分隔整数；解析后清 `g_uart_rx_sta` 准备下一帧。
4. **回显**：串口打印 `您输入的是20%02d 年 %02d 月 %02d 日 %02d 周`（数值来自解析结果）。
5. **设置**：校验并调用 `rtc_set_date(YY,MM,DD,WW)`，随后按 §7 约定的默认逻辑设置时间与闹钟（`rtc_set_time`、`rtc_set_alarm`）。
6. **完成**：串口与屏幕打印 `Initializing Succeed!`（已确认更正原任务描述中的拼写错误），退出初始化模式，恢复主循环正常运行（时间/日期/星期显示、LED 翻转）。
7. **输入非法**（解析失败或 YY>99 / MM∉1-12 / DD∉1-31 / WW∉1-7）：串口打印错误提示并重新等待输入，不退出初始化模式。
8. 初始化模式执行期间，主循环的屏幕时间刷新与 LED 翻转暂停；USMART 的 TIM18 扫描中断不受影响。

## 6. Files Affected

| 文件 | 变更方式 |
|---|---|
| `Appli/Core/Src/main.c` | 仅 USER CODE 区域：Includes 增 `./KEY/key.h`；PV 增状态变量；USER CODE 0/4 增解析与设置辅助函数；WHILE 增按键扫描与初始化模式分支 |
| `Appli/CMakeLists.txt` | **不手改**；实现后重跑 `python3 ../../Tools/atk_stm32n6_cmake_prepare.py .`，由脚本 AUTO BSP 块自动加入 `KEY` 组件 include/源文件 |
| `Drivers/BSP/KEY/key.c` / `key.h` | 只读引用，不修改 |
| `.ioc` / 链接脚本 / mx-generated.cmake | 不修改 |

## 7. Integration Design

- **状态机**：`typedef enum { APP_STATE_RUN, APP_STATE_SETUP_WAIT_INPUT } app_state_t;` 存于 main.c USER CODE PV。RUN 态 = 现有逻辑 + `key_scan(0)`；检出 `KEY0_PRES` → 输出提示 → 进入 SETUP_WAIT_INPUT。
- **接收与解析**：SETUP 态轮询 `g_uart_rx_sta & 0x8000`；帧到达后复制 `g_uart_rx_buf[0..len)`、清零 `g_uart_rx_sta`，用 `sscanf(buf, "%hhu,%hhu,%hhu,%hhu", &yy,&mm,&dd,&ww)==4` 解析并做范围校验。
- **按键 GPIO**：在 `MX_GPIO_Init()` 的 `USER CODE BEGIN MX_GPIO_Init_2` 区为 PC6 显式配置 `GPIO_MODE_INPUT` + `GPIO_PULLUP`（不改 `.ioc`），消除对复位默认态的隐式依赖。
- **设置逻辑（已与用户确认）**：
  - 日期：`rtc_set_date(YY, MM, DD, WW)`；
  - 时间：`rtc_set_time(8, 0, 0, 0)`（沿用 `rtc_init` 默认 08:00:00）；
  - 闹钟 A：`rtc_set_alarm(WW, 8, 1, 0)`（当周星期 WW、08:01:00，即设置时间 1 分钟后，便于现场验证 `Alarm A!` 打印）。
- **屏幕布局**（480×272，16 号字）：提示行置于现有 `Week:` 行下方（y≈190、210），避免与时间/日期刷新区重叠；完成信息打印后可由后续正常刷新覆盖。
- **串口帧协议**：依赖 BSP 现有 `\r\n` 结束帧；要求串口助手勾选"发送新行"。输入示例：`26,07,28,2`。
- **并发约束**：所有设置 API 均在主循环上下文调用，无 ISR 共享数据（`g_uart_rx_buf` 消费前先清完成标志后拷贝，再清零状态）。

## 8. Constraints

- 只改 `main.c` 的 CubeMX USER CODE 区域；不整文件覆盖。
- 不改 `.ioc`、链接脚本、`mx-generated.cmake`；CMake 只经 prepare 脚本再生。
- 不修改共享 BSP / Middlewares 源文件。
- 复用 BSP UART `\r\n` 帧协议与 200 B 接收上限；输入串超过缓冲区按 BSP 现有行为丢弃（`g_uart_rx_sta` 归零）。
- 串口波特率保持 115200；闹钟触发行为沿用现有 `printf("Alarm A!\r\n")` 回调。
- RTC 已被 RIF 配置为 secure full，本核（secure 上下文）可正常写日期/闹钟（`rtc_init` 已实证）。
- 本规格阶段：禁止实现、构建、连接或烧录开发板。

## 9. Risks and Open Questions

已确认（用户答复 2026-07-28）：

1. **屏幕无法显示中文**：`rgblcdfont.h` 仅 ASCII 字模。`请输入当前日期…` 与 `您输入的是…` 只在串口呈现；屏幕显示英文等价文案（`Initializing...` / `Input date: YY,MM,DD,WW` / `Initializing Succeed!`）。→ **已确认接受**。
2. **拼写更正**：完成信息统一为 `Initializing Succeed!`。→ **已确认**。
3. **时间与闹钟取值**：采用 §7 默认逻辑（时间 08:00:00；闹钟 A = 当周 WW 08:01:00）。→ **已确认**。
4. **"一次性"语义**：一次进入即完成全部设置；运行中再次按 KEY0 可重新进入初始化模式。→ **已确认**。

仍为风险/边界（无需确认，实现与验证时关注）：

5. **KEY0 引脚未经 CubeMX 配置**：依赖复位默认输入 + 板载上拉；§7 已在 USER CODE 区显式配置上拉输入以消除不确定性（低风险，但需实现后实测按键电平）。
6. **解析鲁棒性**：`sscanf` 对多余空格/前导零可接受；非数字或字段缺失走错误重试分支。超长帧（>199 B）被 BSP 丢弃，表现为无响应——属可接受边界。
7. 初始化模式期间若 UART 帧恰好带 USMART 命令，两者共用同一接收缓冲；初始化模式优先消费完整帧，USMART 命令在该窗口内会被吞掉（低风险，例程场景可接受）。

## 10. Verification

- **构建**：实现后在新的隔离目录配置并构建 Debug（`cmake -S Appli -B build/… -G Ninja -DCMAKE_TOOLCHAIN_FILE=… -DCMAKE_BUILD_TYPE=Debug`），要求 0 error；确认 KEY 组件已出现在 AUTO BSP 块；确认 `g_pfnVectors` 仍在 `0x34000400`。
- **静态检查**：`git diff` 仅触及 `main.c` USER CODE 区与 prepare 脚本再生块；`.ioc`/链接脚本无改动。
- **上板验证（需另行授权的 debug 会话）**：SRAM 加载 Debug ELF 后：
  1. 按 KEY0 → 串口与屏幕出现 `Initializing...` 与输入说明；
  2. 串口发送 `26,07,28,2\r\n` → 串口回显 `您输入的是2026 年 07 月 28 日 02 周`；
  3. 屏幕 Time/Date/Week 行随后显示 2026-07-28、Week:2、时间自 08:00:00 起走；
  4. 约 1 分钟后串口打印 `Alarm A!`；
  5. 串口与屏幕出现 `Initializing Succeed!`，LED0 恢复翻转、时间持续刷新；
  6. 发送非法串（如 `abc` 或 `26,13,40,9`）→ 打印错误提示并等待重新输入。

## 11. Acceptance Criteria

1. 按一下 KEY0 后，串口和屏幕同时显示 `Initializing...` 及输入说明（串口含 `请输入当前日期，格式为YY,MM,DD,WW`）。
2. 串口发送合法 `YY,MM,DD,WW` 后，串口打印 `您输入的是20xx 年 xx 月 xx 日 xx 周`（数值与输入一致）。
3. RTC 日期被设置为输入值，时间与闹钟按 §7 确认逻辑完成设置（屏幕日期/星期行与输入一致，闹钟可触发）。
4. 闹钟设置完成后串口与屏幕打印 `Initializing Succeed!`，随后主循环恢复正常运行（时间刷新、LED 翻转）。
5. 非法输入不会写入 RTC，串口给出错误提示并允许重新输入。
6. 改动仅限 `main.c` USER CODE 区与 prepare 脚本再生块；Debug 构建 0 error，向量表地址契约不回归。

## 12. Implementation Plan

1. `main.c` USER CODE Includes：添加 `#include "./KEY/key.h"`。
2. `main.c` USER CODE PTD/PV：添加 `app_state_t` 枚举与状态变量、输入解析缓冲。
3. `main.c` `MX_GPIO_Init` USER CODE 2：配置 PC6 上拉输入。
4. `main.c` USER CODE 0/4：实现辅助函数（提示输出、帧等待与解析校验、回显、日期/时间/闹钟设置、完成输出）。
5. `main.c` USER CODE WHILE：接入 `key_scan(0)` 与初始化模式分支；保持现有刷新/LED 逻辑仅在 RUN 态执行。
6. 重跑 `atk_stm32n6_cmake_prepare.py .`（dry-run 先验证 KEY 被检出）接入 KEY 组件。
7. 隔离目录构建 Debug，修复至 0 error；做 §10 静态检查。
8. （另行授权后）SRAM 加载并按 §10 上板验证清单逐项确认。
