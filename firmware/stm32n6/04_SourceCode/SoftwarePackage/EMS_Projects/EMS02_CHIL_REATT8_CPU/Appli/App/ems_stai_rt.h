/* ems_stai_rt.h — stai runtime 全局一次初始化共享助手（phase4-g，T-005）。
 *
 * 背景：ems_infer_npu.c（tcnq/workclf 槽位）与 ems_reatt.c（REAT-T 服务）各自
 * 懒初始化，但 NetworkRuntime1201 的 stai_runtime_init 契约为**全局一次**
 * （main_stai.c 同构）。原先“各文件私有 once 标志”在两路都被触达时会双跑
 * runtime init；本助手把 once 状态收敛到单一 TU（spec phase4-g D5 配套）。
 * REF 镜像（EMS_FW_MODE=REF）不链 stai 栈，ems_reatt/ems_infer_ref 不受影响。
 */
#ifndef EMS_STAI_RT_H
#define EMS_STAI_RT_H

#include <stdbool.h>

/* stai_runtime_init 首次调用执行（含 stai_runtime_set_callback(NULL, NULL)），
 * 重复调用为空操作。线程语境：单任务顺序调用（与既有懒初始化语境一致）。 */
void ems_stai_rt_init_once(void);

/* 诊断：是否已初始化 */
bool ems_stai_rt_ready(void);

#endif /* EMS_STAI_RT_H */
