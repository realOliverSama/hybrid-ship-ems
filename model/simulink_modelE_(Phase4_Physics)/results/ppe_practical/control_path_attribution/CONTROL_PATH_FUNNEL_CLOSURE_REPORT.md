# CONTROL_PATH_FUNNEL_CLOSURE — 控制路径漏斗逐点闭合报告

**任务**：在正式（epoch-2）执行的**每一个**决策点上量化预测信息→已实现动作的漏斗，判定信息差异消失的精确层位。

**范围纪律**：无 Simulink 重跑、无控制器重设计、无 effect 重算、无 IID/OOD 访问；只读冻结证据（192 份 ctrace + 192 份 plant 轨迹 + 冻结 slx/配置/缓存策略表）。

**决策点**：t≥300 s、10 s 网格，总数 17,288（由 48 航次数据推导；四臂网格逐点对齐）。


## 1. 冻结 runtime 机制（逐行确权，非假设）

- δλ 唯一通路：`ECMS_EHat_Data` 第 3 列（`run_cstar.m:125`）→ `e_hat` 端口（`build_model_e.m:801`, Demux [2,1]）→ `soc_ref_eff = SOC_ref + K_S·e_hat`；K_soc·K_S = 30·(1/30) = 1 ⇒ `λ_eff = max(0.425 + 30*(0.6 + (1/30)*delta_lambda - SOC(t0)), 0.43)`
- λ₀ = 0.425 **常数**（runner 从不改写 `ECMS_Lambda`；只写 `ECMS_K_S`）
- L5 动作 = min_g mf(g) + lam*mbe(g), g in 0:20:1000 (law=6)；SOC/P_load 取自 1 Hz 冻结轨迹（四臂逐位相同）

## 2. 移植验证（对冻结工件自证）

- **策略表逐格自证**：Python 重生成 vs 冻结缓存 `artifacts/t445/policy_table.mat`：Pg **597,960/597,960 格位逐位相同**（max|Δ|=0.0），fuel_kg 一致（max|Δ|=0.0），`config_sha256` 与冻结配置一致 ✓
- λ 轴：MATLAB colon 与 `start+step·arange` 在 333/3020 点差 1 ulp（1.78e-15），不影响 argmin ✓
- `policy_lookup` 索引算术夹具（含 MATLAB round 半程语义、SOC 分区边界、越域只计数）：全部通过 ✓
- 单元格 ↔ 动作函数一致性（400 随机 (λ,P̄,region) 组合）：逐位相同比例 1.0000 ✓
- 说明：本机无 MATLAB 许可（用户明令避免本地登录），故验证以**冻结缓存表逐格位级比对**替代 MATLAB 夹具探针；该表即正式执行所用表（`policy_table_cached` 缓存路径），证据等级不低于调用 `policy_lookup` 本身。

## 3. 漏斗（§3/§4）

| 指标 | C0 vs C1 | C0 vs C2 | C1 vs C2 |
|---|---|---|---|
| 决策点数 | 17,288 | 17,288 | 17,288 |
| 窗可比（非 hold） | 15,852 | 15,852 | 15,852 |
| 窗 hold（tail） | 1,436 | 1,436 | 1,436 |
| D0 窗不同 | 15,852 | 15,852 | 15,852 |
| D1 δλ 不同 | 948 | 1,210 | 603 |
| D2 λ_eff 不同 | 948 | 1,210 | 603 |
| D3 L5 动作不同 | 686 | 659 | 135 |
| D4 已实现功率不同 | 0 | 0 | 0 |
| 窗→δλ 吸收 | 15,154 | 14,892 | 15,249 |
| δλ→λ floor 吸收 | 0 | 0 | 0 |
| λ→动作 吸收 | 262 | 551 | 468 |
| 动作→plant 吸收 | 686 | 659 | 135 |

- C1/C2 的 1,436 个 hold 点为 plan `tail` 段（`winfun_plan` ZOH，δλ 保持）；C1 vs C2 的 1,436 个 "同哈希" 全部是**双方同时 hold**，非真实窗偶合（修正上一版归因报告的表述）。
- 条件化份额（C0vsC1）：窗不同→δλ 不同 5.98%；δλ 不同→λ_eff 不同 100.0%；λ_eff 不同→动作不同 72.4%；动作不同→已实现不同 0.0%。
- 差异幅度（C0_vs_C1）：|δλ| 中位（跨航次中位）0.069；|λ_eff| 中位 0.0687、最大 0.1818
- 差异幅度（C0_vs_C2）：|δλ| 中位（跨航次中位）0.071；|λ_eff| 中位 0.0712、最大 0.1818
- 差异幅度（C1_vs_C2）：|δλ| 中位（跨航次中位）0.010；|λ_eff| 中位 0.0100、最大 0.1150

