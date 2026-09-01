# DRAFT — workclf 部署口径修复设计修订草案（路线 C 主案 / 路线 B 对照）

> **状态：DRAFT — 待用户批准，未批准不得实施任何修改。**
> 起草：2026-09-07（主 Agent，三路只读侦察汇总：固件锚点 / EML 参考链 / 路线 B 资产盘点）
> 基准：`docs/agent/AI_MODELING.md` §22（口径契约事故）+ `m2_port_report.md` §6/§8 +
> `m2_workclf_calib_report.md`（E1–E5 证据）+ `.specs/ems01-chil-ecms-v1/design.md`
> 修订对象：`.specs/ems01-chil-ecms-v1/design.md`（§Error Handling / §Testing / R8 口径声明）

---

## 0. 问题重述（一句话）

workclf_v1（唯一分类器模型，T-312 训练于**归一化口径**）被接在 **raw 物理量喂入**链路
（EML:82-105）上：double 形态（EML 参考 + REF 后端）退化为不识别段的函数（S20 upstream
89% vs 设计 10.9%），int8 形态（部署 NPU）因标定集口径是归一化样本而**量化饱和坍缩**
（class 6 出现 0 次）。三个口径（训练 / 部署喂入 / int8 标定）不一致。

## 1. 修订目标与"修复后正确"的定义

- **G-A1（口径契约闭合）**：训练口径 == 部署喂入口径 == int8 标定口径，三方一致
  （AI_MODELING §22.3 规则 1–3）；
- **G-A2（移植保真叙事保持）**：层① "固件 REF vs Simulink 参考" 门禁继续有意义
  ——EML 参考与固件**同步**修改喂入口径（C1 方案），层① 预期恢复 ~100% exact；
- **G-A3（门控功能合理化）**：部署形态 upstream 判定对齐训练口径行为（对设计真值
  acc 预期 ≈ E4 实测 0.76/0.60），预测门控 ê 生效时段占比从 EML 现状 ~11% 回到
  分类器真实判定水平；
- **G-A4（不虚报）**：本修订**不解决** T-308 已知的分类器能力缺陷（bridge/berth
  recall=0、switch_window_recall 0.2886、transition latency median 360s）——那属于
  模型改进（路线 B 方向），单独立项。

## 2. 路线 C 技术方案（主案）：固件 workclf 喂入改归一化特征

### 2.1 固件修改点（行号级，侦察证据）

**唯一控制路径构造点** = `EMS01/Appli/App/ems_ctrl.c:325-331`（全固件仅此一处从控制状态
构造 workclf 输入）：

| # | 位置 | 现状 | 改为 |
|---|---|---|---|
| C-1 | `ems_ctrl.c:326-329` | `pdec30[j] = xP[10*j]`（raw） | **删除独立 pdec30 构造**，直接复用 `:293-296` 已算好的 `xnorm[302]`：`pdec_norm[j] = xnorm[10*j]`（j=0..29） |
| C-2 | `ems_ctrl.c:331` | `ems_infer_workclf(pdec30, in->v_ship, accel, ...)` | `ems_infer_workclf(pdec_norm, xnorm[300], xnorm[301], ...)`（xnorm[300/301] 即归一化 v_ship/accel，由 x 布局 [xP;v;accel] 直接得出） |
| C-3 | `ems_ctrl.c:324` 注释 | "EML:81-134 G2 工况识别（原始物理量输入，EML:82）" | 更新为归一化口径说明 + 修订记录引用 |

索引序一致性已三方验证（侦察证据）：固件 `xP[10*j]` 升序 == MirrorCtrl `xP[::10]` ==
T-312 训练切片 `X[:, :300:10]` + `X[:, 300:]`（`train_workclf.py:63-66`、
`gen_reference_int8.py:200-204`），同向升序无重排，窗口展开无补零（平 fill 初始化）。
**E4 实验的喂入切片与 C-1/C-2 改后的固件喂入在索引序上完全一致**——即 E4 结果
（int8×归一化 对设计真值 acc 0.7589/0.5988，与 float×归一化 E3 几乎逐拍一致）就是
本修订后部署形态的预期行为实证。

### 2.2 接口与后端（无需改动，仅文档）

- `ems_infer.h:34-35` 签名不变（形参类型不携带口径语义）；**必须改**注释：
  `:8-11`（"识别器不吃归一化"声明作废）与 `:30-32`（"原始 Pdec"描述）——改为
  "归一化域输入（xnorm 切片），与 T-312 训练口径一致（2026-09 修订）"。
