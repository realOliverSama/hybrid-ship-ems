/* ems_ctrl.c — ECMS 控制律核心：EML eml_ecms_step_mode2.m（函数 ecms_nn）逐条 C 移植。
 * T-503，spec ems01-chil-ecms-v1。语义基准行号 = WS/results/ecms_phase25/gating/
 * eml_ecms_step_mode2.m（下称 EML）。double 数学、平台无关、禁动态内存。
 *
 * ============================ EML 行号 → C 语义映射表 ============================
 * EML:1      函数签名 → ems_ctrl_in_t（4 动态信号 + nn_ok）+ ems_params 常数注入
 * EML:2-3    GATE_MODE=2/GATE_M=3/GATE_S1/S2 → ems_config.h EMS_GATE_MODE（默认 2）
 *            + EMS_GATE（生成物，selected_model_weights_v2.json:gate 同源核对）
 * EML:4-25   识别器/融合权重内联字面量 → ems_params.c 生成物（JSON 全精度 double）
 * EML:26-35  persistent 初始化（lastT=-1e9、Pheld=Pgen_init、k=1、buf 平 fill P_load、
 *            vprev=v_ship、cls_eff=run_cls=-1、run_cnt=0）→ ems_ctrl_step() 首调块
 * EML:36-43  t<lastT 新仿真复位（平 fill/指针/vprev/门控状态；lastT=t → 复位拍本身
 *            落入 EML:47 tick 门返回 Pheld）→ ems_ctrl_step() 复位块（顺序保持）
 * EML:46-49  P=Pheld + tick 门（t-lastT < dt-1e-9 早退）→ ems_ctrl_step() 早退
 * EML:50     lastT=t → 同上
 * EML:51-54  isfinite(P_load) 才写 buf；k=mod(k,300)+1 无条件前进 → 写块
 * EML:55-56  accel=v_ship-vprev; vprev=v_ship（在 ok 判定之外，NaN 也推进）
 * EML:57     ok = nn_ok>0.5 && isfinite(P_load,v_ship,accel)
 * EML:60-62  xP 升序环回展开 + x=[xP;v;accel] + all(isfinite(x))
 * EML:64     xnorm=(x-XMean)./XStd → ems_ctrl_step()（归一化只在 ctrl，design R8）
 * EML:65-67  MLP-Q 前向（W.'*x+b 语义，输出层无 ReLU）→ ems_infer_mlpq()
 * EML:68     Ehat_q = y_norm*YStd+YMean → ems_ctrl_step()
 * EML:69     e_hat=(Ehat_q(2)-Ebar)/Escale（q50）
 * EML:72-79  G1 分位数门控 K_q → ems_ctrl_gate_kq()
 * EML:82     Pdec = xP(1:10:300) → ems_ctrl_step()
 * EML:83-134 workclf 前向（因果 conv 左补零 k5/k3/k3、MaxPool 丢尾 30→15→7、GAP、
 *            旁路融合 [gfeat;s8]、argmax 平局取首）→ ems_infer_workclf()
 * EML:136-148 迟滞状态机 → ems_ctrl_hysteresis_step()
 * EML:149-153 K_w（cls_eff==6 → 0 否则 1）→ ems_ctrl_gate_kw()
 * EML:155-164 K 合成（G0~G3）→ ems_ctrl_gate_combine()
 * EML:165    e_hat = K*e_hat
 * EML:166-169 回退 ê=0（≡law=5；门控状态本拍不动）→ ems_ctrl_step()
 * EML:171-175 soc_ref_eff/λ 自适应/Λ_floor 钳位（K_S≠0 时）→ ems_ctrl_step()
 * EML:176-179 N=floor(Pmax/dP)+1、Pbest=0、Jbest=inf、nb → ems_ctrl_grid_search()
 * EML:180-181 Pg=(i-1)*dP
 * EML:182    Pbat=P_load-η_rect·Pg
 * EML:183-189 SOC≤Discharge_Max：Pbat>1e-9 continue；首个可行 Pg 赋 Pbest 并 break
 * EML:190-191 SOC≥Charge_Min 且 Pbat<0 → continue（禁充电）
 * EML:193-194 Pg<1e-9 → mf=Idle/3600（[kg/h]→[kg/s]，3600 不得"统一"）
 * EML:196-206 BSFC 端点钳位 + 严格小于分段 → ems_ctrl_bsfc_at()
 * EML:207    mf=Pg·bsfc/3.6e6（[kW]·[g/kWh]→[kg/s]，3.6e6 不得"统一"）
 * EML:209-213 mbe：放电 /(η_dcdc·η_dis)/3600；充电 ·(η_dcdc·η_chg)/3600（负值）
 * EML:214    J = mf + λ·mbe
 * EML:215-218 J<Jbest 严格小于 → argmin 平局取首
 * EML:220-222 末尾救急：SOC≤Discharge_Max 且 Pbest<1e-9 且 P_load>η_rect·Pmax
 *            → Pbest=Pmax
 * EML:223-224 P=Pbest; Pheld=Pbest
 * ===============================================================================
 */
