# T-508 前置判别实验：workclf int8 坍缩根因判别 + MLP-Q 同类问题审计

日期：2026-09-07　执行：delegated subagent（只读仓库源码，只写 `experiments/t508_workclf_calib/`）
脚本：`exp_workclf_calib.py`（路径全部由 `__file__` 相对推导）
Python：`model/simulink/.venv_tf/bin/python`（TF 2.20）
复用方式：`import gen_reference_int8 as gri`（只读；`MirrorCtrl` / `workclf_eval_prep` / `make_interp` / 路径常量 `WORKCLF_TFLITE` / `MLPQ_TFLITE` / `PARAMS_C`，出自 `Host/tools/gen_reference_int8.py` 头部 L47–50；`MirrorCtrl` 语义对应 `ems_ctrl.c` EML ecms_nn 窗口/门控/迟滞层）。未复制任何逻辑、未修改任何源码。

## 0. 管线正确性硬门（先于一切结论）

| 校验项 | 结果 |
|---|---|
| tflite 身份 | workclf `7c3571e4eb7c9f0f`、mlpq `56fc3488ff0b210c` = `gen_reference_int8.py` 冻结常量；`workclf_v1.keras` sha256 `adcc1c3f832de4c8…` |
| T-312 冻结 val 指标复现（`workclf_int8_report.json`） | float acc **0.8439 = 0.8439**；int8 acc **0.8437 = 0.8437**；int8 macro-F1 **0.6182 = 0.6182** → 本实验 float/int8 推理路径与 T-312 评估口径机械等价 |
| MirrorCtrl 特征构造同一性 | E2 逐拍复现既有 Host mode B 参考：S20 **5521/5521**、voyage_002 **4212/4212** |
| SOC 重放一致性 | max\|SOC(MirrorCtrl 重放)−SOC(ref)\| = **0.000e+00**（双轨迹） |
| EMS 复刻有效性 | λ+20 kW 网格搜索 Python 复刻（ems_ctrl.c L135-221/370-377 同式）对参考 `e_hat` 解出的 `Pbest` 与 Simulink `P_gen_ref_kW` 逐拍一致：S20 **5521/5521**、voyage_002 **4212/4212** |

> 注：首轮运行曾以「E3 class6 占比 < 50%」作坍缩代理门并触发。经上表冻结指标复现排除管线 bug 后确认：E3 的多类分布是归一化口径与 raw 口径函数分歧的真实性质，不是 T-312 结论被推翻（T-312 的结论是其归一化口径 val 指标，本实验已逐位复现）。坍缩门改用单类占比 ≥ 0.98（E2 形态），E3/E4 实测最大类占比 0.343 / 0.419，未触发。

## 1. 口径定义

- **raw 口径**（固件/EML 现状，`ems_ctrl.c` EML:82-105 + `gen_reference_int8.py` `MirrorCtrl`）：`pdec30`（30 s 重采样 P，kW）+ `[v_ship, accel]` 物理量直接喂入。
- **归一化口径**（T-312 训练管线）：同一 302 维窗口 `xnorm = (x−X_mean)/X_std`（`WS/datasets/fleet_norm_v2.json`，fit on train split only）后取 `[:300:10]` 与 `[300:]`。
- 轨迹：S20 = `ref_G2_S20_composite_voyage_matrix.csv`（5521 tick）；voyage_002 = `ref_G2_test_voyage_002_v2.csv`（4212 tick，fleet 测试航次 v2，真值可由 `voyages_seed20260829.json` 逐秒精确展开）。

## 2. E1–E4：workclf 类别分布（S20 主判别）

S20，tick≥300（primed；0–298 为 MirrorCtrl 零初始化段单列，不参与结论）：

| 实验 | 口径 × 模型 | c1 depart | c2 harbor | c3 eco | c4 rush | c5 bridge | c6 upstream | c7 berth | 与 Simulink double 参考（cls_raw）逐拍一致率 | 对设计真值 acc* |
|---|---|---|---|---|---|---|---|---|---|---|
| E1 | raw × float keras | 0 | 0 | 581 | 16 | 0 | **4924 (89.2%)** | 0 | **1.0000** | 0.1087 |
| E2 | raw × int8 tflite | 0 | 0 | 13 | **5508 (99.8%)** | 0 | 0 | 0 | 0.0000（= 固件坍缩） | 0.3152 |
| E3 | norm × float keras | 37 | 1892 | 1355 | 1693 | 0 | 544 | 0 | 0.1042 | **0.7598** |
| E4 | norm × int8 tflite | 40 | 1894 | 1358 | 1690 | 0 | 539 | 0 | 0.1032 | **0.7589** |
| ref | Simulink double × raw（既有参考） | 0 | 0 | 591 | 0 | 0 | **4930 (89.3%)** | 0 | 1（本体） | 0.1087 |

