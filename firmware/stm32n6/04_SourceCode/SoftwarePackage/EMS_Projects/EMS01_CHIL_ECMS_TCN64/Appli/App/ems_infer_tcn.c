/* ems_infer_tcn.c — REF 推理后端：TCN-f64 预测器 CPU double 前向
 * （FR-511 编译期换型，spec ems01-chil-tcn64-swap，T-804；
 *   closure 精确增量修订：spec 后继任务，主 Agent 2026-09-03 裁决）。
 *
 * 结构（design §Architecture 2；tcn_f64.keras 实测盘点）：
 *   窗 x302[0:300]（升序，归一化域）→ 7 残差块（block0 另有 1×1 skip conv 1→64；
 *   block1..6 恒等 skip）：Conv1D(k=3,causal,d)→ReLU→Conv1D(k=3,causal,d)→ReLU
 *   →(+)→ReLU，d=1,2,4,8,16,32,32 → LastTimeStep（末步 64）
 *   旁路 aux=x302[300:302]=[v;a] → Dense(2→8)+ReLU → concat[72]
 *   → Dense(72→64)+ReLU → Dense(64→3)（线性）= q3_norm（归一化域，
 *   反归一化属调用方，与 MLP-Q 槽位契约一致）。
 *
 * == 设计修订：依赖闭包精确增量（closure）替代 plain 流式 ==
 * 原 design §2 拟定"环深 2d+1 流式 + 窗口连续性 fast/cold 双路"。L0 数值
 * 实验（真实权重，ems_ctrl 型滑窗 700 tick，gcc -O2 -ffp-contract=off，
 * /tmp/tcn_stream/probe.c 沙盒）否决：
 *   (a) 归一化域连续性检查（x302[0:299] == prev_win[1:300] 逐位）永不成立：
 *       x302 为 per-feature 归一化（窗位 i 与 i+1 使用不同 X_mean/X_std），
 *       滑窗平移位上归一化值必不同 → fast path 触发 0/700（机制死码）；
 *   (b) 改用 raw 窗连续性后，plain fast path（环缓存每层推进 1 步）与 batch
 *       前向位级失配 699/699（首拍 tick 2，worst |Δq50| = 3.149 kWh =
 *       11.8 LSB_TCN，相对最高 168%）。根因（结构性，非实现缺陷）：
 *       感受野 RF = 1 + 4·Σd = 1 + 4·(1+2+4+8+16+32+32) = 381 > 窗长 300，
 *       batch 每拍在"窗口首"移动左零边界，而环缓存值携带着上次冷复位点的
 *       陈旧零边界——缓存值 ≠ batch 对齐位中间值；该失配与环深无关（陈旧性
 *       在"何时算出"，不在"存多少"），任何环深/索引方案均无法修复。
 * 定案（主 Agent 裁决）：**依赖闭包精确增量**——每拍从当前 x302 窗重算末
 * 输出（A_6[299]）依赖闭包内的全部 (层,位置) 节点，无跨拍状态：
 *   - 闭包节点集与数据无关（stride 结构固定）：C1_0 全 300 位、A_0/C1_1 各
 *     150、A_1/C1_2 各 75、A_2 38、C1_3 37、A_3/C1_4 各 19、A_4 9、C1_5 7、
 *     A_5 5、C1_6 3、A_6 1，共 889 节点（窗触点全 300）；
 *   - MAC/拍 = 7,300,048（batch 51.9M 的 1/7.1）；host gcc -O2 实测
 *     3.3 ms/call（batch 22.1 ms）；板上按 T-810 实测 ~5.9M MAC/s 折算
 *     ≈1.2 s/拍（batch 8.8 s 的 1/7）——层①全轨迹 INJECT（<2s STALE 契约）
 *     恢复可行；FR-713 的 60 ms 预算仍超，按 design §Error Handling 既定
 *     Escalation 路径处置（登记/降级/终止，用户裁决），本实现不放宽任何阈值；
 *   - bit-exact：每输出元素运算树与 batch 完全一致（见"求和顺序"），输入值
 *     = 当前窗同位值 → 构造性逐位一致；L0 实测 700/700 tick max|Δ| = 0
 *     （S20 型滑窗序列；板级链见 Host/results/tcn_swap/l0_tcn_stream_diff.json）；
 *   - 窗口连续性自检/fast-cold 双路随 plain 流式一并废止：closure 无跨拍
 *     状态，每拍全闭包重算，连续与不连续窗口同价同语义（原 fast path 机制
 *     无承载对象，且 (a) 证其永不触发）。
 *
 * 求和顺序（层① host 位级基准口径，TCN 无 EML 锚点；与 batch/keras 链同族，
 * 原口径不变）：
 *   Conv：每输出位 acc 从偏置起 → kernel kk 升序 →（通道点积 s：输入通道 c
 *   升序累加）→ acc += s → ReLU（j<0 左补零：整项跳过，不加 0）。
 *   残差融合块：out = fmax(块输入 + fmax(acc,0), 0)。
 *   Dense：dense_unit_ref 同式（i 升序累加后加偏置）。
 *
 * 内存（design §Error Handling 对策口径）：static 双缓冲 tcn_buf1/tcn_buf2
 * （各 [300][64] double ≈ 153.6 KB，闭包计算轮转：conv1 写 buf1、conv2+残差
 * 原地写 buf2——buf2 只被同位同元素读后写，无跨位别名）+ 闭包掩码
 * （uint8_t 2×7×300 ≈ 4.2 KB，每拍重建：与数据无关，代价可忽略）。
 * 本文件 bss ≈ 311.7 KB（batch 版 309.6 KB，净增 ~2 KB）；REF 镜像总占用
 * 以 T-806 口径构建实测核对（预算 ≤2047K−64KB）。
 *
 * workclf 前向与遥测函数经宏改名 #include "ems_infer_ref.c" 同源复用（分类器
 * 零改动、零漂移；ems_npu_mlpq.c 改名 include 同款机制）；被换下的 MLP-Q 前向
 * 以 unused 符号保留于本编译单元（板上构建由 gc-sections 裁除）。
 * 测试专用批量参考：-DEMS_TCN_BATCH_REF 编译时本文件的 ems_infer_mlpq 切换为
 * T-804 batch 前向（L0 closure-vs-batch 位级比对基准；固件构建不定义该宏，
 * 批量路径整体编译剔除，零固件成本）。
 * 平台无关：无 HAL、无动态内存；us 输出写 0（同 ems_infer_ref.c 契约）。
 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "ems_infer.h"
#include "ems_params.h"

/* ---- workclf + 遥测：同名换下 MLP-Q 前向后整体包含（见头注释） ---- */
#define ems_infer_mlpq ems_infer_mlpq_unused_mlpq_ref
#define dense_unit dense_unit_ref
#include "ems_infer_ref.c"
#undef ems_infer_mlpq
#undef dense_unit