#include "ems_ctrl.h"

#include <math.h>
#include <stddef.h>

#include "ems_infer.h"

void ems_ctrl_init(ems_ctrl_t *st)
{
    st->initialized = 0; /* 下一 tick 走 EML:27-35 首调初始化 = 平 fill */
}

/* EML:136-148 */
void ems_ctrl_hysteresis_step(ems_ctrl_gate_sm_t *g, int cls)
{
    if (g->cls_eff < 0) {                    /* 首个判决直接采纳（EML:137-138） */
        g->cls_eff = cls;
        g->run_cls = cls;
        g->run_cnt = 0;
    } else if (cls == g->cls_eff) {          /* 同类：复位候选计数（EML:139-140） */
        g->run_cls = cls;
        g->run_cnt = 0;
    } else if (cls == g->run_cls) {          /* 连续同类累加（EML:141-145） */
        g->run_cnt = g->run_cnt + 1;
        if (g->run_cnt >= EMS_GATE.m_ticks) {
            g->cls_eff = cls;
            g->run_cnt = 0;
        }
    } else {                                 /* 新候选：计数从 1 起（EML:146-147） */
        g->run_cls = cls;
        g->run_cnt = 1;
    }
}

/* EML:149-153 */
int ems_ctrl_gate_kw(int cls_eff)
{
    return (cls_eff == EMS_GATE.upstream_class) ? 0 : 1;
}

/* EML:72-79 */
double ems_ctrl_gate_kq(double s_q)
{
    if (s_q <= EMS_GATE.s1_kwh) {
        return 1.0;
    }
    if (s_q >= EMS_GATE.s2_kwh) {
        return 0.0;
    }
    return (EMS_GATE.s2_kwh - s_q) / (EMS_GATE.s2_kwh - EMS_GATE.s1_kwh);
}

/* EML:155-164（mode 语义见 ems_config.h） */
double ems_ctrl_gate_combine(int mode, double k_q, int k_w)
{
    if (mode == 0) {
        return 1.0;
    }
    if (mode == 1) {
        return k_q;
    }
    if (mode == 2) {
        return (double)k_w;
    }
    return k_q * (double)k_w; /* mode == 3（G3） */
}

/* EML:196-207（端点钳位与插值公式保持 EML 运算树，含节点处 lerp 的舍入行为） */
double ems_ctrl_bsfc_at(const double *bp, const double *tbl, int nb, double Pg)
{
    double bsfc = tbl[0];                    /* EML:196 */
    if (Pg >= bp[nb - 1]) {                  /* EML:197-198 端点钳位（上） */
        bsfc = tbl[nb - 1];
    } else if (Pg > bp[0]) {                 /* EML:199 Pg≤bp(1) 保持 tbl(1) 钳位（下） */
        int ii = 0;                          /* EML:200 ii 为 1 基；C 侧 0 基 */
        while (ii < nb - 2 && bp[ii + 1] < Pg) { /* EML:201 严格小于 */
            ii = ii + 1;
        }
        double w = (Pg - bp[ii]) / (bp[ii + 1] - bp[ii]); /* EML:204 */
        bsfc = tbl[ii] + w * (tbl[ii + 1] - tbl[ii]);     /* EML:205 */
    }
    return bsfc;
}