## 4. §6/§7/§8 条件化贡献

- **§7 optimizer/objective 不敏感（主吸收）**：5.98% 的可比决策点上窗不同而 δλ 相同 → DP 把窗差异吸收在决策前。
- **§6 λ_eff floor 贡献 = 0**：δλ 不同 ∧ λ_eff 相同的点数 = 0（CAS C0vsC1）/ 0（C0vsC2）/ 0（C1vsC2）；双raw低于 0.43 的点数为 0。⇒ **上一版归因报告的 "28% 决策点被 0.43 钳位" 系公式误读**（把 δλ 与 e_hat 双重计入）；正式运行中 λ_eff = λ₀+30(0.6−SOC)+δλ，SOC∈[0.593,0.601] ⇒ 从未触及下限。
- **§8 策略表量化贡献**：λ_eff 不同而动作相同的比例 72.4%（C0vsC1）/ 54.5%（C0vsC2）/ 22.4%（C1vsC2） ⇒ 量化吸收真实存在，但**不构成零效应的主因**（见 §5）。
- SOC 采样约定敏感性（D2/D3 对 SOC(t₀) 小扰动）：
  - offset -0.005 → N(D2)=948, N(D3)=548（基线 948/686）
  - offset -0.001 → N(D2)=948, N(D3)=588（基线 948/686）
  - offset +0.001 → N(D2)=924, N(D3)=362（基线 948/686）
  - offset +0.005 → N(D2)=516, N(D3)=202（基线 948/686）
- 说明：D3 的**具体计数**对 SOC 采样约定敏感（平台边缘效应），但 **N(D3) > 0 且 N(D4) = 0 这一定性结论在所有 offset 下不变**；§5 的机制判定不依赖 D3 的精确计数。

## 5. §9 下游/接线 —— 决定性发现

**逐点 L5 重建证明 Pg\* 在 686 个 instant 上不同，而 D4 = 0（48/48 航次、四臂全通道逐位相同）。**
⇒ 按 §9 的判据，**不能**维持 `NO_EVIDENCE_OF_EXECUTION_PATH_DEFECT`；执行路径存在结构性缺陷。

### 实际进入 plant 的参考值来源（由两个已落盘 PMS 内部通道判别）

| 臂 | 假设 | unserved 逐点精确匹配 | unserved max\|Δ\| | 功率链 ≤5 kW 比例 | 功率链 median\|Δ\| |
|---|---|---|---|---|---|
| C0 | A_baseline_LPF60 | 98.88% | 4.80 kW | 94.5% | 0.13 kW |
| C0 | B_ecms_law6_own_delta | 77.67% | 709.77 kW | 5.0% | 276.45 kW |
| C0 | C_ecms_law5 | 47.02% | 709.77 kW | 2.9% | 394.76 kW |
| C0 | D_ems_law0_target | 96.66% | 563.03 kW | 22.6% | 10.79 kW |
| C1 | A_baseline_LPF60 | 98.88% | 4.80 kW | 94.5% | 0.13 kW |
| C1 | B_ecms_law6_own_delta | 77.16% | 709.77 kW | 5.3% | 276.21 kW |
| C1 | C_ecms_law5 | 47.02% | 709.77 kW | 2.9% | 394.76 kW |
| C1 | D_ems_law0_target | 96.66% | 563.03 kW | 22.6% | 10.79 kW |
| C2 | A_baseline_LPF60 | 98.88% | 4.80 kW | 94.5% | 0.13 kW |
| C2 | B_ecms_law6_own_delta | 77.20% | 709.77 kW | 5.3% | 276.22 kW |
| C2 | C_ecms_law5 | 47.02% | 709.77 kW | 2.9% | 394.76 kW |
| C2 | D_ems_law0_target | 96.66% | 563.03 kW | 22.6% | 10.79 kW |
| P0 | A_baseline_LPF60 | 98.88% | 4.80 kW | 94.5% | 0.13 kW |
| P0 | C_ecms_law5 | 47.02% | 709.77 kW | 2.9% | 394.76 kW |
| P0 | D_ems_law0_target | 96.66% | 563.03 kW | 22.6% | 10.79 kW |