#define TCN_T 300
#define TCN_C 64

/* 头部（LastTimeStep 之后的无状态段）与输出契约：两路径共用，batch 同式同序 */
static void tcn_head(const double last[TCN_C], const double x302[302], double q3[3])
{
    double z[TCN_C + 8];
    double h64[TCN_C];
    for (int j = 0; j < TCN_C; j++) {
        z[j] = last[j];
    }
    for (int j = 0; j < 8; j++) { /* s8 = max(W8·[v;a]+b8, 0)：i 升序累加后加偏置 */
        double acc = EMS_TCNQ_HEAD8_W[0 * 8 + j] * x302[300];
        acc += EMS_TCNQ_HEAD8_W[1 * 8 + j] * x302[301];
        z[TCN_C + j] = fmax(acc + EMS_TCNQ_HEAD8_B[j], 0.0);
    }
    for (int j = 0; j < TCN_C; j++) {
        double acc = EMS_TCNQ_HEAD64_W[0 * 64 + j] * z[0];
        for (int i = 1; i < 72; i++) {
            acc += EMS_TCNQ_HEAD64_W[i * 64 + j] * z[i];
        }
        h64[j] = fmax(acc + EMS_TCNQ_HEAD64_B[j], 0.0);
    }
    for (int j = 0; j < 3; j++) {
        double acc = EMS_TCNQ_OUT_W[0 * 3 + j] * h64[0];
        for (int i = 1; i < 64; i++) {
            acc += EMS_TCNQ_OUT_W[i * 3 + j] * h64[i];
        }
        q3[j] = acc + EMS_TCNQ_OUT_B[j];
    }
}

