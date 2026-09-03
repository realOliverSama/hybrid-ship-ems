# EMS01 CHIL — TCN-f64 编译期换型 Design

Spec: `ems01-chil-tcn64-swap`
Status: design-approved（2026-09-02 用户批准）
Created: 2026-09-02
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`（requirements-approved，2026-09-02）
路径约定: `FW` = `EMS_Projects/EMS01_CHIL_ECMS/`；`WS` = `model/simulink_modelD_(EMS_NN_AECMS)`；
`deploy` = `WS/results/ecms_phase25/deploy`；`models` = `WS/results/ecms_phase25/models`

## Summary

按 FR-511 流程将预测器槽位（NOR 0x70200000 mpool）从 MLP-Q G2 编译期换入 TCN-f64：
**NPU 侧** stedgeai generate 产出双输入网络（分析预检已证 CONV/FC 全上 NPU、mpool
零改动），**REF 侧** 新开发 `ems_infer_tcn.c`（double 前向）保住四臂矩阵的 (b) 臂；
验证链为 **L0 host 差分 → 双镜像构建+fw_gate → 板上层①（两段式新口径）→ 层②
（FR-504 新 LSB + INJECT）→ 层③式闭环 (a)vs(b) → 报告登记**。全部测试形态、向量、
场景、指标定义与 JSON schema 沿 v1/v3 既有口径（Fixed Decisions 10），TCN 结果以
增量条目并入 `closedloop_metrics.json`，可直接并入导师报告表格体系。

关键数字前提（本 design 论证依据，来源见 §Memory/§Testing）：

| 项 | 值 | 来源 |
|---|---|---|
| TCN-f64 int8 tflite | 219.7 KB（QDQ，float32 I/O，139/185 张量 int8） | `models/tcn_f64/` |
| 输入量化 | 窗口 [1,300,1] scale=0.012579751 zp=−22；aux [1,2] scale=0.0534951 zp=−19 | tflite 张量表（本 session 实测） |
| 输出量化 | [1,3] per-tensor scale=**0.0161389485** zp=−28 | 同上 |
| **1 LSB_TCN** | 0.0161389485 × y_std 16.495129 = **0.266214 kWh**（mlpq 1 LSB=0.253636） | 本 design 推导 |
| analyze 预检 | 132 epochs（47 SW，均为 Pad/SpaceToDepth/DepthToSpace/Quantize/Gather 数据搬运）；**14 CONV_2D + 3 FULLY_CONNECTED 全 EC（NPU HW）**；macc 11.96M | `/tmp/tcn_analyze` 预检报告（同 mlpq 配置；T-702 正式归档时以工程内归档为准） |
| activations | 131.32 KB @npuRAM3（448 KB 池 29.31%） | 同上 |
| NPU 权重 | 161.267 KB @octoFlash **0x70200000**（1 MB 池 15.75%，与 mlpq 同基址） | 同上 |
| REF 镜像增量 | EMS_TCNQ_* double 表 165,851×8 B ≈ **1.265 MiB**；build-ref2（LRUN_RAMxspi1，2047K）现用 ~344 KB → 换型后 ~1.61 MB，余量 ~430 KB | objdump 实测 + 本 design 推导 |
| NPU 镜像增量 | network.c text ~4.5K + rodata ~103K；EMS_TCNQ_* 表经 vendor 工具链 `-fdata-sections/--gc-sections` 自动裁除（EMS_MLPQ_W1 不在 NPU ELF 已实证） | `gcc-arm-none-eabi.cmake:28,45` + nm 实测 |

## Goals and Non-Goals

### Goals

- FR-701~706：TCN-f64 部署链（`Model/tcn_q/` 全套 + deploy JSON 单一来源 +
  `ems_infer_tcn.c` + NPU 双输入适配）落地且脚本化可复现。
- FR-707/708：双镜像 fw_gate 7/7 ×2 + 指纹新鲜度；三口径一致检查机器可读报告。
- FR-708b/709/710/711：L0 → 层① → 层② → 层③式全链一致性复测，阈值全部先验设定。
- FR-712~715：闭环 4 组新跑 + (b′)/(c) 引用 v3 权威数据；时延实测；FR-508 抽测；
  报告/roadmap/日志登记（表格结构兼容导师报告并入）。

### Non-Goals

- 模型重训 / 量化重标定（T-307 int8 tflite 原样使用）；workclf、门控、FR-508 语义
  修改；运行期热切换（v2）；层② S20 收敛研究；Winner 更换决策；NOR mpool 布局变更；
  主模型 / PlantStep / G-P 仪器链任何改动或重跑。

## Architecture

### 1. 预测器槽位契约（换型边界，FR-705）

v1 的后端契约保持不变：`ems_infer.h` 中 `int ems_infer_mlpq(const double x302[302],
double q3_norm[3], uint32_t *us)` **升格为"预测器槽位契约"**（符号名沿 v1 历史，
头注释注明：实现体随换型编译期选择，FR-511 换型不改调用点）——语义 = 接收归一化
302 维（[P 窗 300 | v | a]，升序），输出归一化 q3[3]。**`ems_ctrl.c` 与 `ems_app.c`
的调用点零改动**（INFER_TEST 路径 ems_app.c:120 与控制路径 ems_ctrl.c:300 均已
按此契约调用）。

```text
                 ┌─ EMS_FW_MODE=NPU ── ems_infer_npu.c（改：TCN 双输入填充）