- `P_gen_unserved_kW` 是 PMS 内部对**所选参考值**的纯代数函数（`= max(0, req − N·clamp(req/N,125,500))`）；
- `P_gen_actual_kW` 是参考值经 allocator clamp + 20 kW/s 爬坡 + τ=5 s 惯性后的实际发电。
**基线需求律 `req = LPF_{τ=60 s}(P_load_bus/η_rect)` 在全部 48 航次、四个臂上以 98.88–98.88% 逐点精确命中（max|Δ| = 4.80 kW），功率链 94.5% 落在 ±5 kW 内（median|Δ| = 0.13 kW）；所有替代假设（ECMS law6 用各臂自身 δλ / law5 / EMS law0）误差量级 563–710 kW、功率链命中率 ≤22.6%，被明确排除。**

### 机制（冻结源码证据链）

- 冻结绑定复核：`model/HybridShip_ModelE.slx`（bound_match=True）与 runner/winfun 文件与冻结 sha 一致 ⇒ 本报告检查的 .slx 与 runner **就是正式执行的字节**；冻结包**未绑定**注入路径（scenario_compile/mission_v3/fleet_v3/inject_voyage 均未出现） ⇒ 该路径属**未受冻结保护的执行前置层**。
- `compiler`: scripts/+mission_v3/scenario_compile.m:292-293
- `manifest_values`: names{8}='PMS_SOURCE_MODE' -> 0.0 ; names{9}='EMS_CTRL_LAW' -> 0.0
- `doc`: scenario_compile.m:67 'PMS_SOURCE_MODE = 0 (BASELINE_POLICY, bench convention)'
- `runner_executes_it`: scripts/+ppe/inject_voyage.m:11-15 assignin per entry
- `runner_overrides_only_law`: scripts/+ppe/inject_voyage.m:17 EMS_CTRL_LAW := arm law
- `net_effect`: every formal PPE run (both epochs, all arms incl. P0) compiled the model with PMS_SOURCE_MODE = 0 -> baseline demand policy selected; EMS output discarded at the selector
- 选择器：control >= 0.5 -> in1 (BASELINE); else in3 (EMS)（in2 = or_baseline = PMS_SOURCE_MODE < 0.5  (SID 381 relop '<' const 0.5) ∨ ems_health_ok < 0.5 (SID 382)）
- 为何无告警：PMSSafetyInterlockE: fback_active latches only if nominal_is_ems > 0.5；Constant C_source_mode = PMS_SOURCE_MODE (=0) -> latch disabled；实测 `safety_baseline_fallback_active` 恒为 0。

⇒ **正式 epoch-1/epoch-2 的全部 C\* 手臂（含 P0）中，EMS 输出在 PMS 源选择器处被丢弃，plant 由 PMS baseline 需求律驱动**；四臂轨迹逐位相同是**构造性必然**，与预测信息量无关。

## 6. §10 P0 上下文

- P0（law=5 连续运行，无预测/无 δλ）与 C0 的 λ_eff 不同点数 17286，重建动作不同点数 10570，而实测轨迹逐位相同点数 17288/17288。
- 结论：P0 与 C0 的"相同"同样由源选择器覆盖解释，**不构成**"两者落入同一动作平台"的证据。

## 7. §11/§12 终局分类

- **primary_mechanism**：`DOWNSTREAM_OVERRIDE_OR_SATURATION`
- mechanism detail：L6/L7 PMS source-selector override: PMS_SOURCE_MODE=0 injected by the frozen scenario compiler (mission_v3.scenario_compile) and executed by ppe.inject_voyage -> baseline demand policy selected, EMS output discarded before the dispatch allocator
- **formal_zero_result_classification**：`NOT_VALID_NEGATIVE_ARCHITECTURAL_RESULT__CONTROL_ACTION_EXECUTION_PATH_DEFECT`
- 依据：pointwise L5 reconstruction shows the ECMS action DIFFERS at N(D3)>0 instants while the realized power is bit-identical at every instant; the frozen evidence identifies the applied reference as the PMS baseline demand policy, i.e. the manipulation never reached the plant
- 前次归因结论 `ACTION_QUANTIZATION_OR_POLICY_INSENSITIVITY` + `VALID_NEGATIVE_ARCHITECTURAL_RESULT` **予以撤回**：量化吸收（§8）与 floor（§6 实测 0）都不是零效应的主因；主因是 L6/L7 源选择器把 EMS 参考值整体短路。

## 8. 未修改声明

- 未修改任何正式 effect artifact / FEC / 掩码 / 冻结工件；未重跑 Simulink；未访问 IID/OOD；未读取燃油通道（本模块通道白名单物理上不含 fuel/J_FEC）。
- 本报告 sha256_self: `dbde5bb6dcf476c8dffc6736fe4e82cf472dc7951dc3bd85c14e3c6f7dfcf4ce`