/* EML:176-222（表达式运算树逐条对应，mf/mbe 单位链 3.6e6 与 3600 不得统一） */
double ems_ctrl_grid_search(const ems_ctrl_ecms_cst_t *c, double P_load,
                            double SOC, double lam)
{
    const int N = (int)floor(c->gen_p_max_kw / c->grid_dp_kw) + 1; /* EML:176 */
    double Pbest = 0.0;                                            /* EML:177 */
    double Jbest = INFINITY;                                       /* EML:178 */
    const int nb = c->bsfc_n;                                      /* EML:179 */

    for (int i = 0; i < N; i++) {                        /* EML:180 */
        const double Pg = (double)i * c->grid_dp_kw;     /* EML:181（0 基等价 i-1） */
        const double Pbat = P_load - c->eta_rect * Pg;   /* EML:182 */

        if (SOC <= c->soc_discharge_max) {               /* EML:183 禁放电硬约束 */
            if (Pbat > 1e-9) {                           /* EML:184-185 */
                continue;
            }
            Pbest = Pg;                                  /* EML:187 首个可行 Pg */
            break;                                       /* EML:188 break（非 J 最优） */
        }
        if (SOC >= c->soc_charge_min && Pbat < 0.0) {    /* EML:190-191 禁充电 */
            continue;
        }

        double mf;
        if (Pg < 1e-9) {                                 /* EML:193-194 怠速 */
            mf = c->idle_fuel_kgph / 3600.0;             /* [kg/h]→[kg/s] */
        } else {
            const double bsfc = ems_ctrl_bsfc_at(c->bsfc_bp, c->bsfc_tbl, nb, Pg);
            mf = Pg * bsfc / 3.6e6;                      /* EML:207 [kg/s] */
        }

        double mbe;
        if (Pbat > 0.0) {                                /* EML:209-210 放电 */
            mbe = Pbat / (c->eta_dcdc * c->bat_eff_dis) / 3600.0;
        } else {                                         /* EML:211-212 充电（负值） */
            mbe = Pbat * (c->eta_dcdc * c->bat_eff_chg) / 3600.0;
        }

        const double J = mf + lam * mbe;                 /* EML:214 */
        if (J < Jbest) {                                 /* EML:215 严格小于=平局取首 */
            Jbest = J;
            Pbest = Pg;
        }
    }

    if (SOC <= c->soc_discharge_max && Pbest < 1e-9 &&
        P_load > c->eta_rect * c->gen_p_max_kw) {        /* EML:220-221 末尾救急 */
        Pbest = c->gen_p_max_kw;
    }
    return Pbest;                                        /* EML:223 */
}

/* EML:134 的 MATLAB max：平局取首个（严格大于才替换） */
int ems_ctrl_argmax_first(const double *v, int n)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (v[i] > v[best]) {
            best = i;
        }
    }
    return best; /* 0 基 */
}

