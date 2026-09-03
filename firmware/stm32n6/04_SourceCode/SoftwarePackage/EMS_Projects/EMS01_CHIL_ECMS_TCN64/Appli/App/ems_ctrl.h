/* ems_ctrl.h — ECMS 控制律核心（EML eml_ecms_step_mode2.m 函数 ecms_nn 的 C 移植，
 * T-503，spec ems01-chil-ecms-v1；design §Data Model §滑窗与控制状态）。
 *
 * 语义基准：WS/results/ecms_phase25/gating/eml_ecms_step_mode2.m（下称 EML，
 * 行号注释即该文件）。移植原则：逐条对应、不做任何"优化"（平 fill、t<lastT 全复位、
 * NaN 跳写指针前进、argmax/argmin 平局取首、SOC≤0.105 首个可行 Pg break + 末尾救急、
 * 迟滞 3-tick、BSFC 端点钳位与严格小于插值、mf/mbe 单位链 3.6e6 与 3600 混用均保留）。
 *
 * 平台无关：无 HAL、无动态内存、double 数学；同一 .c 编入固件也被主机 gcc 编译做 L0 单测。
 * EML→C 语义映射总表见 ems_ctrl.c 文件头注释。
 */
#ifndef EMS_CTRL_H
#define EMS_CTRL_H

#include "ems_config.h"
#include "ems_params.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EMS_CTRL_WIN_N   ((int)EMS_PARAMS_WIN_N)   /* 300 点滑窗（EML:31） */
#define EMS_CTRL_FEAT_N  ((int)EMS_PARAMS_FEAT_N)  /* 302 = 300 + v_ship + accel */

/* 门控迟滞状态机状态（EML:34/42/136-148；-1 = 无状态） */
typedef struct {
    int cls_eff;   /* 生效类别（EML cls_eff） */
    int run_cls;   /* 候选类别（EML run_cls） */
    int run_cnt;   /* 候选连续计数（EML run_cnt） */
} ems_ctrl_gate_sm_t;

/* 控制器持久状态（EML persistent：lastT/Pheld/buf/k/vprev/cls_eff/run_cls/run_cnt） */
typedef struct {
    int    initialized;  /* C 移植附加：EML isempty(lastT) 首调初始化的等价物 */
    double lastT;
    double Pheld;
    double buf[EMS_CTRL_WIN_N];
    int    k;            /* 0 基写指针；EML k 为 1 基（mod 300） */
    double vprev;
    ems_ctrl_gate_sm_t gate;
} ems_ctrl_t;

/* 单拍输入（EML:1 的动态信号；常数经 ems_params 注入）。nn_ok 对应 EML 输入
 * ECMS_NN_WeightsOK（>0.5 判真，EML:57）；固件侧由完整性/回退状态机驱动。 */
typedef struct {
    double t;
    double P_load;
    double v_ship;
    double SOC;
    int    nn_ok;
} ems_ctrl_in_t;

/* 单拍输出（EML 只返回 P；其余为 FR-509 遥测与 L0 观测面） */
typedef struct {
    int    held;        /* 1 = tick 门早退，返回保持值（EML:46-49），其余字段无效 */
    int    ok;          /* 0 = 推理失败回退 ê=0（EML:57/166-169） */
    double P;            /* P_gen_ref [kW]（EML 返回值） */
    double e_hat;        /* 门控后 ê（EML:165/168） */
    double e_hat_raw;    /* 门控前 ê（EML:69） */
    double q3_kwh[3];    /* [q10;q50;q90] 反归一化 [kWh]（EML:68） */
    double s_q;          /* q90-q10 [kWh]（EML:72） */
    double K_q;          /* G1 分位数门控增益（EML:73-79） */
    double K;            /* GATE_MODE 合成增益（EML:156-165） */
    int    K_w;          /* G2 识别器门控增益 0/1（EML:149-153） */
    int    cls_eff;      /* 迟滞后生效类别（EML cls_eff；-1=无状态） */
    int    cls_raw;      /* 识别器原始类别 1..7（EML:134） */
    int    cls_invalid;  /* 1 = 识别输出越界 [1,7]（design Error Handling：按非
                            upstream K=1 处理、跳过迟滞更新并记事件，不误关预测） */
} ems_ctrl_out_t;

/* 复位控制状态：下一 tick 走 EML:27-35 首调初始化 = 平 fill（协议 CMD_RESET 语义） */
void ems_ctrl_init(ems_ctrl_t *st);

/* 单控制拍：完整 EML ecms_nn 一步。返回 P_gen_ref（同 out->P）。 */
double ems_ctrl_step(ems_ctrl_t *st, const ems_ctrl_in_t *in, ems_ctrl_out_t *out);

/* ---------------- 移植保真缝合面（供 L0 白盒单测，生产代码勿另行调用） ----------------
 * 以下函数将 EML 内联片段显式命名为独立函数，语义与行号一一对应；
 * 独立成函数是 L0 单元用例（scout 风险点逐条复现）的白盒入口，不改变任何数值语义。 */

/* EML:136-148 迟滞状态机（首判直接采纳/同类复位计数/连续 EMS_GATE.m_ticks 切换） */
void ems_ctrl_hysteresis_step(ems_ctrl_gate_sm_t *g, int cls);

/* EML:149-153：cls_eff == upstream(6) → K_w=0（预测关断），否则 1 */
int ems_ctrl_gate_kw(int cls_eff);

/* EML:72-79：s_q ≤ S1 → 1；s_q ≥ S2 → 0；其间线性 */
double ems_ctrl_gate_kq(double s_q);

/* EML:156-164：GATE_MODE 合成（mode 由 ems_config.h EMS_GATE_MODE 编译期给定） */
double ems_ctrl_gate_combine(int mode, double k_q, int k_w);

/* EML:196-207：BSFC 端点钳位 + `while bp(ii+1) < Pg` 严格小于分段 + 线性插值 */
double ems_ctrl_bsfc_at(const double *bp, const double *tbl, int nb, double Pg);

/* EML:176-222 网格穷举：J = mf + λ·mbe；SOC≤Discharge_Max 首个可行 Pg break；
 * SOC≥Charge_Min 且 Pbat<0 禁充电；末尾救急分支。常数/表经结构体注入（白盒可换
 * 合成表验证平局/插值/单位链；生产路径用 ems_params 装填，常数不变）。 */
typedef struct {
    double eta_rect, eta_dcdc, bat_eff_dis, bat_eff_chg;
    double soc_discharge_max, soc_charge_min;
    double idle_fuel_kgph, grid_dp_kw, gen_p_max_kw;
    const double *bsfc_bp;
    const double *bsfc_tbl;
    int    bsfc_n;
} ems_ctrl_ecms_cst_t;

double ems_ctrl_grid_search(const ems_ctrl_ecms_cst_t *c, double P_load,
                            double SOC, double lam);

/* EML:134 的 MATLAB max 语义：平局取首个（严格大于才替换） */
int ems_ctrl_argmax_first(const double *v, int n);

#ifdef __cplusplus
}
#endif

#endif /* EMS_CTRL_H */