预测器槽位契约    │                       └ ems_npu_tcnq.c（新：network.c 改名包装）
ems_infer_mlpq ←─┤                        └ Model/tcn_q/network.c（stedgeai 生成）
(x302 → q3_norm) │
                 └─ EMS_FW_MODE=REF ── ems_infer_tcn.c（新：TCN C double 前向）
                                         └ EMS_TCNQ_* 表（ems_params.c 重生成）

workclf 通路（ems_infer_workclf / WC_TEST / 门控）零改动，两模式不动。
```

CMake 增补 `EMS_PREDICTOR` 编译开关（`mlpq | tcnq`，默认 `tcnq` 于换型分支）：
NPU 模式下选编 `ems_npu_mlpq.c` 或 `ems_npu_tcnq.c`；REF 模式下选编
`ems_infer_ref.c` 或 `ems_infer_tcn.c`。换回 MLP-Q = `-DEMS_PREDICTOR=mlpq`
+ 重烧 mlpq hex（资产全保留，可逆性由 FR-511 机制保证）。

### 2. REF 侧：`ems_infer_tcn.c`（方案 R1，Q2 裁决）

网络结构（`tcn_f64.keras` 实测盘点 + analyze 层表交叉核对）：

```text
in0 [1,300,1] 窗（xnorm[0:300]，升序）──┐
in1 [1,2]（xnorm[300:302] = [v,a]）─────┤
                                        ▼
7 个残差块（block0 带 1×1 skip conv 1→64；block1..6 恒等 skip）：
  每块：Conv1D(k=3, causal, dilation d) → ReLU → Conv1D(k=3, causal, dilation d) → ReLU → (+skip) → ReLU
  dilation 序列 d = 1, 2, 4, 8, 16, 32, 32（block0..6）
