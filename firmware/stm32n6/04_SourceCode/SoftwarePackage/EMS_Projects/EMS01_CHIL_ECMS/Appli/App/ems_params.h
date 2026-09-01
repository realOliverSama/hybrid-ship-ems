/* GENERATED FILE — DO NOT EDIT.
 * ems_params.h — EMS01 CHIL ECMS 固件参数（T-503，spec ems01-chil-ecms-v1）
 * 生成器: Host/tools/gen_fw_params.py（单一来源，design.md §Data Model；FR-503/NFR-506）
 * 幂等性: 本文件内容是下列源文件字节的纯函数，重跑生成器逐字节一致；
 *         生成时间不入文件（T-503 幂等验收），由生成器 stdout 记录。
 *
 * 溯源（sha256）:
    selected_model_weights_v2.json   sha256=1d4c8f716c6fec660b0c2f5bca255f0ee197f5f2514039229fd44f234c874758
    fleet_norm_v2.json               sha256=67e8b3bbf96085fbb6555a6a34fa4ab48f604aee200b2730e00a18dfd938fd66
    kS_calibration.json              sha256=a16b0d88da9be50aca974a87848acaf5305a6091c88ba0714287eb547f834b26
    ecms_bsfc_override.mat           sha256=529548d3e835034d200571f07b6de35203ad38cd2c42ec9cb50cdb2117e99bab
    Model_Parameters.m               sha256=830eec47fda301242e8ef4aa6a3165726d79aca8cb67b52c0b023b8863e719d8
 *
 * 布局约定（与 EML eml_ecms_step_mode2.m / selected_model_weights_v2.json 同构）:
 *   Dense 权重 [in x out] 行主序（h = W.' * x + b 语义）;
 *   Conv 权重 [kernel x in_ch x out_ch]; 源值均为 float32 精度的 double。
 * 修改任何常数 = design 修订，须走主 Agent 决策；改源后重跑生成器并以 diff 审。
 */
#ifndef EMS_PARAMS_H
#define EMS_PARAMS_H

#define EMS_PARAMS_WIN_N    300u  /* P_load 滑窗长度（EML:31，300 s @1 Hz） */
#define EMS_PARAMS_FEAT_N   302u  /* = WIN_N + v_ship + accel（fleet_norm_v2 feature_layout） */
#define EMS_PARAMS_BSFC_N   501u  /* ecms_bsfc_override.mat 501 点表（非 7 点默认表） */

/* ---- MLP-Q 302-64-32-3（q10/q50/q90，归一化域输出） ---- */
extern const double EMS_MLPQ_W1[302 * 64];
extern const double EMS_MLPQ_B1[64];
extern const double EMS_MLPQ_W2[64 * 32];
extern const double EMS_MLPQ_B2[32];
extern const double EMS_MLPQ_W3[32 * 3];
extern const double EMS_MLPQ_B3[3];

/* ---- workclf v1 工况识别器（causal Conv1D x3 + MaxPool + GAP + 旁路融合） ---- */
extern const double EMS_CLF_CONV1_W[5 * 1 * 16];
extern const double EMS_CLF_CONV1_B[16];
extern const double EMS_CLF_CONV2_W[3 * 16 * 32];
extern const double EMS_CLF_CONV2_B[32];
extern const double EMS_CLF_CONV3_W[3 * 32 * 32];
extern const double EMS_CLF_CONV3_B[32];
extern const double EMS_CLF_FC1_W[2 * 8];
extern const double EMS_CLF_FC1_B[8];
extern const double EMS_CLF_FC2_W[40 * 64];
extern const double EMS_CLF_FC2_B[64];
extern const double EMS_CLF_FC3_W[64 * 7];
extern const double EMS_CLF_FC3_B[7];

/* ---- 归一化（fleet_norm_v2.json，train-split 统计） ---- */
extern const double EMS_NORM_X_MEAN[302];
extern const double EMS_NORM_X_STD[302];
extern const double EMS_NORM_Y_MEAN;
extern const double EMS_NORM_Y_STD;

/* ---- kS 标定常数（kS_calibration.json，T-106 v2） ---- */
typedef struct {
    double lambda0;        /* λ₀ [kg/kWh] */
    double k_soc;          /* K_soc [kg/kWh/SOC] */
    double k_s;            /* K_S [-]（K_S≠0 时 λ 下限钳位生效，EML:173） */
    double lambda_floor;   /* Λ_floor [kg/kWh] */
    double soc_ref;        /* SOC_ref [-] */
    double ebar_h_kwh;     /* Ē_H [kWh] */
    double e_scale_kwh;    /* E_scale [kWh] */
} ems_ks_t;
extern const ems_ks_t EMS_KS;

/* ---- 门控常数（selected_model_weights_v2.json:gate，与 EML:2-3 同源） ---- */
typedef struct {
    double s1_kwh;         /* GATE_S1 = val P75 [kWh]（禁调参） */
    double s2_kwh;         /* GATE_S2 = val P95 [kWh]（禁调参） */
    int    m_ticks;        /* GATE_M：连续同类 tick 数才切换（EML:143，3 tick = 3 s） */
    int    upstream_class; /* 可辨识受限类 id（=6，EML:149） */
} ems_gatecst_t;
extern const ems_gatecst_t EMS_GATE;

/* ---- plant/ECMS 常数（Model_Parameters.m 人工核对清单，design §Data Model） ---- */
typedef struct {
    double eta_rect;         /* 发电机整流效率 [-] */
    double eta_dcdc;         /* 电池 DC/DC 效率 [-] */
    double bat_eff_dis;      /* 电池放电效率 [-] */
    double bat_eff_chg;      /* 电池充电效率 [-] */
    double soc_discharge_max;/* SOC ≤ 此值禁放电（硬约束，EML:183） */
    double soc_charge_min;   /* SOC ≥ 此值禁充电（硬约束，EML:190） */
    double idle_fuel_kgph;   /* 机组怠速燃油 [kg/h]（EML:194，/3600 → kg/s） */
    double grid_dp_kw;       /* ECMS 网格步长 [kW] */
    double gen_p_max_kw;     /* 发电机聚合上限 [kW] */
    double pgen_init_kw;     /* Pheld 初值 [kW]（EML:29） */
    double dt_ctrl_s;        /* 控制周期 [s]（EML:47 tick 门） */
} ems_plant_t;
extern const ems_plant_t EMS_PLANT;

/* ---- BSFC override 表（g/kWh，bp 严格递增 kW） ---- */
extern const double EMS_BSFC_BP[501];
extern const double EMS_BSFC_TBL[501];

#endif /* EMS_PARAMS_H */