\* 设计真值 = `scripts/+ecms2/voyage_s20.m` 设计段表 → 7 类 taxonomy 近似映射（出桥巡航 11 kn→rush、减速进港→harbor，两处歧义已注明）；S20 设计构成：depart 5.4% / harbor 12.0% / eco 23.9% / rush 31.5% / bridge 10.9% / **upstream 10.9%** / berth 5.4%。E2 对设计真值 acc 0.3152 只是「恒输出 rush」撞上 rush 占比 31.5% 的巧合，无含义。

voyage_002（分布内测试航次，设计真值精确展开：depart 8.3% / eco 17.7% / rush 43.4% / bridge 22.1% / berth 8.5%，**不含 harbor/upstream**）：

| 实验 | c1 | c2 | c3 | c4 | c5 | c6 | c7 | 与参考一致率 | 对设计真值 acc |
|---|---|---|---|---|---|---|---|---|---|
| E1 float×raw | 0 | 0 | 488 | 16 | 0 | **3708 (88.0%)** | 0 | **1.0000** | **0.0000** |
| E2 int8×raw | 0 | 81 | 13 | **4118 (97.8%)** | 0 | 0 | 0 | 0.0000 | 0.4340 |
| E3 float×norm | 104 | 1569 | 769 | 1766 | 0 | 4 | 0 | 0.0010 | 0.5978 |
| E4 int8×norm | 101 | 1572 | 773 | 1765 | 0 | 1 | 0 | 0.0003 | 0.5988 |
| ref double×raw | 0 | 0 | 491 | 0 | 0 | **3721 (88.3%)** | 0 | 1（本体） | **0.0000** |

## 3. 判别结论（按任务预设规则）

**E1 非坍缩（class 6 显著出现：S20 89.2%、voyage_002 88.0%，且与 Simulink double 参考逐拍一致率 100%）→ 根因 = 仅 int8 标定错配**（float 模型本身在 raw 口径下「能输出多样化预测」）。

**机理量化**（`t508_int8_structure.json`，QDQ 形态，输入侧 `tfl.quantize` 即校准参数）：

| 输入量化节点 | scale | zero_point | int8 可表示范围 | raw 实际范围 | 后果 |
|---|---|---|---|---|---|
| P_dec30 (1,30,1) | 0.0125731 | −22 | **[−1.333, +1.873]**（归一化单位） | 0–800 kW | **全部 tick 饱和至 +127 → P 通道恒定** |
| V_accel (1,2) | 0.0534951 | −19 | [−5.831, +7.810] | v 0–5.73 / accel ±0.083 | 不饱和，但 raw 数值被当归一化值解释 |

校准集为 T-312 val 前 1000 **归一化**样本（P 通道量程 ±~1.87 恰好覆盖归一化 P）；固件喂 raw P（100–800 kW）时边界 Quantize 全饱和 → Conv P 支路输出恒定 → 分类仅由 [v,accel] 侧输入决定 → argmax 恒 class 4（S20 5508/5521，voyage_002 4118/4212；偶发 class 3/2 由侧输入波动驱动）。**修复方向（机械层面）= 在 raw 口径数据上重标定 int8**。

## 4. ⚠ 意外发现（设计层，需主 Agent 决策）：raw 口径行为本身不做段识别

- E1 与 Simulink double 参考在 primed 拍 **逐拍 100% 一致**（双轨迹）→ raw 口径 float 行为 = EML 冻结参考行为，二者是同一函数。
- 但该行为与 S20 设计段构成严重不符：设计 upstream ≈10.9%，raw 口径输出 upstream **89%**；ref cls_raw 的 19 个连续段（如 6-run 450–3215、3-run 3216–3410、6-run 3411–5142）边界与设计段边界（300/780/900/2100/2220/3120/3720/4440/5040/5220）**完全不对齐**。
- voyage_002（分布内、真值精确、**不含任何 upstream 段**）：raw 口径仍输出 upstream 88%，对设计真值准确率恰为 **0.0000**（一个拍都对不上）。而归一化口径 acc 0.598。
- 归一化口径（E3/E4）远更接近设计构成（S20：eco 25.0%/rush 30.6%/harbor 34.3%/upstream 10.4% vs 设计 23.9%/31.5%/12.0%/10.9%），但仍有明显错误（bridge/berth/depart 几乎不识别、voyage_002 无 harbor 却预测 37%）——它是「在训练口径下正常工作的分类器」，不是理想分类器。

**含义**：任务规则「E1 非坍缩 → 修复 = raw 口径重标定」在机械层成立，但重标定会把固件行为忠实对齐到一个**本身不执行段识别的退化函数**（upstream 恒 89%、voyage_002 全错）。这是部署口径设计问题（raw vs 归一化口径喂入、EML 参考行为是否可接受、是否改为归一化口径喂入或以设计段先验修正参考），按 AGENTS.md §10/§2 属主 Agent 决策范围；本实验只提供量化依据，未改任何源码/常数/模型。

## 5. E5：MLP-Q 审计（int8 vs Simulink double，同归一化口径）

