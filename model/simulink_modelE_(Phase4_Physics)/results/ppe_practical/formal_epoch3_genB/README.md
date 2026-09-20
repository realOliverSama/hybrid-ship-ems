# formal_epoch3_genB — EPOCH-3 世代 B 正式战役语料

EPOCH-3 generation-B（genB）4 臂对照实验的**权威正式语料**与验收工件。
冻结根 `a2dc634f…`（= `freeze/epoch3_pre_execution_freeze_genb.json` 文件 sha256），
执行 commit `ac618ae0`，全部 192 条 terminal 绑定该冻结。

## 目录内容

- `FLTA_*` 航次子目录 × 若干：genB terminal 证据，每 (voyage, arm) 一组
  `<ARM>.json`（terminal 记录：fuel_raw_kg / J_FEC_kg / SOC0 / SOC_end / 绑定与遥测哈希）、
  `<ARM>.traj.v1.mat` + sidecar（1 Hz 轨迹，FC-3 允许通道集）、
  `<ARM>.audit.v1.json`（原生审计）、`<ARM>.ctrace.v1.json`（10 s 控制决策
  delta_lambda + forecast window；P0 applies=false）、
  `<ARM>.sourcesel.v1.mat` + sidecar（源选择遥测，1 Hz）。
  注意：部分航次的部分臂落盘在世代 A 路径 `../formal_epoch3/<voyage>/` 下
  （同样绑定 a2dc634f），分析按 (voyage, arm) 跨两 namespace 合并去重。
- `EPOCH3_GENB_FORMAL_ACCEPTANCE.json` — 最终验收工件（G1–G9 九项门禁，
  overall_pass=true；对账 192/192、FC-3 v3 重判 192/192 FEASIBLE、
  freeze gate、效果分析、占用率、清单身份、Attempt-A 处置、模块身份、无变异）。
- `_analysis/` — 效果分析（描述性统计 + 预注册配对分析）与占用率描述工件：
  - `effect_analysis_primary_epoch3.json`：逐航次 B_perfect / B_predicted /
    L_prediction（分母 J_C0）与恒等式残差
  - `effect_analysis_group_tier_epoch3.json`：split_group → tier → FULL_COHORT 聚合
  - `effect_analysis_bootstrap_epoch3.json`：冻结种子 bootstrap CI、R 诊断、
    Wilcoxon 补充检验
  - `effect_analysis_fec_sensitivity_epoch3.json`：预注册 k_eq 0.8×/1.2× 敏感性、
    break-even 与带内符号翻转审计
  - `effect_analysis_context_epoch3.json`：P0 上下文配对对比（分 tier、链式估计、
    Wilcoxon）与 SOC 摆动审计
  - `occupancy_descriptive_epoch3.json`：P_gen_ref 带 125/500 kW 占用、
    N_dispatch、λ 网格分布、源选择/fallback/clamp 频率
  - `effect_analysis_acceptance_epoch3.json`：效果分析自身验收
  - `scripts/`：生成上述工件的三个脚本（bytes 与工件 bindings 中的 sha 一致）
- `fc3_authority_declaration.json` / `known_failure_regression_matrix.json`：
  FC-3 权威声明与已知失败回归矩阵（先前提交）。

## 边界

- `../formal_epoch3/`（世代 A namespace）为 **Attempt-A 封存证据**
  （`_audit_stop_gate/ATTEMPT_A_EXCLUSION_LEDGER.json` 判定
  INVALID_FOR_FINAL_EFFECT_INFERENCE），其非 genB terminal 不属于本语料，
  任何分析不得混入。
- terminal 内记录的 `hard_feasibility`（v2 静态语义）已被 FC-3 v3
  动态包络权威重判取代（192/192 READJUDICATED_FEASIBLE）。

入库策略：正式实验语料，入库（与 formal_epoch2 先例一致：traj.mat 全量入库）。