LastTimeStep：取末端时间步 [1,64]
旁路：s8 = max(W8·[v;a]+b8, 0)（Dense 2→8）
concat [64;8] → Dense(72→64) + ReLU → Dense(64→3)（线性）= q3_norm
```

C 实现约定（对齐 workclf 既有风格，design 固定）：

- **dilation 因果卷积**：`out[t][f] = b[f] + Σ_{kk=0..2} w[(kk·CIN+c)·COUT+f]·x[t−(2−kk)·d][c]`
  （j<0 左补零跳过）；参数布局沿 `EMS_CLF_CONV*_W[(kk·CIN+c)·COUT+f]` 既有约定；
- **求和顺序**（层① host 位级基准，TCN 无 EML 锚点）：每输出位 acc 从偏置起，
  kernel kk 升序 → 输入通道 c 升序累加（与 workclf conv2/conv3 的通道点积顺序同族）；
  该顺序写入 `ems_infer_tcn.c` 头注释与口径声明表；
- **精度**：全 double（沿 REF 镜像口径）；权重表 double 来自 deploy JSON（源 = keras
  float32，上转精确无损）；
- **返回值契约**：输出非有限 → 返回 1（调用方 FR-508 FALLBACK ê=0），与 ems_infer.h
  现有语义一致；
- 内部缓冲：静态栈上数组（`[300][64]` double ×2 乒乓 = 300×64×8×2 ≈ 307 KB 栈占用
  ——**超 REF 任务栈预算风险**，见 §Error Handling 栈对策：改用 int8 域?否——REF
  语义为 double 前向，对策 = 声明为 static 全局或分块计算，design 定为 **static
  双缓冲 + 编译期 static assert**，栈占用归零，bss 增 ~614 KB，REF 镜像总占用
  1.61 MB + 0.6 MB ≈ 2.2 MB **超 2047K 窗口**——不可行；改定：**双缓冲改 int32
  中间量化域?否**——最终采用**按块覆盖计算**（单缓冲 [300][64] = 153.6 KB bss +
  残差分支原位读输入），见 Error Handling §栈与内存对策）。

> 注：上段中划线方案为推演过程，**定案见 §Error Handling「栈与内存对策」**——
> 单缓冲原地更新（conv1 输出直接覆写输入缓冲的未读区不安全，故用
> `in`/`tmp` 双数组但每块 tmp 复用同一块，bss 增量 ≈ 2×153.6 KB ≈ 307 KB，
> REF 镜像总计 ≈ 344 KB（现）+ 1,332 KB（表）+ 307 KB（缓冲）+ 代码 ≈ 1.99 MB，
> **逼近 2047K 上限**——design 要求 T-704 构建后以 objdump 实测总占用并预留
> ≥64 KB 余量；若超限，Escalation 预案 = TCN 表改 float32 存储（−664 KB，源精度
> 即 f32，反归一化语义不变，须在口径声明表注明）。

### 3. NPU 侧：双输入适配（FR-704）

- `Model/tcn_q/generate-n6-model.sh`：沿 mlpq 脚本逐行同构（`--no-inputs-allocation
  --no-outputs-allocation`、`--st-neural-art default@user_neuralart.json`、objcopy
  基址 0x70200000 → `network-data-tcnq.hex`）；生成符号仍为 `*_Default`，
  `ems_npu_tcnq.c` 以同名宏改名为 `*_Tcnq` 后 `#include` 生成源（沿
  `ems_npu_mlpq.c` 惯例；TCN 为单网络实例，改名仅为与未来并存预留一致性）。
- `ems_infer_npu.c` 的 `ems_infer_mlpq` 实现改为 TCN 双输入填充：
  `Set_User_Input_Buffer_x(0, win, 300×4)`（[1,300,1] 窗）+ `(1, aux, 2×4)`
  （[1,2]），I/O 缓冲 32B 对齐 + `SCB_InvalidateDCache` 沿既有模式；输出 3×f32
  反归一化由调用方负责（R8 不变）。
- NOR 权重：TCN hex 烧录至 0x70200000（Level C，专用加载器，仅该 1 MB 池区域）；
  workclf 池 0x70400000 不动。

### 4. 数据流（换型后单拍）

```text
P_load 窗/v/accel (raw, 302)
  → ems_ctrl 归一化（单份，R8 不变）xnorm[302]
      → [预测器槽位] TCN 前向（NPU int8 | REF double）→ q3_norm[3]
      → 反归一化 → q10/q50/q90 [kWh] → S_q 门控（G2 常数不动）→ ê → SOC_ref 调制
      → workclf（xnorm 稀疏切片，v1.1 口径不动）→ K 逆水关断
      → ECMS 网格决策（λ 穷举，double，不动）
```

## Data Model

### 1. 部署 JSON（单一来源链第二环）

新增 `models/selected_model_weights_tcnq.json`（生成脚本 `python/export_tcn_deploy.py`，
新增于 WS/python/）：