/* 输出有限性 + us 契约（batch/ref 同语义；FR-508 回退由调用方执行） */
static int tcn_tail(double q3[3], uint32_t *us)
{
    for (int j = 0; j < 3; j++) {
        if (!isfinite(q3[j])) {
            return 1; /* 输出非有限 → 调用方回退（FR-508，design Error Handling） */
        }
    }
    if (us != NULL) {
        *us = 0u; /* host/REF 不计时；板上由 DWT 封装负责（同 ems_infer_ref.c 契约） */
    }
    return 0;
}

#ifdef EMS_TCN_BATCH_REF
/* ==================== 测试专用：T-804 batch 前向（位级基准） ====================
 * 与 T-804 版逐字同构（static 双缓冲 + 整窗三重循环）；仅 L0 closure-vs-batch
 * 位级比对编译（test_ctrl_diff.py -DEMS_TCN_BATCH_REF），固件构建（无本宏）
 * 完全剔除。本分支的 ems_infer_mlpq 即槽位实现（驱动零改动）。 */
static double tcn_a[TCN_T][TCN_C]; /* 块输入 →（融合残差）块输出 */
static double tcn_b[TCN_T][TCN_C]; /* conv1 / conv2 中间 */
static double tcn_win[TCN_T];      /* x302[0:300]（1 通道，block0 conv1/skip 输入） */

/* causal dilated Conv1D(k=3)：out[t][f] = b[f] + Σ_{kk=0..2} s(kk)，s = Σ_c
 * W[(kk*CIN+c)*COUT+f]·x[t-(2-kk)*d][c]（j<0 左补零跳过）→ ReLU。 */
static void tcn_conv3(const double x[TCN_T][TCN_C], int cin,
                      const double *w, const double *b, int dilation,
                      double out[TCN_T][TCN_C])
{
    const int cout = TCN_C;
    for (int t = 0; t < TCN_T; t++) {
        for (int f = 0; f < cout; f++) {
            double acc = b[f];
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dilation;
                if (j >= 0) {
                    double s = 0.0;
                    for (int c = 0; c < cin; c++) {
                        s += x[j][c] * w[(kk * cin + c) * cout + f];
                    }
                    acc += s;
                }
            }
            out[t][f] = fmax(acc, 0.0);
        }
    }
}

/* Conv1D(k=1)（仅 block0 skip，cin=1）：out[t][f] = b[f] + W[f]*win[t]。
 * keras tcn_block 的 skip 分支 Add 前无 ReLU（线性旁路），ReLU 只在 Add 之后。 */
static void tcn_conv1_skip(const double win[TCN_T], const double *w,
                           const double *b, double out[TCN_T][TCN_C])
{
    for (int t = 0; t < TCN_T; t++) {
        for (int f = 0; f < TCN_C; f++) {
            out[t][f] = b[f] + w[f] * win[t];
        }
    }
}

