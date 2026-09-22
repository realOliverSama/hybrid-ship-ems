/* ems_stai_rt.c — stai runtime 全局一次初始化共享助手（phase4-g，T-005）。
 * 见 ems_stai_rt.h 头注。仅 NPU/tcnq 栈链接（CMake EMS01 AI 块）。
 */
#include <stdbool.h>

#include "stai.h"

#include "ems_stai_rt.h"

static bool ems_stai_rt_inited;

void ems_stai_rt_init_once(void)
{
    if (!ems_stai_rt_inited) {
        (void)stai_runtime_init();
        stai_runtime_set_callback(NULL, NULL);
        ems_stai_rt_inited = true;
    }
}

bool ems_stai_rt_ready(void)
{
    return ems_stai_rt_inited;
}