double ems_ctrl_step(ems_ctrl_t *st, const ems_ctrl_in_t *in, ems_ctrl_out_t *out)
{
    /* ---- EML:27-35 首调初始化（平 fill 禁止零填充） ---- */
    if (!st->initialized) {
        st->lastT = -1e9;                                /* EML:28 */
        st->Pheld = EMS_PLANT.pgen_init_kw;              /* EML:29 */
        st->k = 0;                                       /* EML:30（1 基 k=1） */
        for (int i = 0; i < EMS_CTRL_WIN_N; i++) {
            st->buf[i] = in->P_load;                     /* EML:31-32 平 fill */
        }
        st->vprev = in->v_ship;                          /* EML:33 */
        st->gate.cls_eff = -1;                           /* EML:34 */
        st->gate.run_cls = -1;
        st->gate.run_cnt = 0;
        st->initialized = 1;
    }

    /* ---- EML:36-43 新仿真复位（复位拍 lastT=t → 落入 tick 门返回 Pheld） ---- */
    if (in->t < st->lastT) {
        st->lastT = in->t;                               /* EML:38 */
        st->k = 0;                                       /* EML:39 */
        for (int i = 0; i < EMS_CTRL_WIN_N; i++) {
            st->buf[i] = in->P_load;                     /* EML:40 平 fill */
        }
        st->vprev = in->v_ship;                          /* EML:41 */
        st->gate.cls_eff = -1;                           /* EML:42 */
        st->gate.run_cls = -1;
        st->gate.run_cnt = 0;
    }

    /* ---- EML:46-49 输出保持 + tick 门 ---- */
    double P = st->Pheld;                                /* EML:46 */
    if (in->t - st->lastT < EMS_PLANT.dt_ctrl_s - 1e-9) {
        out->held = 1;
        out->ok = 1;
        out->P = P;
        return P;
    }
    st->lastT = in->t;                                   /* EML:50 */

    /* ---- EML:51-54 NaN 跳写、指针无条件前进 ---- */
    if (isfinite(in->P_load)) {
        st->buf[st->k] = in->P_load;                     /* EML:52 */
    }
    st->k = (st->k + 1) % EMS_CTRL_WIN_N;                /* EML:54 */

    /* ---- EML:55-56 accel 差分（ok 判定之外，NaN 也推进基线） ---- */
    const double accel = in->v_ship - st->vprev;
    st->vprev = in->v_ship;

    /* ---- EML:57 ok 判定 ---- */
    int ok = (in->nn_ok != 0) && isfinite(in->P_load) &&
             isfinite(in->v_ship) && isfinite(accel);

    /* ---- EML:60-62 滑窗升序展开 + x 构造 + 全元素有限校验 ---- */
    double xP[EMS_CTRL_WIN_N];
    int idx = 0;
    for (int i = st->k; i < EMS_CTRL_WIN_N; i++) { /* EML buf(k:300)，写指针已前进 */
        xP[idx++] = st->buf[i];
    }
    for (int i = 0; i < st->k; i++) {              /* EML buf(1:k-1) */
        xP[idx++] = st->buf[i];
    }
    double x[EMS_CTRL_FEAT_N];
    for (int i = 0; i < EMS_CTRL_WIN_N; i++) {
        x[i] = xP[i];
    }
    x[EMS_CTRL_WIN_N] = in->v_ship;
    x[EMS_CTRL_WIN_N + 1] = accel;
    for (int i = 0; i < EMS_CTRL_FEAT_N; i++) {
        if (!isfinite(x[i])) {
            ok = 0;
            break;
        }
    }

    /* ---- 观测面清零（EML 无对应输出；held 之外的默认值仅遥测用） ---- */
    out->held = 0;
    out->e_hat = 0.0;
    out->e_hat_raw = 0.0;
    out->q3_kwh[0] = 0.0;
    out->q3_kwh[1] = 0.0;
    out->q3_kwh[2] = 0.0;
    out->s_q = 0.0;
    out->K_q = 1.0;
    out->K = 1.0;
    out->K_w = 1;
    out->cls_eff = st->gate.cls_eff;
    out->cls_raw = 0;
    out->cls_invalid = 0;

    double e_hat = 0.0;
    if (ok) {
        /* ---- EML:64 归一化（只在 ctrl 实现一份，design R8） ---- */
        double xnorm[EMS_CTRL_FEAT_N];
        for (int i = 0; i < EMS_CTRL_FEAT_N; i++) {
            xnorm[i] = (x[i] - EMS_NORM_X_MEAN[i]) / EMS_NORM_X_STD[i];
        }

        /* ---- EML:65-67 MLP-Q 前向（后端只做张量前向，design R8） ---- */
        double q3_norm[3];
        if (ems_infer_mlpq(xnorm, q3_norm, NULL) != 0) {
            ok = 0; /* 推理失败 → ê=0 回退（design Error Handling） */
        }

        if (ok) {
            for (int i = 0; i < 3; i++) {
                if (!isfinite(q3_norm[i])) {
                    ok = 0; /* 输出非有限 → 回退 */
                }
            }
        }

        if (ok) {
            /* ---- EML:68-69 反归一化 + ê（q50 通道） ---- */
            const double e_hat_raw =
                (q3_norm[1] * EMS_NORM_Y_STD + EMS_NORM_Y_MEAN - EMS_KS.ebar_h_kwh) /
                EMS_KS.e_scale_kwh;

            /* ---- EML:71-79 G1 分位数门控 ---- */
            const double q10 = q3_norm[0] * EMS_NORM_Y_STD + EMS_NORM_Y_MEAN;
            const double q50 = q3_norm[1] * EMS_NORM_Y_STD + EMS_NORM_Y_MEAN;
            const double q90 = q3_norm[2] * EMS_NORM_Y_STD + EMS_NORM_Y_MEAN;
            const double s_q = q90 - q10;                    /* EML:72 */
            const double k_q = ems_ctrl_gate_kq(s_q);

            /* ---- EML:81-134 G2 工况识别（原始物理量输入，EML:82） ---- */
            double pdec30[30];
            for (int j = 0; j < 30; j++) {
                pdec30[j] = xP[10 * j];                      /* EML:82 xP(1:10:300) */
            }
            int cls = 0;
            if (ems_infer_workclf(pdec30, in->v_ship, accel, &cls, NULL, NULL) != 0) {
                ok = 0; /* 识别推理失败 → ê=0 回退（design Error Handling） */
            }

            if (ok) {
                /* 类别越界守卫（design Error Handling：按非 upstream K=1 处理，
                 * 跳过迟滞更新并记事件；EML:134 argmax 本不会越界，此为固件扩展） */
                if (cls >= 1 && cls <= 7) {
                    /* ---- EML:136-148 迟滞状态机 ---- */
                    ems_ctrl_hysteresis_step(&st->gate, cls);
                    out->cls_raw = cls;
                } else {
                    out->cls_raw = cls;
                    out->cls_invalid = 1;
                }
                const int k_w = out->cls_invalid ? 1 : ems_ctrl_gate_kw(st->gate.cls_eff);

                /* ---- EML:155-165 GATE_MODE 合成 + ê 门控 ---- */
                const double k = ems_ctrl_gate_combine(EMS_GATE_MODE, k_q, k_w);
                e_hat = k * e_hat_raw;

                out->e_hat_raw = e_hat_raw;
                out->q3_kwh[0] = q10;
                out->q3_kwh[1] = q50;
                out->q3_kwh[2] = q90;
                out->s_q = s_q;
                out->K_q = k_q;
                out->K_w = k_w;
                out->K = k;
                out->cls_eff = st->gate.cls_eff;
            }
        }
    }
    if (!ok) {
        e_hat = 0.0; /* EML:166-169 回退 ê=0（≡law=5）；门控状态本拍不动 */
        out->K = 1.0; /* 观测面中性值（EML 该分支无 K 概念） */
        out->cls_eff = st->gate.cls_eff;
    }
    out->ok = ok;
    out->e_hat = e_hat;

    /* ---- EML:170-175 λ 自适应（SOC_ref 通道）+ Λ_floor 钳位 ---- */
    const double soc_ref_eff = EMS_KS.soc_ref + EMS_KS.k_s * e_hat;  /* EML:171 */
    double lam = EMS_KS.lambda0 + EMS_KS.k_soc * (soc_ref_eff - in->SOC); /* EML:172 */
    if (EMS_KS.k_s != 0.0) {                                         /* EML:173-175 */
        /* fmax 与 MATLAB max(NaN, floor)=floor 语义一致（SOC 非有限时） */
        lam = fmax(lam, EMS_KS.lambda_floor);
    }

    /* ---- EML:176-223 网格穷举决策 ---- */
    const ems_ctrl_ecms_cst_t cst = {
        EMS_PLANT.eta_rect,
        EMS_PLANT.eta_dcdc,
        EMS_PLANT.bat_eff_dis,
        EMS_PLANT.bat_eff_chg,
        EMS_PLANT.soc_discharge_max,
        EMS_PLANT.soc_charge_min,
        EMS_PLANT.idle_fuel_kgph,
        EMS_PLANT.grid_dp_kw,
        EMS_PLANT.gen_p_max_kw,
        EMS_BSFC_BP,
        EMS_BSFC_TBL,
        (int)EMS_PARAMS_BSFC_N,
    };
    const double pbest = ems_ctrl_grid_search(&cst, in->P_load, in->SOC, lam);

    /* ---- EML:223-224 输出 + 保持 ---- */
    P = pbest;
    st->Pheld = pbest;
    out->P = P;
    return P;
}