```json
{
  "task": "tcn64-swap", "run_id": "tcn_f64",
  "model": "TCN-f64 7-block causal dilated (d=1,2,4,8,16,32,32) f=64 + head",
  "source_tflite": "models/tcn_f64/tcn_f64_int8.tflite", "source_keras": "...",
  "sha256_tflite": "...", "git_commit_sha": "...",
  "caliber": { "input_desc": "302 = [P_demand(t-299..t)x300 | v_ship | accel]",
               "norm_source": "datasets/fleet_norm_v2.json",
               "input_domain": "normalized (X_mean/X_std per-feature)",
               "output_domain": "normalized, q10/q50/q90",
               "window_order": "ascending 1s",
               "quant_calib": "T-307 val 前 1000 归一化样本（与部署喂入口径相同）" },
  "norm": { "X_mean": [...302], "X_std": [...302], "y_mean": ..., "y_std": ... },
  "weights": { "b0_conv1_w": [3*64], "b0_conv1_b": [64],
               "b0_skip_w": [64], "b0_skip_b": [64],
               "b0_conv2_w": [3*64*64], "b0_conv2_b": [64],
               "b1_c1_w": ..., "b1_c2_w": ..., ... "b6_c2_w": ...,
               "head8_w": [2*8], "head8_b": [8],
               "head64_w": [72*64], "head64_b": [64],
               "out_w": [64*3], "out_b": [3] }
}
```

权重抽取自 `tcn_f64.keras`（经 `architectures.LastTimeStep` custom object 加载），
导出后以参数量断言（165,851）+ 3 组随机窗口 spot-check（keras vs 导出权重重放
≤1e-6 kWh）机械验证。`gen_fw_params.py` 扩展：读两份 JSON（mlpq + tcnq），
norm/gate 逐位断言一致后生成 `EMS_TCNQ_*` 表追加至 `ems_params.c`；`EMS_MLPQ_*`
保留（可逆性，NPU 镜像由 gc-sections 裁除）。

### 2. 权重表（ems_params.h 增补，布局与 C 实现一一对应）

| 表 | 形状（元素数） | 语义 |
|---|---|---|
| `EMS_TCNQ_B0_CONV1_W/B` | 192 / 64 | block0 conv1（cin=1） |
| `EMS_TCNQ_B0_SKIP_W/B` | 64 / 64 | block0 skip 1×1 |
| `EMS_TCNQ_B<i>_C1_W/B`、`B<i>_C2_W/B`（i=0..6） | 各 12,288 / 64 | 14 层主卷积 |
| `EMS_TCNQ_HEAD8_W/B` | 16 / 8 | 旁路 Dense(2→8) |
| `EMS_TCNQ_HEAD64_W/B` | 4,608 / 64 | Dense(72→64) |
| `EMS_TCNQ_OUT_W/B` | 192 / 3 | 输出头（64→3） |

合计 165,851 参数（=analyze weights 165,137 int8 + bias fp32 之差在生成侧核对）。

### 3. 机器可读产物（schema 沿 v1/v3，Fixed Decisions 10）

- 层①两段：`Host/results/tcn_swap/l0_tcn_diff.json`（host C vs keras）+
  `layer1_ref_vs_host.json`（板上 REF vs host gcc，网格一致率）；
- 层②：FR-504 ×200 报告（`m1_tcn_infer_test/`，LSB 口径换算系数 0.266214 kWh）+
  INJECT `fw_compare_tcn.json`（NPU vs host-int8 网格一致率 + 层③ board NPU vs
  host REF 报告性）；
- 闭环：`closedloop_metrics.json` 增量条目（同 schema，新增 `tcn_f64` 模型键）+
  逐组 gate JSON；
- 三口径一致检查：`deploy/m3_tcn_swap_calib_report.json`（结构沿
  m2_workclf_calib_report.json 家族）。

## API / Interface Changes

- **协议零变更**：CMD_INFER_TEST / CMD_WC_TEST / CMD_TICK / INJECT 帧格式、CRC、
  会话语义全部不动（INFER_TEST 固件内归一化→槽位前向→反归一化，对调用方透明）。
- **ems_infer.h**：契约签名不变；头注释升格说明（预测器槽位 + 换型语义 + us 遥测
  契约不变）。
- **CMake**：新增 `EMS_PREDICTOR` cache 变量（mlpq|tcnq）与对应源选择分支；不改
  既有 NPU/REF 源清单语义（mlpq 分支保持逐字节现状）。
