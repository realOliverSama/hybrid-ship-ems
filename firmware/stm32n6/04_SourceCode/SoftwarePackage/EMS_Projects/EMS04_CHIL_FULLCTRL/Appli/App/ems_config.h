/* ems_config.h — EMS01 CHIL ECMS 编译开关（T-503）。
 * design §工程目录：EMS_FW_MODE ∈ {REF, NPU}；REF = ems_infer_ref.c（CPU double），
 * NPU = ems_infer_npu.c（LL_ATON，T-505 交付）。两后端同签名，见 ems_infer.h。
 */
#ifndef EMS_CONFIG_H
#define EMS_CONFIG_H

#define EMS_FW_MODE_REF 1
#define EMS_FW_MODE_NPU 2

#ifndef EMS_FW_MODE
#define EMS_FW_MODE EMS_FW_MODE_REF
#endif

/* GATE_MODE：EML:1 固定 GATE_MODE=2（G2 工况识别门控）。
 * 0=G0 无门控 / 1=G1 分位数 / 2=G2 识别器 / 3=G3 乘积（EML:155-164）。
 * 编译期测试缝：L0 单测以 -DEMS_GATE_MODE=n 覆盖 G0~G3 的 K 计算；
 * 默认值 2 = EML 语义，不得改动。 */
#ifndef EMS_GATE_MODE
#define EMS_GATE_MODE 2
#endif

/* phase4-g（D6 退役，2026-09-29）：EMS_REATT_UART_BAUD 运行时覆盖机制退役，
 * USART1 波特率改由 .ioc 主导——EMS01_CHIL_ECMS.ioc 设 USART1.BaudRate=921600，
 * MX_USART1_UART_Init 生成行即 921600（main.c 不再有运行时重初始化）。
 * 历史登记（保留教训，不得静默丢弃）：
 *  - 本宏原经 Appli/CMakeLists.txt target_compile_definitions 接线（cache 变量
 *    EMS_REATT_UART_BAUD，默认 115200）；main.c 旧覆盖块注释曾误写"默认 921600"
 *    造成文档漂移（实际默认 115200），本次一并修正。
 *  - 921600 在板载 CH340 调试链路实测不可靠（高速垃圾字节流）——该结论针对
 *    当时 debug 直连形态；现 host 工具已波特率参数化（--baud，默认 115200），
 *    921600 主导后 host 侧需以 --baud 921600 对接。
 *  - CMakeLists.txt 的 -DEMS_REATT_UART_BAUD 接线暂保留（宏已无消费方，
 *    无害的孤立 -D），后续 CMake 清理时移除。
 * 协议字节流与波特率无关。 */

#endif /* EMS_CONFIG_H */
