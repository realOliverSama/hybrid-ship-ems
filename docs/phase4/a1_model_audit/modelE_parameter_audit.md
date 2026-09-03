# Model E 参数审计报告（Phase 4-A1）

> 本文件由 `scripts/render_audit_reports.py` 从 `modelE_parameter_audit.csv` 渲染，幂等可重跑；手工内容仅存在于"假设影响分析"保留块（`<!-- MANUAL:... -->`）。

## 统计摘要

参数总表共 **210 行**（按 (parameter, module) 逐行登记，多处定义不合并）；事实源：`modelE_parameter_audit.csv`（脚本列由 build_parameter_table.py 生成，人工列由复核/回填维护）。

### 按 source_class5（五级来源分类）

| source_class5 | 行数 |
|---|---|
| drawing | 9 |
| vendor | 3 |
| literature | 3 |
| estimated | 127 |
| assumed | 68 |
| pending | 0 |

### 按 impact（对论文结论的影响，启发式预填 + 人工复核）

| impact | 行数 |
|---|---|
| high | 71 |
| medium | 53 |
| low | 86 |

### 按 legacy_caliber（现行口径；UNKNOWN = 无法判定）

| legacy_caliber | 行数 |
|---|---|
| DRAWING | 10 |
| LEGACY | 6 |
| ASSUMED | 48 |
| UNKNOWN | 146 |

## 五级来源默认映射规则（FR-405）

映射为**默认值**，人工复核可对具体行 override 并写明理由（记录于报告）。

| 现行口径（legacy_caliber） | 默认 source_class5 | 说明 |
|---|---|---|
| DRAWING | drawing | v2.2 起部分"真实设备参数"人工复核可升/改为 vendor |
| LEGACY | estimated | 备注"继承原模型 HybridShip0730，原始依据待追溯"；查到依据后升级 |
| ASSUMED | assumed | — |
| UNKNOWN（无法判定） | pending | 备注必填（Gate 校验），不得猜测归类 |

## 假设影响分析（重点假设清单）
<!-- MANUAL:assumption_impact_analysis -->

### 十类重点假设逐项分析（FR-406）

| # | 假设项 | 现状（参数表依据） | 对论文结论的影响机制 | 影响等级 |
|---|---|---|---|---|
| 1 | 船体阻力 Hull_K_R | `Hull_K_R` estimated/high（派生自设计点，v2.0/v2.1 文献区间交叉校核；排水量 Δ≈500t 用户工作假定未确认，Δ∝K_R 缩放关系已论证不影响闭环节油率结论） | 直接决定 speed-power 曲线形状；但节油率是功率量同比例缩放下的无量纲比值，方向性结论稳健 | 中 |
| 2 | 排水量 Ship_Mass | 1.0e6 kg LEGACY→estimated/high | 影响加速暂态与惯性能量回收窗口；对稳态油耗无影响 | 中 |
| 3 | 吃水 T | **参数不存在**（T-404 ADD 依赖） | Model E 浅水效应的输入之一；缺失则 UKC 无法计算 | 高（对 Model E 新物理） |
| 4 | 水深 h | **参数不存在**（T-404 ADD 依赖） | 工况 v3 浅滩语义的物理基础；决定浅水附加阻力 | 高（对 Model E 新物理） |
| 5 | 螺旋桨参数 | K_Fw/K_Fv/K_Qw/K_Qv/wake_fraction 全 estimated/high；D、K_T/K_Q 图谱缺失（propeller_decision: keep_simplified） | 影响推进效率链与转速-功率映射；A2 以 ±20% 敏感性验证界定结论稳健域 | 高 |
| 6 | 电机效率 Elec_Eff | 0.92 固定值 LEGACY→estimated/high（T-405 候选 f(n,T)；无可信数据则保持 constant——纪律见路线文档 T-405） | 恒定效率抹平部分负荷损耗差异，可能轻微低估低负荷工况能耗 | 中 |
| 7 | 发电机最低稳定负荷 | `ASSUMED_Gen_MinStablePower=0` assumed/high——物理不可信（柴油机不可 0 kW 稳定运行） | **直接决定 ECMS/PMS 的低负荷分配策略与'一台 vs 两台'经济性结论**（T-407/408/409 核心）；data_request_list B 节优先索取项 | **极高** |
| 8 | 发电机启停过程 | 参数不存在于 Model D；Ecms_Phase0_Config 中 `cfg.C_start_kg=2` assumed/high | 启停代价决定 PMS v2 启停阈值与滞环设计；假设值过大/过小直接改变最优启停频次结论 | **极高**（对 B 包） |
| 9 | 电池内阻/OCV | **参数不存在**（T-410 ADD 依赖）；现仅有充放电效率 0.95 assumed/high | 1RC ECM 的 SOP 精度依赖 R_0/OCV；缺失则电池 SOP 只能停留在 SOC 降额口径 | 高（对 Model E 新物理） |
| 10 | 辅机负荷 P_hotel_nom | 100 kW DRAWING→estimated/high（旧审计 LEGACY: EMS_P_Hotel；真实辅机负荷谱待索取） | 辅机占比影响基线负荷水平与电池缓冲余地；constant 假设抹平 mission load 变化（C 包 T-425 应急工况需可变 P_hotel） | 中 |

### 最可能影响论文结论的假设（明确清单）

按"假设错误 → 结论方向可能改变"的严格标准排序：

1. **发电机最低稳定负荷 = 0**（#7）——当前模型允许发电机 0 kW 运行，使"电池优先、机组跟随"策略显得比现实更优；真实最低稳定负荷（典型 25~40% 额定）会强迫低负荷充电或启停切换，**直接改变 ECMS 与 PMS v2 对比的定量结论**。
2. **BSFC 低负荷段形状**（cfg.bsfc_tiers_d25 抛物线族，assumed/high）——节油机理本身就是 BSFC 非线性（roadmap §10 逻辑链起点），低负荷段斜率假设决定运行点搬移的收益上限。
3. **发电机启停代价**（#8）——B 包 PMS v2 与 F 包机组组合优化的核心输入。
4. **螺旋桨参数族**（#5）——推进链效率，A2 敏感性验证兜底。

第 1、2 项同为 `docs/data_request_list.md` B 节优先索取项，建议用户回填时最先处理；第 3 项随 B 包启动前回填。

### 回填与复核纪律

- 用户分批回填 `user_value` / `user_reference` 两列；回填**不改变**本报告已批准结论，但可触发 A2 spec 范围微调（如螺旋桨结论翻转）——走 A2 brainstorm 修订；
- "不改假参数凑指标"红线（`docs/optimization_backlog.md` §1）继续生效：本阶段只登记与分类，不替换任何参数值；
- `build_parameter_table.py` 重跑幂等保留人工列（已验证：override 与 impact 修订在重跑后原样保留）。
<!-- /MANUAL:assumption_impact_analysis -->