- **Host 工具**：`gen_reference_int8.py` / `gen_host_int8_pgen.py` 增加 `--model
  tcnq` 变体（TCN 双输入拆分喂 tflite 解释器；向量/轨迹/输出格式不变）；
  `test_ctrl_diff.py` 增补 `--predictor tcnq`（gcc 编译 ems_infer_tcn.c，参考链
  切 keras float32）。
- 向后兼容：`-DEMS_PREDICTOR=mlpq` + mlpq hex 复原即回到 v3 收官状态（冻结资产
  与 NOR 布局均未动）。

## Error Handling

- **推理失败路径**：TCN 输出非有限 → ems_infer 返回 1 → FR-508 FALLBACK（ê=0，
  law=5 行为）——与 MLP-Q 同语义，FR-714 抽测覆盖（NaN 输入 / 停发超时 /
  CMD_RESET 恢复）。
- **双输入装配错误防护**：窗口/aux 错位会在 L0 差分与 ×200 中产生量级偏差被门禁
  捕获；`ems_npu_tcnq.c` 包装层对生成物 `network.h` 的 IN_1/IN_2 字节数做
  `static_assert`（300×4 / 2×4），防错位静默通过。
- **NOR 权重校验**：烧录后 CRC32 核对（记录进烧录证据，沿 FR-514 惯例）；
  DEBUG 模式 NPU init 失败 → FR-508 FALLBACK + 事件环记录。
- **栈与内存对策（定案）**：`ems_infer_tcn.c` 内部缓冲用 `static`（不占任务栈）；
  REF 镜像构建后实测总占用，要求 ≤2047K − 64KB 余量；超限时第一候选 = TCN 表
  float32 化（−663 KB，口径声明注明），第二候选 = Escalate 重议（不静默放宽）。
- **时延超预算**（FR-713）：合计 >60ms → 不放宽，Escalate（候选降级 TCN-f16 或
  接受登记或终止换型，用户裁决）。

## Security and Privacy

- 无网络面/无密钥/无用户数据；UART 协议仅实验室主机（沿 v1）。
- 硬件授权边界：SRAM 加载（embedded-debugger MCP，令牌 = DEBUG_MUTATION_AUTHORIZED_*
  单次核销）；NOR 权重烧录（Level C：STM32CubeProgrammer + ATK 外部加载器
  `MX25UM25645G_ATK-CNN647B.stldr`，仅写 0x70200000 起自有两段中的预测器段，
  禁 Mass Erase、禁 Option Bytes/Boot/Security 改动，烧录前后 region 读取留证，
  用户在场）。FSBL 区零接触。
- 阶段隔离：Build/Gate（离线）→ SRAM Debug（层①②）→ NOR Flash（权重）→ 闭环
  复测，逐阶段显式授权，前一阶段完成不自动授权后一阶段。

## Testing Strategy

阈值全部先验设定，FAIL → 机制级排查，不放宽；全部机器可读（Fixed Decisions 10：
与 MLP-Q 电池逐项同构，结果同表可比）。