- 两后端确认"喂什么算什么"（侦察）：REF `ems_infer_ref.c` 全程直接乘加、无归一化逻辑
  （grep NORM/MEAN/STD 零命中）；NPU `ems_infer_npu.c:131-140` 仅 double→float 拷贝，
  输入尺度完全由 tflite 图内 QDQ 决定。**两后端代码零改动。**

### 2.3 int8 标定复核（预期免重标定，留验证步骤）

- 现标定集 = T-312 val 前 1000 **归一化**样本（`t312_workclf_int8.py:38,163-166`），
  与修订后部署喂入口径**天然一致** → **预期无需重标定**（E4 已实证该口径下 int8 行为
  正常）。
- 实施时补一步**可表示范围核对**（AI_MODELING §22.3 规则 3 的量化版）：对 S20 +
  voyage_002 的归一化特征实测 min/max，核对 `P_dec30`（scale=0.01257312, zp=−22，
  ≈[−1.333,+1.873]）与 `V_accel`（scale=0.05349511, zp=−19）的 int8 可表示范围，
  报告饱和 tick 占比（E4≈E3 近乎逐拍一致提示实际无有效饱和，但须留证据）。

### 2.4 EML 参考同步（C1 方案，保层①叙事的关键）

- EML = 内嵌 `Stateflow.EMChart`（无外部 .m 依赖）；G2 文本正本 =
  `WS/results/ecms_phase25/gating/eml_ecms_step_mode2.m`，由
  `WS/python/export/gen_eml_gate.py` 生成（G2 段在 `@@G2BEGIN@@/@@G2END@@` 之间），
  经 `scripts/63_integrate_gate_v2.m`（Stateflow API `target.Script=newCode`）注入。
  **全程脚本化，无 GUI**。
- 修改点：`gen_eml_gate.py` G2 段 `:82` `Pdec = xP(1:10:300)` → `Pdec = xnorm(1:10:300)`
  （xnorm 在 `:64` 已存在）；`:131` `s8 = max(F1W'*[v_ship;accel]+F1B,0)` → 归一化
  v/accel。注意：mode0/1/3 变体同源生成，是否同步改属**决策点 D-2**（建议同步，
  避免变体间口径分裂）。
- `ecms_nn_dbg.m` 诊断副本由 `66_export_fw_reference.m:101-124` 文本变换产出，依赖
  4 个唯一锚点（`:108-113` 各须恰好 1 次）——修改 G2 文本后须确认锚点仍唯一。

### 2.5 参考再生成连锁（🟡 门禁集中在两处）

顺序执行（全部脚本化，命令为草案）：

```text
a. python/export/gen_eml_gate.py                    # 重出 4 变体（含新喂入）
b. matlab -batch "GATE_MODE=2; eval(fileread('scripts/63_integrate_gate_v2.m'))"
c. matlab -batch "EXPORT_TEST_VOYS=2; eval(fileread('scripts/66_export_fw_reference.m'))"
   # 66 自包含：重注入 chart + 重写 ecms_nn_weights_v2.mat + 导出 ref_G2_*.csv/meta
d. ⚠ 66 的 REPRO 门（:312-331，|Δfuel|<1e-3 kg / |ΔSOC|<1e-6 对比 64_run_matrix_v2.m
   缓存）必然 FAIL（EML 行为变了）→ 须重跑 64（至少 G2 两 run）刷新
   gating/runs/*.mat → 连带 gate_result_v2.json 的 G2 消融判定变化
   → 【🟡 决策点 D-3：Golden Baseline 连锁重建批准】
e. L0：test_l0_trace_diff.py 重放（glob 全部 ref_*.csv 自动纳入）→ 重写
   l0_diff_report.* + l0_deviation_*.csv（门禁 ≥99% 同网格/max≤20kW 不变）
f. Host 参考：gen_reference_int8.py --trace（模式 B，MirrorCtrl 同步改 ：490-494
   为 norm 切片）→ gen_host_int8_pgen.py 重出 pgen CSV → 派生件全链刷新
```

- **v1 基线冻结**：旧 `ref_G2_*.csv` 已入库（e2350042），无内容指纹硬引用
  （fw_gate 零引用，侦察确认）→ 冻结 = 显式记录基线 commit SHA + meta_*.json +
  gate_result_v2.json 归档（【决策点 D-4：是否加指纹门】）。
- 版本化治理：修订实施后 spec 建议走 **v1.1 amendment**（design.md 修订记录 +
  requirements 不变），已关闭的 T-310 gate 证据**不覆盖**、以新 run_id 版本化并存
  （【决策点 D-5】）。

### 2.6 工具链同步清单（侦察确认的完整面）

