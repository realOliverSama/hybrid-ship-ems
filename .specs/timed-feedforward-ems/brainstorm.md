# Model C: 定时前馈 EMS（Peak + Time-to-Peak 预测调度） Brainstorm

Spec: `timed-feedforward-ems`  
Status: brainstorm-complete  
Created: 2026-08-19

## User Intent

## 用户意图
在 hybrid-ship-ems 项目中开展新研究课题：基于"峰值幅值 ΔP_peak + 到达时间 t_peak"双输出负荷预测的定时前馈能量管理（Timed Feedforward EMS），目标是相对现有"单峰值即时前馈"方案降低甲醇发电机燃油消耗，同时不降低直流母线电压安全性。

## 关键确认（逐轮问答结论）
1. **机组接口**：06_Genset_Average_Plant 为"目标电平 P_gen_ref + 内部 Rate Limiter(±40 kW/s) + 一阶惯性(τ=5s)"，无定时启动机制；定时启动完全在 EMS 层通过对前馈项的时间门控实现，机组接口不变。
2. **定时启动律**：t_start = t_peak − ΔP/r_max − τ_margin；t < t_start 时 P_gen_ref = baseline（负荷跟随+SOC修正），t ≥ t_start 时 P_gen_ref = max(baseline, P_load + ΔP)。
3. **节油机理**：现有即时前馈在峰值到达前多发功率，被下垂母线上的电池吸收，产生充放往返损耗；定时启动压缩"空等发电"时间。燃油按 ∫P_gen×BSFC(P_gen)/3600 积分。
4. **退化策略（用户认可）**：t_peak 小于爬坡所需时间（来不及）时退化为 C1 即时响应；SOC 低时 t_start 加保守偏置提前。
5. **模型选型**：现有 MLP 单峰值输出不能直接用于 C2，必须重训双输出。控制律研究与模型选型解耦为两个 Phase。
6. **工程结构**：在 model/ 下新建 simulink_modelC/ 副本，排除 .venv_tf(1.6G，符号链接复用)、slprj、results；副本内全部改用独立 Model C 命名（含由 Model B 复制而来的文件与内部引用），消除路径依赖；新构建脚本独立编号，不改写 build_model_b.m，EMS 新增变体而非修改现有变体。
7. **subagent**：用户明确授权在合适的时候调用 subagent。

## 研究方案
### 对照定义
- C0 Baseline EMS（无预测）
- C1 即时前馈（现有 EMS_AI_Enhanced：Peak → 立即抬 P_gen_ref）
- C2 定时前馈（Peak + t_peak → t_start 时刻才抬 P_gen_ref）

### Phase 1 — 控制律验证（不依赖新模型，oracle 先行）
扩展 AI_Pred_Source=1 (FromWorkspace) 接口为 [t, P_peak, t_peak] 三列：
- E0: 无预测 + C0（基线油耗）
- E1: oracle + C1（即时前馈油耗上界）
- E2: oracle + C2（C2 节油潜力上限；核心假设检验门）
- E3: 现有 MLP 幅值 + t_peak 真值 + C2（幅值误差鲁棒性）
**通过门**：E2 显示 C2 相对 C1 有显著节油（具体阈值在 requirements 中定义），否则不启动 Phase 2。

### Phase 2 — 双输出模型选型（E2 通过后）
训练 3 个双输出 (ΔP_peak, t_peak) 模型：MLP-dual (110→64→32→2)、CNN1D-dual、TCN-dual (k=3, d=1/2/4，主候选)。选型指标：ΔP MAE、t_peak MAE、INT8 量化损失、Neural-ART NPU 映射率。赢家回灌 Simulink 闭环替换 E3 预测源，得端到端结果；后续接 STM32N6 INT8 部署。

### 主评价指标
航程等效燃油消耗 fuel_total_kg（主）；安全约束：Vdc_min 不低于 Baseline、SOC 终端偏差受限；辅助：电池 RMS/峰值功率、爬坡饱和时间。

## 遗留事项
- E2 通过门的具体节油阈值、τ_margin 取值、SOC 保守偏置档位在 requirements/design 阶段定量化。
- t_peak 预测误差的闭环敏感性分析（sweep t_peak 偏移）纳入 Phase 1 或 Phase 2 待 requirements 确定。

## Clarifying Questions Asked

- TODO: Record key questions asked one at a time and the user's answers.

## Proposed Approach

- TODO: Summarize the recommended direction before requirements are drafted.

## Assumptions

- TODO: List assumptions accepted during brainstorming.

## Remaining Doubts

- TODO: List unresolved questions, if any, and whether they block requirements.

## Decision

- [ ] User approved creating the spec skeleton and drafting requirements.