| 层 | 内容 | 载体与口径 | 门禁（design 固定） | 挂靠 |
|---|---|---|---|---|
| T0 资源预检 | analyze 报告归档 + mpool/RAM/权重预算核对（本 design 表数值） | `Model/tcn_q/analyze_report.txt` | CONV/FC 全 EC；RAM/权重在既有池内；不满足 = 候选重开 | FR-706/AC-1 |
| T1 部署链一致性 | deploy JSON 导出 spot-check + gen_fw_params 断言（norm 逐位一致） | 导出脚本输出 | 参数量 165,851；spot ≤1e-6 kWh | FR-702/AC-1 |
| T2 L0 host 差分 | gcc REF 链（ems_infer_tcn.c）vs keras float32 参考 | `test_ctrl_diff.py --predictor tcnq`；向量 = val 抽样（seed=20260831）+ 32 随机窗（seed 固定）+ S20/voyage002 全轨迹 | **|Δq50| ≤ 1 LSB_TCN = 0.266214 kWh**（逐 tick 全轨迹同门）；P_gen 决策链网格一致率 ≥99% | FR-708b/AC-2 |
| T3 构建与门禁 | 双镜像构建 + fw_gate 7/7 ×2 + 指纹（ELF mtime ≥ 源 mtime）+ REF 总占用实测（≤2047K−64KB） | build/Debug + build-ref2 | exit 0 ×2；占用记录 | FR-707/AC-3 |
| T4 层① | 两段式：① host gcc vs keras（同 T2 数据，板上会话前完成）；② 板上 REF vs host gcc | INJECT 注入（S20 + voyage002，同 v1 M2 轨迹） | ① 同 T2 门禁；② P_gen 网格一致率 **100%**（同 v1 层① 精确语义） | FR-709/AC-4 |
| T5 层② | FR-504 ×200（val 固定 seed=20260831 抽样，原始物理量输入，同 MLP-Q 电池）+ INJECT NPU vs host-int8 | 板 NPU 镜像 + `ems_inject.py`（同 seed 同向量同轨迹） | LSB 口径（LSB_TCN）：mean ≤1.5 / p99 ≤6 / max ≤8 / 无偏置 / 无发散 / RMSE 比 ≤1.1；INJECT 网格一致率 ≥99%（S20 边际 FAIL → (a) 接受登记，沿 v1.1 先例） | FR-710/AC-4 |
| T6 层③式（开环报告性） | 板 NPU vs host REF（TCN C）P_gen | `ems_inject.py` 同电池 | 报告性不设门（沿 v1 M2 层③ 口径） | FR-710 |
| T7 层③式（闭环） | (a) NPU vs (b) REF，v3 harness lock-step | `ems_closedloop.py --port/--image`，S20 + voyage002 | 数值健康前置；SOC 终点 \|Δ\| ≤ 5e-3；fuel_corr 相对偏差 ≤ 2%；无 NaN/发散；P_gen 网格一致率报告性 | FR-711/AC-5 |
| T8 闭环矩阵 | (a)(b) × 2 场景 4 组 REPRO + (b′)/(c) 引用 v3 + 公平性带（±0.02）判定 + 跨模型对比表（TCN vs MLP-Q vs law=5） | `closedloop_metrics.json` 增量条目（同 schema） | 逐组 gate JSON `overall=true` 才采信 | FR-712/AC-5 |
| T9 时延 | TCN 推理 + workclf DWT CYCCNT 实测 | RSP 遥测 infer_us / 闭环 RTT | 合计 ≤60 ms（超限 Escalate） | FR-713/AC-6 |
| T10 FR-508 抽测 | NaN 输入 / 停发超时 / CMD_RESET 恢复（mock 或板上） | `ems_fault_inject.py --closed-loop` / `closedloop_selftest.py` | 语义与 v3 收官记录同构（F1 FALLBACK / F2 STALE / F4 清环） | FR-714/AC-6 |
| T11 登记验收 | 换型报告 + metrics/roadmap/日志/README 登记 + §14 git diff 范围核对 | 文档产物 | 主 Agent 验收 | FR-715/AC-7 |

- **回归保护**：v1.1 冻结件 / baseline_manifest / ref_G2 / fw_reference 零改动；
  G-P1/G-P2 免重跑（被控对象零改动）；L0 MLP-Q 差分链保留（`--predictor mlpq`
  仍可跑通，作为可逆性证据）。
- **T2/T4① 参考实现**：keras float32 前向参考 = `python/export_tcnq_reference.py`
  （加载 tcn_f64.keras + fleet_norm_v2.json，输出逐 tick q3 CSV）；运行环境
  `.venv_tf`。