int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    static const int dil[7] = {1, 2, 4, 8, 16, 32, 32};

    for (int i = 0; i < TCN_T; i++) {
        tcn_win[i] = x302[i];
    }

    /* ---- block0：conv1(cin=1, d=1) → a；conv2(d=1) → b；skip → 融合残差 → a ---- */
    for (int t = 0; t < TCN_T; t++) {
        for (int f = 0; f < TCN_C; f++) {
            double acc = EMS_TCNQ_B0_CONV1_B[f];
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dil[0];
                if (j >= 0) {
                    acc += EMS_TCNQ_B0_CONV1_W[kk * TCN_C + f] * tcn_win[j];
                }
            }
            tcn_a[t][f] = fmax(acc, 0.0);
        }
    }
    tcn_conv3(tcn_a, TCN_C, EMS_TCNQ_B0_CONV2_W, EMS_TCNQ_B0_CONV2_B, dil[0], tcn_b);
    tcn_conv1_skip(tcn_win, EMS_TCNQ_B0_SKIP_W, EMS_TCNQ_B0_SKIP_B, tcn_a);
    /* 此时 a = skip_raw；融合：a = relu(skip_raw + relu(conv2))（keras Add+ReLU） */
    for (int t = 0; t < TCN_T; t++) {
        for (int f = 0; f < TCN_C; f++) {
            tcn_a[t][f] = fmax(tcn_a[t][f] + tcn_b[t][f], 0.0);
        }
    }

    /* ---- block1..6：conv1 → b；conv2+残差+ReLU 融合原地写 a ---- */
    for (int i = 1; i < 7; i++) {
        const double *w1 = (i == 1) ? EMS_TCNQ_B1_C1_W : (i == 2) ? EMS_TCNQ_B2_C1_W :
                           (i == 3) ? EMS_TCNQ_B3_C1_W : (i == 4) ? EMS_TCNQ_B4_C1_W :
                           (i == 5) ? EMS_TCNQ_B5_C1_W : EMS_TCNQ_B6_C1_W;
        const double *b1 = (i == 1) ? EMS_TCNQ_B1_C1_B : (i == 2) ? EMS_TCNQ_B2_C1_B :
                           (i == 3) ? EMS_TCNQ_B3_C1_B : (i == 4) ? EMS_TCNQ_B4_C1_B :
                           (i == 5) ? EMS_TCNQ_B5_C1_B : EMS_TCNQ_B6_C1_B;
        const double *w2 = (i == 1) ? EMS_TCNQ_B1_C2_W : (i == 2) ? EMS_TCNQ_B2_C2_W :
                           (i == 3) ? EMS_TCNQ_B3_C2_W : (i == 4) ? EMS_TCNQ_B4_C2_W :
                           (i == 5) ? EMS_TCNQ_B5_C2_W : EMS_TCNQ_B6_C2_W;
        const double *b2 = (i == 1) ? EMS_TCNQ_B1_C2_B : (i == 2) ? EMS_TCNQ_B2_C2_B :
                           (i == 3) ? EMS_TCNQ_B3_C2_B : (i == 4) ? EMS_TCNQ_B4_C2_B :
                           (i == 5) ? EMS_TCNQ_B5_C2_B : EMS_TCNQ_B6_C2_B;
        tcn_conv3(tcn_a, TCN_C, w1, b1, dil[i], tcn_b);
        for (int t = 0; t < TCN_T; t++) {
            for (int f = 0; f < TCN_C; f++) {
                double acc = b2[f]; /* conv2 累加（与 tcn_conv3 同顺序） */
                for (int kk = 0; kk < 3; kk++) {
                    const int j = t - (2 - kk) * dil[i];
                    if (j >= 0) {
                        double s = 0.0;
                        for (int c = 0; c < TCN_C; c++) {
                            s += tcn_b[j][c] * w2[(kk * TCN_C + c) * TCN_C + f];
                        }
                        acc += s;
                    }
                }
                tcn_a[t][f] = fmax(tcn_a[t][f] + fmax(acc, 0.0), 0.0);
            }
        }
    }

    tcn_head(tcn_a[TCN_T - 1], x302, q3);
    return tcn_tail(q3, us);
}

#else /* 常规固件/L0 构建：依赖闭包精确增量（本文件主体） */

/* 闭包掩码：1 = 该 (层,时间位) 输出被末输出依赖（数据无关，每拍重建）。
 * C1_i = block i conv1 输出（conv2_i 输入）；A_i = block i 输出（残差后）。 */
static uint8_t tcn_m_c1[7][TCN_T];
static uint8_t tcn_m_a[7][TCN_T];

/* 重建闭包：need 从 A_6[299] 反向传播（conv2/conv1 各自的 3 tap 读集）。
 * 残差同位读：m_a[i] ⊆ m_c1[i]（t 为自身 tap），由 m_c1[i] 的传播覆盖。 */
