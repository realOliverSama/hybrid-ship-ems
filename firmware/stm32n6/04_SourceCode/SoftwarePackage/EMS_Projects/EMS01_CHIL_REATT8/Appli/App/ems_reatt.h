/* ems_reatt.h — REAT-T 板上预测服务（phase4-g，spec .specs/phase4-g-reatt-board-deployment）。
 *
 * 语义：接收 **原始物理域** hist 窗（[t][ch]，t=0..299 升序，ch 序 =
 * registry INPUT_H1 11 通道）与 KF 窗（[k][ch]，k=0..29 lead 升序，ch 序 =
 * registry INPUT_K+INPUT_F 6 通道），在板上完成：
 *   1. 冻结归一化（ems_reatt_params，formal_norm_v3 逐 bin + norm_ext 逐 lead）；
 *   2. int8 全 CPU 推理（Model/reatt8_q，stai 4.0.1 全 CPU 编译形态）；
 *   3. 单调分位变换 q10=m−softplus(a) / q50=m / q90=m+softplus(b)
 *      （稳定 softplus，D1：TFLite SOFTPRESS 溢出对策）。
 * 输出 q[k][3]（kW 域，[q10,q50,q90] 每 lead），与 keras 科学口径
 * （reatt_s0 + MonotonicQuantileTransform）数值对应（int8 量化差另计）。
 *
 * 三口径契约（AI_MODELING §22.3）：板上喂入域（本函数入口原始域 → 冻结常数
 * 归一化）== 训练域（windows_v3_formal train，frozen transform）== int8 标定域
 * （quantize_reatt.py 标定集取自同域 train 窗归一化后样本）。
 *
 * 失败语义（FR-508 同族）：init/推理失败或输出非有限 → 返回 1（调用方拒绝
 * 该预测，事件环记录）；成功返回 0。
 */
#ifndef EMS_REATT_H
#define EMS_REATT_H

#include <stdint.h>

#define EMS_REATT_HIST_N (300u * 11u)  /* 原始 hist 窗元素数（t 主序 × 11 通道） */
#define EMS_REATT_KF_N   (30u * 6u)    /* 原始 KF 窗元素数（k 主序 × 6 通道） */
#define EMS_REATT_Q_N    (30u * 3u)    /* 输出元素数 [k][{q10,q50,q90}] */

/* 单窗推理。hist_raw/kf_raw/q_out 均按上述元素数；infer_us 返回纯推理耗时
 * （DWT CYCCNT，含归一化与后处理则见 ems_app 遥测口径）。返回 0=OK，1=失败。 */
int ems_reatt_query(const float hist_raw[EMS_REATT_HIST_N],
                    const float kf_raw[EMS_REATT_KF_N],
                    float q_out[EMS_REATT_Q_N],
                    uint32_t *infer_us);

#endif /* EMS_REATT_H */