| 文件 | 改动 |
|---|---|
| `Host/tools/gen_reference_int8.py` | MirrorCtrl `:490-494` workclf 输入 raw→norm 切片；mode A `cls_raw/cls_norm` 列语义对调说明（修完后"固件口径"=norm）；头部注释 |
| `Host/tests/test_ctrl_diff.py` | Python Mirror `:351-352` 同步 norm；`test_workclf_mirror :829-846` 直喂用例输入量纲改归一化域随机量 |
| `Host/tests/test_driver_int8ref.c` | 结构不变（stub 按 tick 返回 CSV 值，与喂入口径解耦）；仅注释 |
| `Host/ems_inject.py` / `ems_fault_inject.py` | 零改动（轨迹/协议层，与口径解耦）；⚠ `ems_app.c:150-176` CMD_WC_TEST 直喂主机载荷——M1 式 WC 一致性测试的载荷语义将变为归一化切片（记录到重验证计划） |
| `gen_fw_params.py` | 零改动（EMS_NORM_X_MEAN/STD[302] 已全量存在，`ems_params.c:7422/7501`，来源 fleet_norm_v2.json，`:207-215` 断言齐全） |

### 2.7 重验证计划（板会话需用户在板前，PC 部分可远程）

1. PC：双镜像重构建（REF/NPU）+ fw_gate 7/7；L0 差分（新参考，门禁不变）；
   `--selftest` ×3；
2. 板（用户在场 + 令牌）：层① REF 注入（预期 ~100% exact 恢复）→ 层② NPU vs
   host-int8（≥99% 门禁不变）→ 层③ vs 新 Simulink 参考（**预期大幅改善**：K_w 失配
   消除后分歧只剩 int8-vs-double 量化噪声，参照 K_w 一致组 1/593 翻转基线）→
   分类器对设计真值 acc 复测（预期 ≈E4：0.76/0.60，双轨评估 §22.3 规则 5）→
   FR-508 四场景回归（喂入口径变更不触及回退状态机，仍须回归留证）。

## 3. 路线 B 对照案：raw 口径重训分类器（保持固件 raw 喂入）

**核心差异**：B 不改固件喂入（保持 EML:82-105 raw），改的是**模型本身**——用 raw 口径
特征重训 + raw 代表集重量化。EML 参考同样要换新权重（FR-511 流程：chart 内嵌权重字面量
更新 → 66 重导出 → 参考换血），因此 **B 同样触发 2.5 节的 🟡 再生成连锁**（差别：
C 改的是 chart 代码文本，B 改的是 chart 权重字面量 + 固件 tflite + 冻结常数）。

### 3.1 可复用资产（侦察确认）

| 资产 | 复用方式 |
|---|---|
| 段标签 `segment_labels/seg_labels_v2.npz` | **直接复用**（标签按窗口锚点时刻段型定义，与特征口径无关） |
| 数据划分 seed=20260829（280/60/60） | **禁止换**（AI_MODELING §6/§18 可比性红线） |
| 模型结构/训练循环/指标/switch-latency 评估 | `train_workclf.py:39-49,62-88,117-129` 原样 |
| int8 转换框架（QDQ/代表集机制/float-int8 对照/escalation guard） | `t312_workclf_int8.py` 克隆，仅代表集换 raw 样本（MirrorCtrl raw 特征构造可直接当代表集生成器） |
| raw 窗口数据 | `fleet_windows_v2.npz` **只存归一化 X**——最短路径 = 反归一化 `Xr = Xn*x_std + x_mean`（z-score 严格可逆，float32 舍入内等价于原始 raw；mode A 已用同式产出 vectors_raw.csv） |

### 3.2 必须新写/决策

- `train_workclf_raw.py`（prep() 换 raw 切片）+ `t312_workclf_int8_raw.py`（raw 代表集）；
- **冻结常数更新**：`gen_reference_int8.py:43-44` tflite sha16 + T-312 冻结指标——
  换模型即断旧指纹，属 design 层动作（FR-511 + 🟡）；
- T-312 缺陷史警示：`workclf_v1_float.tflite` 曾误标 int8（已留证）——B 的导出段
  必须沿用 T-312 的量化防呆；
- 工作量粗估：T-308 训练 40 epochs/BATCH 256（中等规模，单卡可跑）+ 全部署链重走
  （≈T-308+T-312+T-505~T-508 验证子集）。**B 的真实价值不在修口径 bug（C 已修），
  而在借重训同时改善分类能力（bridge/berth recall=0、switch latency）**——若只做
  "raw 口径重训而不改模型/数据方案"，acc 预期与 C 相当（模型一样、信息等价），
  论文增量有限；B 要有贡献必须连带特征/标签/模型方案改进 → 实为独立研究项。