static void tcn_closure_build(void)
{
    static const int dil[7] = {1, 2, 4, 8, 16, 32, 32};
    for (int i = 0; i < 7; i++) {
        for (int t = 0; t < TCN_T; t++) {
            tcn_m_c1[i][t] = 0;
            tcn_m_a[i][t] = 0;
        }
    }
    tcn_m_a[6][TCN_T - 1] = 1; /* LastTimeStep：仅末步 */
    for (int i = 6; i >= 1; i--) {
        /* need_A_i → need_C1_i（conv2 的 3 tap 读集，j<0 左补零跳过） */
        for (int t = 0; t < TCN_T; t++) {
            if (!tcn_m_a[i][t]) {
                continue;
            }
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dil[i];
                if (j >= 0) {
                    tcn_m_c1[i][j] = 1;
                }
            }
        }
        /* need_C1_i → need_A_{i-1}（conv1 的 3 tap 读集；C1 位置本身即残差
         * 读位——A_i[t] 需 A_{i-1}[t]，t ∈ m_c1[i]，故 t 已含于标记集） */
        for (int t = 0; t < TCN_T; t++) {
            if (!tcn_m_c1[i][t]) {
                continue;
            }
            tcn_m_a[i - 1][t] = 1;
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dil[i];
                if (j >= 0) {
                    tcn_m_a[i - 1][j] = 1;
                }
            }
        }
    }
    /* block0：need_A_0 → need_C1_0（conv1_0 的 3 tap 读窗）；skip 同位读窗
     * （t ∈ m_a[0] ⊆ 窗触点）。窗触点 = C1_0 触点 ∪ A_0 触点，闭包传播后
     * C1_0 触点已覆盖 A_0 触点（t 为自身 tap），无需独立 win 掩码。 */
    for (int t = 0; t < TCN_T; t++) {
        if (!tcn_m_a[0][t]) {
            continue;
        }
        for (int kk = 0; kk < 3; kk++) {
            const int j = t - (2 - kk) * dil[0];
            if (j >= 0) {
                tcn_m_c1[0][j] = 1;
            }
        }
    }
}

/* 双缓冲：conv1 写 buf1（读 buf2/x302），conv2+残差原地写 buf2（仅同位同元素
 * 读后写，无跨位别名；buf1 在本层 conv2 循环中只读）。占用与 batch 版同量级。 */
static double tcn_buf1[TCN_T][TCN_C];
static double tcn_buf2[TCN_T][TCN_C];

