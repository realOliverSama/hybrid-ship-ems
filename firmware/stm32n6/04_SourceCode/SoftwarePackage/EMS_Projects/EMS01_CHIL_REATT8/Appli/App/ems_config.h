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

/* phase4-g（D6）：USART1 运行时波特率覆盖（main.c USER CODE 2）。
 * .ioc 保持 115200（CubeMX 生成态不动）；本工程默认 921600 承载
 * CMD_REATT 13924 B 长帧（115200 下 1.21 s/帧不可接受）。
 * 主机工具默认同值（Host/tools/reatt_board_client.py --baud）。
 * 链路不稳时可 -DEMS_REATT_UART_BAUD=115200 回退（协议字节流不变）。 */
#ifndef EMS_REATT_UART_BAUD
#define EMS_REATT_UART_BAUD 921600
#endif

#endif /* EMS_CONFIG_H */
