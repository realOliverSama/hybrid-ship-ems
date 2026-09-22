/* ems_infer.h — 推理后端接口（design §API/Interface Changes，FR-503/R8）。
 *
 * 两后端同签名，经 ems_config.h 的 EMS_FW_MODE 编译期选择：
 *   REF 后端 ems_infer_ref.c — CPU double 前向（T-503，与 EML 逐条对应）；
 *   NPU 后端 ems_infer_npu.c — LL_ATON int8（T-505）。
 *
 * 职责边界（design R8，防双后端代码漂移）：后端只做张量前向。
 * 预处理（滑窗/归一化/Pdec 抽取）只在 ems_ctrl.c 实现一份：
 *   - mlpq 输入 x302 为归一化后窗口（ems_ctrl 完成 (x-mean)/std）；
 *   - workclf 输入为同一 xnorm 的切片（Pdec 10s 重采样 30 点 + v_ship + accel 的
 *     归一化值），与 T-312 训练口径一致（2026-09 口径修订；修订前为原始物理量，
 *     导致 int8 输入量化饱和坝缩，见 docs/agent/AI_MODELING.md §22）。
 *
 * 返回值：0 = 成功；非 0 = 推理失败（触发 ems_ctrl 的 ê=0 回退，design Error Handling）。
 * us：推理耗时 [μs] 输出（DWT CYCCNT 由目标侧封装；REF/host 侧写 0），可为 NULL。
 * 平台无关：无 HAL 调用、无动态内存、可被主机 gcc 编译（L0 单测）。
 */
#ifndef EMS_INFER_H
#define EMS_INFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MLP-Q 前向：x302（归一化域）→ q3[3] = [q10; q50; q90]（仍为归一化域，
 * 反归一化 q*Y_STD+Y_MEAN 属 ems_ctrl，对应 EML:64-68）。 */
int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us);

/* workclf 前向：pdec30（归一化域 Pdec 30 点切片，xnorm(1:10:300)）+ 归一化
 * [v_ship, accel]（xnorm[300:302]）→ cls ∈ 1..7
 * （1..7，MATLAB max 语义平局取首，EML:134）。out_logits[7] 可为 NULL：非 NULL 时
 * 回填 7 类 logits（NPU 后端 = 内部 clf_out；REF 后端 = ocl 前向值 f32 截断），
 * 供 CMD_WC_TEST（0x08）回显与 T-507 类别一致率测量；控制律路径传 NULL 不变。 */
int ems_infer_workclf(const double pdec30[30], double v, double a, int *cls,
                      float out_logits[7], uint32_t *us);

/* ---- T-506 FR-505 遥测：上一控制拍 mlpq+workclf 合计推理时长 [μs] ----
 * 调用方在 ems_ctrl_step 前调 ems_infer_last_us_reset()，控制拍完成后读
 * ems_infer_last_us()（RSP_TICK infer_us 字段，ems_app.c）。两后端同名导出：
 *   NPU 后端 = DWT CYCCNT 实测累加（mlpq + workclf 各自贡献）；
 *   REF 后端无独立计数（板上 REF 时延由 ctrl_us 全量覆盖），恒返回 0 ——
 *   这是后端差异语义（非口径变更），已记录于 T-506 任务报告。 */
void ems_infer_last_us_reset(void);
uint32_t ems_infer_last_us(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS_INFER_H */