int ems_infer_mlpq(const double x302[302], double q3[3], uint32_t *us)
{
    static const int dil[7] = {1, 2, 4, 8, 16, 32, 32};
    static const double *const w1t[7] = {
        EMS_TCNQ_B0_CONV1_W, EMS_TCNQ_B1_C1_W, EMS_TCNQ_B2_C1_W, EMS_TCNQ_B3_C1_W,
        EMS_TCNQ_B4_C1_W, EMS_TCNQ_B5_C1_W, EMS_TCNQ_B6_C1_W};
    static const double *const b1t[7] = {
        EMS_TCNQ_B0_CONV1_B, EMS_TCNQ_B1_C1_B, EMS_TCNQ_B2_C1_B, EMS_TCNQ_B3_C1_B,
        EMS_TCNQ_B4_C1_B, EMS_TCNQ_B5_C1_B, EMS_TCNQ_B6_C1_B};
    static const double *const w2t[7] = {
        EMS_TCNQ_B0_CONV2_W, EMS_TCNQ_B1_C2_W, EMS_TCNQ_B2_C2_W, EMS_TCNQ_B3_C2_W,
        EMS_TCNQ_B4_C2_W, EMS_TCNQ_B5_C2_W, EMS_TCNQ_B6_C2_W};
    static const double *const b2t[7] = {
        EMS_TCNQ_B0_CONV2_B, EMS_TCNQ_B1_C2_B, EMS_TCNQ_B2_C2_B, EMS_TCNQ_B3_C2_B,
        EMS_TCNQ_B4_C2_B, EMS_TCNQ_B5_C2_B, EMS_TCNQ_B6_C2_B};

    tcn_closure_build(); /* 数据无关，每拍重建（~O(7·300·3)，远小于卷积） */

    /* ---- block0：conv1(cin=1, d=1) 闭包位 → buf1；conv2+skip+残差 → buf2 ---- */
    for (int t = 0; t < TCN_T; t++) {
        if (!tcn_m_c1[0][t]) {
            continue;
        }
        for (int f = 0; f < TCN_C; f++) {
            double acc = b1t[0][f];
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dil[0];
                if (j >= 0) {
                    acc += w1t[0][kk * TCN_C + f] * x302[j];
                }
            }
            tcn_buf1[t][f] = fmax(acc, 0.0);
        }
    }
    for (int t = 0; t < TCN_T; t++) {
        if (!tcn_m_a[0][t]) {
            continue;
        }
        for (int f = 0; f < TCN_C; f++) {
            double acc = b2t[0][f];
            for (int kk = 0; kk < 3; kk++) {
                const int j = t - (2 - kk) * dil[0];
                if (j >= 0) {
                    double s = 0.0;
                    for (int c = 0; c < TCN_C; c++) {
                        s += tcn_buf1[j][c] * w2t[0][(kk * TCN_C + c) * TCN_C + f];
                    }
                    acc += s;
                }
            }
            /* 融合（keras Add+ReLU）：A_0 = fmax(skip_raw + relu(conv2), 0)；
             * skip_raw = b[f] + w[f]*x302[t]（batch 先存后加，同运算树逐位一致） */
            tcn_buf2[t][f] = fmax(EMS_TCNQ_B0_SKIP_B[f] +
                                      EMS_TCNQ_B0_SKIP_W[f] * x302[t] + fmax(acc, 0.0),
                                  0.0);
        }
    }

    /* ---- block1..6：conv1 → buf1（读 buf2 全部闭包位）；conv2+残差原地写 buf2 ---- */
    for (int i = 1; i < 7; i++) {
        for (int t = 0; t < TCN_T; t++) {
            if (!tcn_m_c1[i][t]) {
                continue;
            }
            for (int f = 0; f < TCN_C; f++) {
                double acc = b1t[i][f];
                for (int kk = 0; kk < 3; kk++) {
                    const int j = t - (2 - kk) * dil[i];
                    if (j >= 0) {
                        double s = 0.0;
                        for (int c = 0; c < TCN_C; c++) {
                            s += tcn_buf2[j][c] * w1t[i][(kk * TCN_C + c) * TCN_C + f];
                        }
                        acc += s;
                    }
                }
                tcn_buf1[t][f] = fmax(acc, 0.0);
            }
        }
        for (int t = 0; t < TCN_T; t++) {
            if (!tcn_m_a[i][t]) {
                continue;
            }
            for (int f = 0; f < TCN_C; f++) {
                double acc = b2t[i][f]; /* conv2 累加（与 batch 同顺序） */
                for (int kk = 0; kk < 3; kk++) {
                    const int j = t - (2 - kk) * dil[i];
                    if (j >= 0) {
                        double s = 0.0;
                        for (int c = 0; c < TCN_C; c++) {
                            s += tcn_buf1[j][c] * w2t[i][(kk * TCN_C + c) * TCN_C + f];
                        }
                        acc += s;
                    }
                }
                /* buf2[t][f] = 块输入（残差，同元素读后写）；闭包保证 conv1 对
                 * buf2 的读已在本层 conv2 循环前完成（buf1/buf2 分离），无别名 */
                tcn_buf2[t][f] = fmax(tcn_buf2[t][f] + fmax(acc, 0.0), 0.0);
            }
        }
    }

    /* ---- LastTimeStep（buf2[299] = A_6[299]，闭包保证已算）+ 头部 ---- */
    tcn_head(tcn_buf2[TCN_T - 1], x302, q3);
    return tcn_tail(q3, us);
}

#endif /* EMS_TCN_BATCH_REF */