MLP-Q int8 的 q50 先还原 `e_hat_raw = (q50−ebar)/escale`（`ems_ctrl.c` L314-315 同式，ebar=36.8933, escale=20.5070，参数实测自 `ems_params.c`）；`e_hat = K_w·e_hat_raw`。tick≥300 掩码统计：

| 指标 | S20 | voyage_002 |
|---|---|---|
| \|Δq50\| mean / p50 / p99 / max（kWh） | 0.407 / 0.319 / 1.907 / **3.168** | 0.536 / 0.398 / 2.118 / **3.646** |
| \|Δe_hat_raw\|（MLP-Q 纯量化差，e_hat 单位）mean / p50 / p99 / max | 0.0199 / 0.0156 / 0.0930 / 0.1545 | 0.0262 / 0.0194 / 0.1033 / 0.1778 |
| \|Δe_hat\|（含 K_w 门效应）mean / p50 / max | 0.803 / 0.990 / 1.522 | 0.619 / 0.668 / 1.373 |
| \|Δλ\| mean / max | 0.1019 / 0.4936 | 0.0068 / 0.4936 |
| 20 kW 网格翻转 | **3205/5521（60.4%）** | 258/4212（6.1%） |
| K_w 一致拍内翻转 | **1/593** | 0/492 |
| K_w 失配拍内翻转 | 3204/4928（65.0%） | 258/3720（6.9%） |
| K_w 参考（1/0）/ int8 | 593 / 4928；int8 恒 1 | 492 / 3720；int8 恒 1 |

- 未掩码 \|Δq50\| max ≈ 30.7/30.8 kWh 全部集中在 tick≤11（top5 tick = 11/9/10/6/8，双轨迹一致）——MirrorCtrl 零初始化窗口 vs Simulink 平填充窗口的已知伪影（T-506 mode B 同款），掩码后 max 降至 3.17/3.65 kWh。
- **MLP-Q 审计结论**：固件先归一化再喂入与校准口径一致，无 workclf 式口径错配迹象；int8-vs-double 的 e_hat_raw 误差小（p50 ≈ 0.016–0.020，p99 ≈ 0.09–0.10，max ≈ 0.15–0.18 → q50 误差 max ≈ 3.2–3.6 kWh，约 37–60 kWh 量程的 8–10%）。S20 上 int8 与参考的 EMS 行为差异（60.4% 网格翻转）的**主导因素是 workclf 坍缩 → K_w 门失配**（参考在 class 6 拍关闸 e_hat=0，int8 坍缩使 K_w 恒 1），而非 MLP-Q 量化误差（K_w 一致组内翻转仅 1/593、0/492）。

## 6. 产物清单（全部位于本目录，不入库）

| 文件 | 内容 |
|---|---|
| `exp_workclf_calib.py` | 实验脚本（E1–E5，路径 `__file__` 相对推导） |
| `report.md` | 本报告 |
| `run_log.txt` | 完整运行日志（exit 0） |
| `t508_summary.csv` | 77 行机器可读汇总（含 E1–E5 全部统计） |
| `t508_class_distribution.csv` | E1–E4 × {all, tick≥300} × 7 类分布长表 |
| `t508_S20_preds.csv` / `t508_voyage_002_preds.csv` | 逐拍预测 + 设计真值 + 双参考对照 |
| `t508_designed_truth_compare.csv` | 各口径 vs 设计真值分布长表 |
| `t508_S20_ehat_grid.csv` / `t508_voyage_002_ehat_grid.csv` | 逐拍 q50/e_hat/λ/网格解/翻转标记 |
| `t508_int8_structure.json` | int8 激活量化节点自省（scale/zp 证据） |
| `t508_model_identity.csv` | 模型身份 sha |

## 7. 局限

1. S20 设计真值为 `voyage_s20.m` 设计段表的**近似映射**（出桥巡航 11 kn→rush、减速进港→harbor 两处歧义）；但结论（upstream 89% vs ~11%、段边界不对齐、voyage_002 精确真值 acc=0）对映射歧义稳健。
2. tick<300 段（零初始化窗口）单列掩码；MirrorCtrl 零初始化与 `ems_ctrl.c` 平填充在 tick 0–298 语义不同（E1 与参考的 52 拍差异全部在此段，已排除出结论）。
3. 「对设计真值 acc」衡量的是分类器行为 vs 设计段构成，非任务指标；E3/E4 的 0.60–0.76 也不应解读为「归一化口径模型已可用」。
4. 网格翻转粗判基于 20 kW 网格 + Python 复刻（与 Simulink `P_gen_ref_kW` 逐拍 100% 一致），未含迟滞与 K_h 逻辑（报告性）。

## 8. 需主 Agent 决策（非本实验决定）

raw 口径重标定（机械修复）会把固件对齐到本身不执行段识别的退化行为（voyage_002 对设计真值 acc=0.0000）。是否改为：归一化口径喂入 + 重新标定 / raw 口径重训 / 修正 EML 参考行为，属 EMS 设计与研究路线判断（AGENTS.md §10），本实验不替主 Agent 决策。