> **实施期澄清（2026-09-02 晚，T-805 执行，主 Agent 批准记录）**：
> ① T1 "spot ≤1e-6 kWh" 在 f32 keras 域物理不可达（f32 输出张量在 q50≈60 kWh
> 处舍入粒度 ≈3.6e-6 kWh）——门实现为 **f64 上转 keras 前向 vs f64 重放**
> （实测 3.663e-15 kWh），f32 噪声底（1.344e-6 kWh）作为记录性测量保留
> （sanity 上限 1e-3）——比原字面更严格，非放宽；
> ② **连带发现（登记不修复）**：同一启动预热装配缺陷存在于 v1 冻结的
> `Host/reference/reference_int8_trace_*.csv`（前 ~300 tick q 列同源失配；v1
> 轨迹级门禁为 P_gen 网格一致率 ≥99%，对该缺陷不敏感故未暴露）。处置：v1 冻结
> 资产不回溯修改（冻结纪律），登记为 v1 已知限制；本换型全部参考（tcnq_*）用
> 修复后生成器，验证链不受影响；修入 §18 日志与换型报告。
> ③ **层① 板上全轨迹 INJECT 不可行（T-810 板上实测，2026-09-02 深夜）**：
> REF 后端 TCN C 前向 ≈8.8s/拍（52M MAC double 标量 + cache 关 + dilation
> 访存，DWT 32 位回绕已排除假象：显示 1.66s = 8.83s − 7.16s 逐位吻合；
> 核心时钟 600MHz 正常，CYCCNT/uwTick 判别实验实测）——超出 ems_proto
> 注入会话 2s STALE 契约 4.4×，全轨迹 INJECT 会半数 tick 被 held 污染
> （协议不可行，非数据问题）且纯计算 23.8h。**层①板级证据改为**：
> 板 INFER_TEST ×200 vs keras（LSB_TCN 门，实测 mean 1.3e-5 / max 1.2e-4
> LSB，裕量 ~10⁴×）+ L0 宿主全轨迹网格 1.0000 传递闭合；闭环 (b) 臂
> 缓期（23.8h），用户裁决后继（流式增量 TCN 前向为生产正确形态，
> 按工程隔离规则在新工程实施）。

## Rollout and Migration

```text
T-801 Model/tcn_q/ 部署链（tflite 只读副本 + user_neuralart.json + mpool +
      generate-n6-model.sh + analyze/generate 报告归档）
T-802 export_tcnq_reference.py + export_tcnq 脚本（keras → deploy JSON）+ 口径声明
T-803 gen_fw_params.py 扩展（EMS_TCNQ_*）+ ems_params 重生成 + 三口径一致检查报告
T-804 ems_infer_tcn.c 开发 + L0 差分（T2）→ FAIL 阻断
T-805 NPU 侧：ems_npu_tcnq.c + ems_infer_npu.c 双输入 + CMake EMS_PREDICTOR
T-806 双镜像重构建 + fw_gate 7/7 ×2 + 指纹 + REF 占用实测
T-807 Host 工具 tcnq 变体（gen_reference_int8/gen_host_int8_pgen/test_ctrl_diff）
      + 离线全量复演（INJECT 轨迹 host 侧三层数据预生成）
T-808 板会话（用户在场，阶段隔离）：
      ① SRAM 加载 NPU 镜像（令牌）→ 层② ×200 + INJECT
      ② SRAM 加载 REF 镜像（令牌）→ 层①② 段
      ③ NOR 权重烧录（Level C 令牌，0x70200000 区域，前后读取留证）
      ④ 层② 复测（TCN 权重生效后）→ 层①②③ 全绿闸门
      ⑤ 闭环 4 组 + FR-508 抽测 + 时延实测
T-809 收尾：closedloop_metrics 增量 + 换型报告 + roadmap §8 + README 两级登记 +
      §18 日志 + §15 汇报 + 提交（feat/* 分支）
```

- 板会话前闸门：T-804~807 全绿（离线无板可完成）才进入 808；
- **NOR 烧录时机**：层①/② REF 段与 NPU 段均只需应用 ELF（SRAM）——但 NPU 前向读
  NOR 权重，**TCN NPU 镜像在权重烧录前会 FALLBACK（权重区 CRC 不符）**；故顺序
  设计为：先 REF 镜像层①（不依赖 NOR 预测器权重，REF 前向用 ems_params double 表，
  在 ELF 内）→ 再 NOR 烧录 → 再 NPU 镜像层②/闭环。（备选：若烧录会话受限，NPU
  层②可在烧录完成后于同场执行；顺序细节由 tasks 固化，用户在场裁决。）
- 回退策略：任何阶段失败不影响既有资产；`-DEMS_PREDICTOR=mlpq` + mlpq hex 重烧
  （0x70200000，同 Level C 流程）即完整回滚；git 分支隔离（feat/tcn64-swap）。

