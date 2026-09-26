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

/* phase4-g（D6，2026-09-22 bring-up 修正）：USART1 运行时波特率覆盖。
 * ⚠ 事故教训：本宏必须经 CMake target_compile_definitions 接线
 * （cache 变量 EMS_REATT_UART_BAUD），此前只改 cache 不传编译定义，
 * #ifndef 兜底默认静默生效——板上 bring-up 全程被未接线的开关带偏。
 * 默认 115200（板载 CH340 链路久经验证；921600 实测 CH340 不可靠：
 * 高速垃圾字节流）。13924B 长帧代价 = 1.21s/帧 × ~391 锚点/航次 ≈ +8min，
 * 可接受。协议字节流与波特率无关。 */
#ifndef EMS_REATT_UART_BAUD
#define EMS_REATT_UART_BAUD 115200
#endif

#endif /* EMS_CONFIG_H */