## 4. C vs B 对比与推荐意见（供用户裁决，不代替决策）

| 维度 | 路线 C | 路线 B |
|---|---|---|
| 问题性质 | **集成 bug 修复**（口径契约） | **模型/训练方案改进**（研究项） |
| 固件代码改动 | 1 处构造点 + 注释 | 0（保持 raw 喂入）+ 新 tflite |
| EML/chart 改动 | 喂入文本（gen_eml_gate G2 段） | 权重字面量（FR-511 流程） |
| int8 重标定 | **不需要**（现标定集=归一化） | 需要（raw 代表集） |
| 冻结常数 | 不动 | sha16 + T-312 指标全换 |
| 🟡 再生成连锁 | 有（2.5） | 有（同等规模） |
| 修复后分类能力 | ≈E4：0.76/0.60（模型不变） | 取决于新训练方案（仅换口径则 ≈C） |
| 论文叙事 | "口径契约事故与修复"（工程贡献，已有完整证据链） | "部署对齐训练 + 分类器改进"（研究贡献，需新实验） |
| 风险 | 低（E4 已实证目标行为） | 中（重训结果不确定 + 全链重走） |

**推荐：分两步走。第一步实施 C**（bug 修复，最小爆炸半径，恢复部署形态与门禁体系的
一致性，证据链现成）；**B 作为独立研究项另行立项**（若论文需要分类器能力贡献，
B 应连带特征/模型方案改进一起设计，而非仅换口径重训——那与 C 等价且白付重训成本）。
两步不冲突：C 落地后 B 在其上做（B 的重训产物走 FR-511 换模型流程）。

## 5. 待用户决策点清单

| # | 决策 | 建议 |
|---|---|---|
| D-1 | 批准路线 C 实施？ | 批准（本草案主案） |
| D-2 | mode0/1/3 变体是否随 G2 同步改归一化喂入？ | 同步（避免变体口径分裂） |
| D-3 | Golden Baseline 连锁重建批准（64 缓存刷新 + gate_result_v2 G2 判定重算） | 批准（修订必经） |
| D-4 | 旧参考 v1 基线冻结方式（commit SHA 记录；是否加 CSV 指纹门） | SHA 记录必做；指纹门建议加（防"换血无感知"复发） |
| D-5 | 已关闭 T-310 证据的处理（版本化并存 vs 覆盖） | 版本化并存，不覆盖 |
| D-6 | 路线 B 是否立项（独立研究项） | 留待 C 落地后评估论文需要 |
| D-7 | 修订实施后的板会话时间（需用户在板前） | 由用户排期 |

## 6. 实施任务分解预览（批准后展开为 tasks）

```text
T-A1 ems_ctrl.c C-1/C-2/C-3 + ems_infer.h 注释 + L0 工具链同步（test_ctrl_diff 等）
T-A2 gen_eml_gate.py G2 段（+变体，D-2）→ 63 注入 → 锚点唯一性校验
T-A3 66 参考重导出 + 64 缓存刷新（D-3）→ 新 ref_G2_*.csv/meta 归档 + v1 基线冻结（D-4）
T-A4 Host 参考重生成（模式 B norm 口径 + pgen）→ L0 差分门禁复跑
T-A5 双镜像重构建 + fw_gate 7/7 + 绝对路径扫描
T-A6 板会话重验证（2.7，需用户在场）：层①②③ + 设计真值 acc + FR-508 回归
T-A7 M2 报告增补（修订后数据）+ AI_MODELING §22.4 路线决策记录 + 文档同步
```

## 7. 风险与开放问题

- R1 `V_accel` 通道归一化后实际值域 vs int8 可表示范围——2.3 验证步骤覆盖（E4≈E3
  强提示无有效饱和）；
- R2 `66` 的 dbg 文本变换 4 锚点在 EML 修改后须仍唯一（`:108-113` 校验自带，失败即拒
  绝变换，属安全失败）；
- R3 REPRO 门失败是**预期行为**（EML 行为变更的直接后果），处理路径已在 2.5.d 固化；
- R4 归一化喂入改变了 `EMS_NORM_*` 常数的"消费者"范围（mlpq 专用 → mlpq+workclf），
  gen_fw_params 无需改，但参数溯源文档应更新；
- R5 本修订后，`m2_port_report.md` 的层①②③数据成为 v1 基线证据（不覆盖，修订后
  新报告以 v1.1 增补形式并存）。
