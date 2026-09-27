/**
  ******************************************************************************
  * @file    ems04_s3c_bddiag.h
  * @brief   EMS04 S3-C BD 腿批量诊断回传（qualification-only，开关默认 0）
  ******************************************************************************
  * @attention
  *
  * 背景：S3-C V2 holdout/755 的 BD 腿需逐锚读取 N1 输出（h_past）与 N2 输出码
  * （n2_out/kf）以及 Q27 影子头 CRC。逐锚 halt+调试器读的批式流程在大规模
  * （48/755 锚）下不可行；本模块提供**纯 UART 回传**通道：主机发 CMD_BDDIAG
  * （0x0C），固件把"最近一次 CMD_REATT 的边界缓冲"打包回 RSP_BDDIAG（0x0D）。
  *
  * 性质与纪律：
  *  - 只在 -DEMS04_BDDIAG_UART=1 构建中存在（默认 0 ⇒ 零源文件/零符号/零体积）；
  *  - **只读**边界缓冲与 Q27 影子头，不改任何推理数学/控制流/协议既有命令；
  *  - 非交付件：不得用于生产/正式 soft-HIL 航次；
  *  - 与 Q27 影子（EMS04_Q27_SHADOW）组合时回传真实 Q27 CRC/valid；
  *    未组合时该两字段填 0（不影响 h_past/n2 两列）。
  *
  * RSP_BDDIAG 载荷（666 B，小端；TYPE/LEN/CRC 由 ems_proto 帧层包装）：
  *   off   0 : seq        u32  （回显 CMD 载荷的 seq）
  *   off   4 : q27_crc32  u32  （Q27 影子头 crc32；无影子 = 0）
  *   off   8 : q27_valid  u8    (+3 B 保留 = 0)
  *   off  12 : hpast      96×f32  （rs_n1_hpast，384 B）
  *   off 396 : n2out      90×i8   （rs_n2_out，板侧 N2 输出码）
  *   off 486 : n2kf       180×i8  （rs_n2_kf，板侧 N2 输入 kf 码）
  ******************************************************************************
  */

#ifndef EMS04_S3C_BDDIAG_H
#define EMS04_S3C_BDDIAG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMS04_S3C_BDDIAG_HPAST_N   96u
#define EMS04_S3C_BDDIAG_N2OUT_N   90u
#define EMS04_S3C_BDDIAG_N2KF_N    180u
#define EMS04_S3C_BDDIAG_RSP_LEN   (4u + 4u + 4u + EMS04_S3C_BDDIAG_HPAST_N * 4u \
                                    + EMS04_S3C_BDDIAG_N2OUT_N + EMS04_S3C_BDDIAG_N2KF_N) /* = 666 */

/**
  * @brief  打包最近一次 REATT 的边界缓冲 + Q27 影子头到 rsp（长度必须
  *         >= EMS04_S3C_BDDIAG_RSP_LEN；offset 0 写入 seq 回显）。
  * @param  rsp  输出缓冲
  * @param  seq  主机 seq（原样回显）
  * @note   仅 -DEMS04_BDDIAG_UART=1 构建有实现；调用点亦须同宏包裹。
  */
void ems04_s3c_bddiag_fill(uint8_t *rsp, uint32_t seq);

/* ems_reatt_split.c 提供的只读访问器（同开关包裹定义） */
const float      *ems04_s3c_bddiag_hpast(void);
const signed char *ems04_s3c_bddiag_n2out(void);
const signed char *ems04_s3c_bddiag_n2kf(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_S3C_BDDIAG_H */