## Requirements Traceability

| Requirement | Design Decision | Validation |
|---|---|---|
| FR-701 | `Model/tcn_q/` 目录结构（§Architecture 3）+ generate-n6-model.sh 同构 | T0/T-801：analyze 报告 + generate 日志归档 |
| FR-702 | export_tcnq 双脚本 + JSON schema（§Data Model 1）+ gen_fw_params 扩展 | T1/T-802/803：参数量断言 + spot-check |
| FR-703 | ems_infer_tcn.c（§Architecture 2：结构/求和顺序/契约） | T2 L0 差分 |
| FR-704 | ems_npu_tcnq.c + 双输入填充 + static_assert（§Architecture 3） | T3 构建 + T5 ×200 |
| FR-705 | 槽位契约升格（调用点零改动）+ EMS_PREDICTOR 编译期选择 | T3 + L0（mlpq 模式回归） |
| FR-706 | stedgeai generate 同参 + objcopy 0x70200000 | T0 报告核对 |
| FR-707 | 双镜像 + gate ×2 + 指纹 + 占用实测（§Summary 表） | T3 |
| FR-708 | 三口径机器可读报告（calib_report） | T1 产物 + AC-1 |
| FR-708b | T2 层（keras 参考链 + LSB 门） | T2 |
| FR-709 | 两段式层①（§Testing T4） | T4 |
| FR-710 | ×200 新 LSB 门 + INJECT ≥99% + 层③开环报告性 | T5/T6 |
| FR-711 | T7 闭环 (a)vs(b) 阈值（SOC ≤5e-3 / fuel ≤2%） | T7 |
| FR-712 | T8 矩阵 + metrics 增量 + 公平性带 | T8 |
| FR-713 | T9 时延 + Escalation 路径 | T9 |
| FR-714 | T10 FR-508 抽测 | T10 |
| FR-715 | T11 登记表格口径（导师报告兼容） | T11/§14 |
| NFR-701/702 | §Summary 内存/时延核算表 + 占用实测 | T0/T3/T9 |
| NFR-703 | 全链相对路径 + sha256 溯源 + README 两级登记 | §14 扫描核对 |
| NFR-704 | §Security 阶段隔离 + 令牌 | T-808 授权核对 |

## Risks and Trade-offs

- **R1 REF 镜像内存逼近上限**（表 1.33MB + 缓冲 0.3MB → ~1.95MB/2MB）：缓解 = 构建
  后实测 + float32 表备选（−663KB，语义不变须注明）；触发条件与决策树见
  §Error Handling。
- **R2 dilated conv SW fallback 时延**：analyze 预检已排除（CONV/FC 全 EC，SW 仅
  数据搬运）；残余风险 = SW 搬运算子墙上时间（估算 1–2ms 量级），T9 实测兜底；
  超预算走 FR-713 Escalation。
- **R3 双输入张量装配**：IN_0/IN_1 顺序与 [1,300,1] 布局（channels-last）错位风险
  → static_assert + L0 + ×200 三重捕获；workclf 双输入先例降低风险。
- **R4 TCN 闭环结果劣于 MLP-Q**：预期内可能（int8 口径 TCN 指标本就略劣）；处置 =
  如实记录，换型价值定位不变（Fixed Decisions 前言）；Winner 决策不在本 spec。
- **R5 层① "精确一致"语义变化**：TCN 无 EML，层① 从"vs Simulink 位级"改为
  "host gcc 同源位级 + keras 容差"两段——已在 Q2 裁决并写入 FR-709，报告须明确
  陈述该口径差异（不得暗示与 v1 层① 同锚点）。
- **R6 workclf/门控回归风险**：换型仅触及预测器槽位；WC_TEST 与门控回归由 T5/T10
  覆盖；ems_ctrl.c 零改动为结构性保证（diff 审查项）。
- **R7 metrics JSON 兼容性**：增量条目若与既有 schema 冲突（新增模型键），以"新增
  字段不改动既有键"为纪律，closedloop_metrics.py 的读取方兼容性在 T-809 前用
  dry-run 校验。