# EMS01 CHIL v3 — soft-HIL 真闭环实验 Tasks

Spec: `ems01-chil-v3-closedloop`  
Status: implementation-complete
Created: 2026-09-08  
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`
Design: `./design.md`（design-approved 2026-09-08）

## Implementation Plan

任务按里程碑依赖序排列（M-A→M-F）；阈值与 OQ 定案见 design §5（已随 design 批准）。
产物位置与命名遵循 design §10；全部脚本相对路径（§20.2）。

### M-A 被控对象子集模型 + G-P1（AC-601）

- [x] 1. 被控对象子集构建脚本
  - ID: T-601
  - Requirement(s): FR-601, NFR-604, AC-601
  - Files/areas: `model/simulink_modelD_(EMS_NN_AECMS)/scripts/20_build_plantstep_model.m`（新增，复用 `+model/build_model_c.m` 函数族）；`scripts/` 目录 README 登记
  - Notes: 构建新变体 `HybridShip_ModelD_PlantStep.slx`——12 子系统进、05_EMS_Controller 替换为 Inport `P_gen_ref_kW`、Outport 11 项（t_s + 6 状态 + fuel_total_kg/P_bat_kW/P_prop_kW/voyage_stop）、求解器 Fixed-step ode3 @0.01s、参数单一来源 `Model_Parameters.m`；主模型 `HybridShip_ModelD_DCBus.slx` 零改动（Do Not Change）
  - Validation: 脚本重复执行幂等；结构审计断言（子系统清单 + 边界 1 入 11 出）机器可读 PASS；`git status` 确认主模型文件零改动
- [x] 2. G-P1 门禁（S20）
  - ID: T-602
  - Requirement(s): FR-604, AC-601
  - Files/areas: `model/.../scripts/22_run_gp1_gate.m`（新增：ref_G2 P_gen 序列回放驱动子集模型解释执行 → 逐拍对照 SOC/v_ship/x_ship/P_load + fuel 终值 vs `export_summary.json` → 写 gate JSON）
  - Files/areas: 产物 `EMS01_CHIL_ECMS/Host/reference/v3_gp1_S20_*.json/csv`（不触碰 v1.1 冻结文件）
  - Validation: `v3_gp1_S20_gate.json` overall=true（阈值 §5.2：逐拍 max|Δ| ≤1e-6、fuel 相对 ≤1e-6、无数值健康异常）
- [x] 3. G-P1 门禁（voyage_002）
  - ID: T-603
  - Requirement(s): FR-604, AC-601
  - Files/areas: 复用 `22_run_gp1_gate.m` 扩 voyage_002（VOYAGE_* 注入路径首次实测）
  - Validation: `v3_gp1_voyage002_gate.json` overall=true；场景变量哈希入 gate JSON

### M-B codegen 链 + G-P2（AC-602）

- [x] 4. P3 codegen 链 + ctypes 推进器
  - ID: T-604
  - Requirement(s): FR-602, NFR-601, AC-602
  - Files/areas: `model/.../scripts/23_codegen_plantstep.m`（按场景烘焙 VOYAGE_*/SCN → ERT 生成）；`Host/tools/build_plantstep.sh`；`Host/plantstep_advancer.py`（ctypes：init/step/state/fuel/voyage_stop）；产物 `Host/plantstep/S20/` + `Host/plantstep/voyage002/`（含 `build_manifest.json` 指纹：模型/参数哈希 + MATLAB 版本 + 产物 sha256）
  - Validation: 两场景生成+编译成功；wrapper API 冒烟（IC 复位 + 单拍推进数值合理性检查）；指纹机器可读
- [x] 5. G-P2 门禁（codegen C vs 解释执行）
  - ID: T-605
  - Requirement(s): FR-605, AC-602
  - Files/areas: `Host/tools/run_gp2_gate.py`（同回放输入下 wrapper vs `sim(HybridShip_ModelD_PlantStep)` 对照，阈值同 §5.2）
  - Validation: `v3_gp2_{S20,voyage002}_gate.json` overall=true
- [x] 6. P3/P1 走道决策评审（条件门）
  - ID: T-606
  - Requirement(s): FR-603, AC-602
  - Files/areas: 决策记录（进展日志 §18 条目；如回退则 design.md §4.3 激活记录）
  - Validation: T-604/605 实际结果评审后明确记录走道（P3 确认 or §4.3 回退触发证据链）；禁止静默换道

### M-C 闭环 harness + selftest（AC-603）

- [x] 7. 闭环 harness 主循环
  - ID: T-607
  - Requirement(s): FR-606, FR-607, NFR-602, AC-603
  - Files/areas: `Host/ems_closedloop.py`（复用 `ems_inject.py` 协议栈 import，协议帧零改动）：lock-step 循环 §6.2、逐拍 CSV + meta JSON 记录 §6.3、事件环扩展（VOYAGE_STOP/PLANT_STEP_ABNORMAL）、固定拍数终止 + voyage_stop 兜底断言、整跑重做语义（mid-run 失败轨迹作废不采信）
  - Validation: 回放模式（P_gen 来自 ref_G2 离线驱动推进器）无板冒烟通过；记录列完整性检查
- [x] 8. harness selftest 套件
  - ID: T-608
  - Requirement(s): FR-606, FR-607, AC-603
  - Files/areas: `Host/tools/closedloop_selftest.py`（pty mock 板 + 推进器回放）：帧配对/超时语义/事件环/记录列完整性/终止条件全覆盖
  - Validation: selftest 机器可读结果全绿（AC-603）；板会话前置条件
- [x] 9. FR-508 闭环故障注入
  - ID: T-609
  - Requirement(s): FR-608, AC-604
  - Files/areas: `Host/ems_fault_inject.py` 扩展闭环模式（F1 NaN/F2 STALE/F3 垃圾帧/F4 RESET 在运行闭环上注入；回退段事件界定 FALLBACK 进入/退出 + 段内时长/Δfuel/ΔSOC 统计）
  - Validation: pty mock 下四场景注入 selftest PASS（状态机行为 + 回退段界定 + 被控对象轨迹连续合理）

### M-D 原生臂 (b′)(c)（AC-604 数据）

- [x] 10. 原生臂运行与导出脚本
  - ID: T-610
  - Requirement(s): FR-609, AC-604
  - Files/areas: `model/.../scripts/21_run_v3_native_arms.m`（复用 64 运行/缓存/REPRO 断言机制与 66 导出格式，扩展逐拍 fuel_total 列；law=7 可复用 G2 缓存、law=5 新增运行）
  - Files/areas: 产物 `model/.../results/ecms_phase25/closedloop/`（两场景 × 两 law 轨迹 CSV + 指标 JSON；不回写 v1.1 冻结文件）
  - Validation: REPRO 断言通过（d_fuel/d_SOC 与缓存一致）；逐拍列完整性检查
- [x] 11. 原生臂执行（两场景 × law=7/5）
  - ID: T-611
  - Requirement(s): FR-609, AC-604
  - Files/areas: 执行 `21_run_v3_native_arms.m` 全矩阵，收集轨迹与指标
  - Validation: 4 组运行全部产出且指标 JSON 完整；(b′) 与 G2 缓存一致性记录

### M-E 板会话（AC-604/607）

- [x] 12. 双镜像构建与授权准备
  - ID: T-612
  - Requirement(s): FR-614, NFR-601, NFR-603, AC-607
  - Files/areas: NPU/REF 双镜像 fw_gate（`fw_gate.py` exit 0 ×2）+ 产物指纹新鲜度核对（M2 §11.5 缓存陷阱教训）；板会话时间预算声明 + 令牌预申请清单（沿袭 v1 SRAM 令牌机制）
  - Validation: 两镜像 gate PASS + 指纹核对记录 + 授权材料就绪（无板操作本任务内执行）
- [x] 13. 闭环 (b) REF 板会话
  - ID: T-613
  - Requirement(s): FR-606, FR-609, FR-610, FR-614, AC-604, AC-607
  - Files/areas: SRAM 加载 REF（令牌）→ 闭环 S20 → voyage_002（仪器校准证据优先）；每次运行 meta 含镜像哈希 + plantstep 指纹 + 拍耗时分布（NFR-602）
  - Validation: 两次运行轨迹 CSV + meta 完整；仪器级① gate JSON PASS（§5 阈值）；用户在场/远程授权 + 令牌记录
- [x] 14. 闭环 (a) NPU 板会话 + FR-508 闭环注入
  - ID: T-614
  - Requirement(s): FR-608, FR-609, FR-610, FR-614, AC-604, AC-607
  - Files/areas: SRAM 加载 NPU（令牌）→ 闭环 S20 → voyage_002；同场穿插四场景闭环故障注入（T-609 代码实战）
  - Validation: (a) 轨迹 + 仪器级 gate JSON PASS；回退段事件界定与代价统计完整；令牌记录

### M-F 指标 / 报告 / 文档（AC-605/606）

- [x] 15. 指标汇总与公平性判定
  - ID: T-615
  - Requirement(s): FR-610, FR-611, AC-605
  - Files/areas: 四臂两场景统一指标 JSON（fuel_raw/fuel_corr/SOC_end/v_track_rmse/临界工况/ê 生效占比，公式 §8.1）；SOC 公平性判定（±2% 带，超带标 SOC-UNFAIR 转 fuel_corr 次级结论）；层③ 报告性输出（(a) vs ref 同拍对照）
  - Validation: 指标 JSON 机器可读；公平性判定记录；fuel_corr 使用 64/66 同式（禁新造公式）
- [x] 16. 闭环报告 + 基线说明 + 文档登记
  - ID: T-616
  - Requirement(s): FR-612, FR-613, NFR-605, NFR-606, AC-605, AC-606
  - Files/areas: `model/.../results/ecms_phase25/closedloop/v3_closedloop_report.md`（结构 §8.4 七节）；v3 基线说明登记层② (a) 裁决；两级 README 同步（Model 目录 `README.md`/`README_Model.md` 增补 plantstep 变体条目；`Host/` README 增补闭环工具条目）；产物入库核对（codegen 产物本体不入库、manifest/哈希入库）；§20.2 绝对路径扫描
  - Validation: 报告七节齐全 + 图表；README 登记完整；路径扫描干净；`git status` 核对（AGENTS.md §14）

## Verification Checklist

- [x] Requirements IDs are referenced by tasks.（FR-601~614 / NFR-601~606 / AC-601~607 全覆盖）
- [x] Design testing strategy is represented by tasks.（T1=T-602/603、T2=T-605、T3=T-607/608、T4=T-613/614、T5=T-610/611）
- [x] Each task has a clear validation command or manual check.
- [x] Rollback/migration tasks are included when applicable.（T-606 P3/P1 走道决策；FR-603 回退路径）

## Execution Notes

- Keep `Status: tasks-draft` until the user explicitly approves this task plan.
- After approval, change status to `tasks-approved` and run implementation-readiness validation before coding.
- When coding starts, change status to `implementation-in-progress`.
- When all tasks are checked and validated, change status to `implementation-complete`.
- Complete tasks in order unless the user approves reordering.（M-A→M-B→M-C 严格序；M-D 可与 M-C 并行无文件冲突）
- Mark tasks complete only after validation passes.
- Record deviations from design in design.md before implementing them.
- 板会话任务（T-612 准备除外）需用户在场或远程授权 + 令牌；G-P1/G-P2/selftest/原生臂均无板可执行。
- 编译期换型不在本清单（design §11：以 v1 spec FR-511 验收条目为载体，v3 完成后立即启动）。