# EMS01 CHIL — TCN-f64 编译期换型 Tasks

Spec: `ems01-chil-tcn64-swap`
Status: tasks-approved（2026-09-02 用户批准）
Created: 2026-09-02
Brainstorm: `./brainstorm.md`
Requirements: `./requirements.md`（requirements-approved 2026-09-02）
Design: `./design.md`（design-approved 2026-09-02）

## Implementation Plan

任务按依赖序排列：M-A 离线部署链 → M-B REF C 前向 + L0 → M-C NPU 适配 → M-D 双镜像
构建门禁 → M-E 离线复演 → M-F 板会话（阶段隔离）→ M-G 收尾登记。阈值与门禁数值已在
design §Testing 固定（先验设定，FAIL 不放宽）；板会话任务（T-808~811）须用户在场、
逐阶段令牌授权，前一阶段完成不自动授权后一阶段。

执行分工：M-A~M-E 为离线执行性工作（默认 ollama-cloud delegate 承担，主 Agent 负责
方案监督与逐任务验收）；M-F 板会话全程用户在场；M-F/M-G 验收 = 主 Agent §14 实测核对。

> **2026-09-02 晚授权变更**：用户下班前授权后续任务自主执行（含板会话无人在场：
> SRAM 加载令牌由主 Agent 生成+审计留痕，NOR 权重烧录 Level C 授权在案）；
> Mass Erase / Option Bytes / Boot 配置 / FSBL 区仍绝对禁止；停止条件见任务书与
> spec requirements；全部板操作机器可读留证。
>
> **2026-09-02 晚工程隔离规则（用户指令）**：凡为适配当前预测模型而需修改
> 工程源代码的后续工作（如流式 TCN 前向等设计修订项），必须在 `EMS_Projects/`
> 下复制新工程实施，不得改动现有已验证工程；本 spec 已完成的换型工作为
> 编译期槽位切换（`-DEMS_PREDICTOR=mlpq` + mlpq hex 可逆，红线文件零改动，
> v3 行为已回归验证），提交时以 feat 分支隔离。

### M-A 离线部署链（AC-1）

- [x] 1. `Model/tcn_q/` 部署链目录
  - ID: T-801
  - Requirement(s): FR-701, FR-706, NFR-703, AC-1
  - Files/areas: `FW/Model/tcn_q/`（新增：`tcn_q.tflite` 只读副本 + sha256 记录、
    `user_neuralart.json`（同 mlpq 配置拷贝）、`my_mpools/stm32n6.mpool`（同 mlpq）、
    `generate-n6-model.sh`（沿 mlpq 脚本逐行同构，`WEIGHTS_BASE=0x70200000`，
    产物 `network-data-tcnq.hex`）；`FW/Model/README.md` 登记
  - Notes: 源 tflite 只读副本 = `models/tcn_f64/tcn_f64_int8.tflite`，sha256 溯源
    写入目录 NOTE；**不得复制 prepare 脚本或 BSP**（vendor 子树 AGENTS.md）
  - Validation: `./generate-n6-model.sh` 可重复执行成功；`analyze_report.txt` +
    `generate.log` 归档；analyze 表核对 = design §Summary（CONV/FC 全 EC、
    activations @npuRAM3、权重 @0x70200000、mpool 零改动）
- [x] 2. TCN 部署 JSON 导出 + 参考链脚本
  - ID: T-802
  - Requirement(s): FR-702, FR-708, AC-1
  - Files/areas: `WS/python/export_tcn_deploy.py`（新增：keras →
    `models/selected_model_weights_tcnq.json`，schema 见 design §Data Model 1，
    含 caliber 口径声明块）；`WS/python/export_tcnq_reference.py`（keras float32
    逐 tick q3 参考链，`.venv_tf` 环境）
  - Notes: 加载须带 `architectures.LastTimeStep` custom object；导出后机械验证 =
    参数量断言 165,851 + 3 随机窗 spot-check（keras vs 导出权重重放 ≤1e-6 kWh）
  - Validation: spot-check 机器可读 PASS；JSON 含 sha256_tflite 与 git_commit_sha
- [x] 3. gen_fw_params 扩展 + ems_params 重生成 + 三口径报告
  - ID: T-803
  - Requirement(s): FR-702, FR-708, AC-1
  - Files/areas: `FW/Host/tools/gen_fw_params.py`（扩展：双 JSON 源，norm/gate 与
    fleet_norm_v2.json 逐位断言，追加 `EMS_TCNQ_*` 表至 `Appli/App/ems_params.c`，
    `ems_params.h` 增补声明）；产物 `deploy/m3_tcn_swap_calib_report.json`
    （三口径一致检查，结构沿 m2 家族）
  - Notes: 三口径核对项（§22.3）：① 训练口径（fleet_norm_v2 + 302 特征 + 升序窗）
    == ② 部署喂入（ems_ctrl xnorm 拆分）== ③ int8 标定（T-307 val 前 1000 归一化
    样本）；结论不一致 = 禁止部署（阻断）
  - Validation: `ems_params.c` 重生成后 sha256 溯源头更新；norm 断言通过；
    calib_report 三方逐项 PASS

### M-B REF C 前向 + L0（AC-2）

- [x] 4. `ems_infer_tcn.c` 开发
  - ID: T-804
  - Requirement(s): FR-703, NFR-702, AC-2
  - Files/areas: `FW/Appli/App/ems_infer_tcn.c`（新增）+ `ems_infer.h` 头注释升格
    （槽位契约语义，调用点零改动）
  - Notes: 结构/求和顺序/返回值契约 = design §Architecture 2（dilation 因果卷积、
    acc 从偏置起 kk 升序→通道 c 升序、double、非有限返回 1）；内部缓冲 static
    （不占任务栈）；`ems_ctrl.c`/`ems_app.c`/`ems_proto.*` **零改动**（Do Not
    Change，diff 审查项）
  - Validation: 代码审查 = 布局与 `EMS_TCNQ_*` 表一一对应；无 HAL 依赖、平台无关
- [x] 5. L0 host 差分（T2 层）
  - ID: T-805
  - Requirement(s): FR-708b, AC-2
  - Files/areas: `FW/tests/test_ctrl_diff.py`（增补 `--predictor tcnq`：gcc 编译
    ems_ctrl.c + ems_infer_tcn.c + ems_params.c；参考 = export_tcnq_reference.py
    输出）；产物 `FW/Host/results/tcn_swap/l0_tcn_diff.json`
  - Notes: 向量 = val 抽样（seed=20260831）+ 32 随机窗（seed 固定）+ S20/voyage002
    全轨迹；门禁 **|Δq50| ≤ 1 LSB_TCN = 0.266214 kWh** 逐 tick + P_gen 网格一致率
    ≥99%；FAIL 阻断上板（不放宽）
  - Validation: l0_tcn_diff.json overall=true；FAIL 时按机制排查（装配/求和顺序/
    权重布局）后重跑
- [x] 6. mlpq 模式回归保护
  - ID: T-806
  - Requirement(s): FR-705, AC-2
  - Files/areas: `test_ctrl_diff.py --predictor mlpq`（既有链原样跑通）
  - Notes: 证明 CMake/ems_params 扩展未破坏 v1 路径（可逆性证据）
  - Validation: 既有 L0 基线结果不变（对比 v1 验收记录）

### M-C NPU 侧适配 + CMake（AC-2）

- [x] 7. NPU 双输入适配 + EMS_PREDICTOR
  - ID: T-807
  - Requirement(s): FR-704, FR-705, AC-2
  - Files/areas: `FW/Appli/App/ems_npu_tcnq.c`（新：符号改名 `*_Tcnq` + static_assert
    IN_1=1200B/IN_2=8B/OUT_1=12B）；`FW/Appli/App/ems_infer_npu.c`（预测器槽位实现
    改 TCN 双输入填充：Buffer_x(0=窗 300×4, 1=aux 2×4)，对齐与 cache 沿既有模式）；
    `FW/Appli/CMakeLists.txt`（`EMS_PREDICTOR` cache 变量：NPU 分支选编
    ems_npu_mlpq.c|ems_npu_tcnq.c，REF 分支选编 ems_infer_ref.c|ems_infer_tcn.c；
    mlpq 分支保持现状逐字节）
  - Notes: workclf 路径（ems_npu_workclf.c、WC_TEST）零改动；`ems_npu_tcnq.c`
    不复制生成物（`#include Model/tcn_q/network.c` 原路径）
  - Validation: 离线构建通过（M-D 联合验证）；diff 范围核对（仅预测器槽位相关）

### M-D 双镜像构建 + 门禁（AC-3）

- [x] 8. 双镜像重构建 + fw_gate ×2 + 指纹 + 占用实测
  - ID: T-808
  - Requirement(s): FR-707, NFR-701, NFR-702, AC-3
  - Files/areas: NPU 默认树 `Appli/build/Debug`（EMS_PREDICTOR=tcnq）+ REF 树
    `build-ref2`（-DEMS_FW_MODE=REF -DEMS_PREDICTOR=tcnq）；`prepare-stm32n647-project`
    惯例构建；`fw_gate.py` ×2；指纹记录 `FW/Host/results/tcn_swap/image_fingerprints.json`
    （沿 t612 惯例：ELF mtime ≥ 源 mtime + sha256）
  - Notes: **REF 总占用实测**（objdump 段累加 ≤2047K−64KB）；NPU 镜像核对
    `EMS_TCNQ_*` 已被 gc-sections 裁除（nm 检查）；超限 → design §Error Handling
    预案（float32 表化 → 重生成重构建，口径注明）或 Escalate
  - Validation: fw_gate exit 0 ×2；占用记录机器可读；指纹新鲜度 PASS

### M-E Host 工具 + 离线复演（AC-2/AC-4 预备）

- [x] 9. Host 工具 tcnq 变体 + 离线全量复演
  - ID: T-809
  - Requirement(s): FR-710, AC-4
  - Files/areas: `FW/Host/tools/gen_reference_int8.py` / `gen_host_int8_pgen.py`
    （`--model tcnq`：TCN 双输入拆分喂 tflite 解释器）；`FW/Host/ems_inject.py`
    若需模型参数化则最小增补；离线复演 = S20 + voyage002 INJECT 轨迹的 host 侧
    参考数据（int8 + REF C）预生成
  - Notes: 向量/轨迹/输出格式与 MLP-Q 电池完全一致（Fixed Decisions 10）；host
    int8 参考数值与 T-307 int8_val 指标抽样交叉核对
  - Validation: 复演产物机器可读；与 MLP-Q 同名字段 schema 一致

### M-H 流式增量前向 + NPU 频率（2026-09-02 深夜用户裁决 C 方案，P3 新工程）

> 背景与实测依据见 design 实施期澄清③（REF TCN 8.8s/拍，超 INJECT 2s STALE 契约
> 4.4×，层① 全轨迹与闭环 (b) 臂协议不可行）。承载工程 = `EMS01_CHIL_ECMS_TCN64`
> （P3，工程隔离规则：模型适配性修改在新工程实施，已注册 EMS_Projects/README）。

- [x] 16. 流式增量 TCN REF 前向（P3）
  - ID: T-816
  - Requirement(s): FR-703 修订延申, FR-709 重验, AC-4
  - Files/areas: P3 `Appli/App/ems_infer_tcn.c`（环状缓冲增量前向 + 窗口连续性自检 fast/cold 双路径）+ `Host/tests/test_ctrl_diff.py`（streaming 用例）+ `Host/results/tcn_swap/l0_tcn_stream_diff.json`
  - Notes: 槽位契约不变；每拍仅算新时间步（MAC ÷300 → ms 级）；streaming == batch 逐位一致（S20/voyage002 全轨迹 + val200 + random32，max|Δ|=0）+ vs keras LSB 门；ems_ctrl/ems_app/ems_proto 红线零改动；cold path（INFER_TEST 向量）= batch 等价总 MAC
  - Validation: l0_tcn_stream_diff.json（bit-exact 断言 + LSB 分布）+ mlpq 回归不变 + 编译零警告 + REF 占用 ≤2047K−64KB
- [ ] 17. NPU 域频率 400→600MHz（P3）
  - ID: T-817
  - Requirement(s): NFR-701 延伸, FR-505 预算余量优化
  - Files/areas: P3 `Appli/Core/Src/main.c` `npu_config()`（运行时 LL_RCC_IC2_SetDivider(2)）+ `EMS01_CHIL_ECMS.ioc`（IC2Div 2/600MHz，声明对齐）
  - Notes: 用户授权（2026-09-02 晚"npu 频率拉到最大"，限定新工程）；PLL1=1.2GHz，RM0486 NPU 上限 1GHz → 最大安全档 Div2=600MHz（991 为其 PLL1=1.6GHz/2=800MHz 不可直接照搬）；workclf 42.5ms 预期 → ~28ms（板上 RSP us_infer 前后实测验证）；fw_gate 需 `FW_GATE_ALLOW_IOC=1`
  - Validation: 板上 workclf us_infer 前后对照（batch 镜像 NPU 臂）+ gate exit 0

### M-F 板会话（用户在场，阶段隔离，AC-4/AC-5/AC-6）

- [x] 10. 板会话①：SRAM 加载 REF 镜像 → 层① 复测（T4 层）
  - ID: T-810
  - Requirement(s): FR-709, AC-4
  - Files/areas: embedded-debugger MCP（connect probe=auto / target=STM32N647 /
    1800kHz / no-reset / no-halt → load_elf_to_sram，令牌核销）；`ems_inject.py`
    注入（同 v1 M2 轨迹）；产物 `layer1_ref_vs_host.json`
  - Notes: REF 前向权重在 ELF 内（ems_params double 表），**不依赖 NOR 预测器
    权重**——置于 NOR 烧录前；门禁 = P_gen 网格一致率 100%（同 v1 层① 精确语义）
    + 段① T2 门禁复核；串口 /dev/cu.usbserial-1130
  - Validation: 网格一致率 100%；FAIL → 机制排查（不放宽），连续失败 Escalate
- [x] 11. 板会话②：NOR 权重烧录（Level C）
  - ID: T-811
  - Requirement(s): FR-707 载体前提, NFR-704, AC-4（预备）
  - Files/areas: STM32CubeProgrammer + 外部加载器 `MX25UM25645G_ATK-CNN647B.stldr`
    （sha256 预检）；仅写 0x70200000 起预测器池区域（TCN `network-data-tcnq.hex`
    161.3KB）；烧录前后 region 读取留证 + CRC32 记录 →
    `FW/Host/results/tcn_swap/nor_evidence_t811/`
  - Notes: **Level C 令牌（PROGRAM_AUTHORIZED_*）单次核销，用户在场**；禁 Mass
    Erase / 通用 MCP 擦写 / Option Bytes / Boot 配置改动；workclf 池 0x70400000
    与 FSBL 区零接触
  - Validation: CRC32 与生成时记录值一致；证据文件归档
- [ ] 12. 板会话③：SRAM 加载 NPU 镜像 → 层② + 层③开环（T5/T6 层）
  - ID: T-812
  - Requirement(s): FR-710, AC-4
  - Files/areas: SRAM 加载 NPU 镜像（令牌）；`CMD_INFER_TEST` ×200（val 固定
    seed=20260831 同向量）+ `ems_inject.py` INJECT 双场景
  - Notes: LSB 门禁按 LSB_TCN=0.266214 kWh（mean ≤1.5 / p99 ≤6 / max ≤8 / 无偏置 /
    无发散 / RMSE 比 ≤1.1）；INJECT 网格一致率 ≥99%，S20 边际 FAIL → (a) 接受
    登记（沿 v1.1 先例，阈值不放宽）；层③开环（板 NPU vs host REF）报告性；若
    NPU init 失败（权重 CRC 不符）→ 先核对烧录证据再排查
  - Validation: FR-504 报告 + `fw_compare_tcn.json` 机器可读；S20 若边际 FAIL 按
    (a) 登记路径留证
- [ ] 13. 板会话④：闭环 4 组 + FR-508 抽测 + 时延（T7/T8/T9/T10 层）
  - ID: T-813
  - Requirement(s): FR-711, FR-712, FR-713, FR-714, AC-5, AC-6
  - Files/areas: `FW/Host/ems_closedloop.py`（(a) --image npu / (b) --image ref ×
    S20/voyage002 = 4 组 lock-step + REPRO 断言）；`ems_fault_inject.py
    --closed-loop`（NaN/停发/CMD_RESET 三场景）；RTT/Infer_us 时延采集；产物
    `FW/Host/results/closedloop/`（增量）+ 逐组 gate JSON
  - Notes: 层③式阈值 = SOC 终点 |Δ| ≤5e-3、fuel_corr 相对 ≤2%、无数值健康异常
    （P_gen 网格一致率报告性）；时延合计 ≤60ms 超限 → Escalate 不放宽；公平性带
    ±0.02 判定；TCN vs MLP-Q vs law=5 对比表数据来自 closedloop_metrics 增量 +
    v3 收官条目
  - Validation: 4 组 gate JSON overall=true；REPRO 断言 PASS；FR-508 三场景语义
    记录与 v3 同构

### M-G 收尾登记（AC-7）

- [ ] 14. metrics 增量 + 换型报告 + roadmap/README/日志登记
  - ID: T-814
  - Requirement(s): FR-715, NFR-703, AC-7
  - Files/areas: `closedloop_metrics.json` 增量条目（同 schema，TCN 数据与 v3 权威
    数据同表）；`FW/Host/results/tcn_swap/tcn_swap_report.md`（层①②③ + 闭环 +
    资源/时延 + 口径声明节，表格结构沿导师报告行列口径）；`docs/research_roadmap.md`
    §8 换型小节；`FW/Model/README.md` + `FW/Model/tcn_q/README.md`（新目录登记）+
    `FW/README_Model.md` 预测模型清单更新；§18 进展日志
  - Notes: metrics dry-run 校验读取方兼容（design R7）；报告明确陈述层① 口径
    差异（design R5，不得暗示与 v1 同锚点）；换型后板上状态（TCN 常驻）与回退
    路径写明
  - **报告/roadmap/docx 三步管线（2026-09-02 晚用户指令，T-814 范围扩充）**：
    ① 先将 `docs/reports/EMS01_研究总结报告_soft-HIL真闭环_20260902.docx`（终稿，
    1308ab76，含 §2.1 Model D 分模块建模扩写等内容）的最新内容同步回
    `docs/research_roadmap.md`；
    ② 再将本次换型新研究内容（TCN-f64 换型闭环结果）写入 roadmap；
    ③ 最后回更 docx：**扩展 `docs/reports/generate_figures.py` 使新测试结果
    （TCN 四臂/两场景）画入图 2/3/5/7 等既有数据图（fuel_bars/soc/pgen/gp1 等从
    closedloop_metrics.json + TCN 轨迹 CSV 重绘，非仅改文字）**，再跑
    `build_docx.py` 重生成报告（图表编号自动化沿用）；三步全部脚本化幂等
  - Validation: 主 Agent §14 验收（git diff 范围核对：仅预测器槽位相关变更；
    冻结资产/主模型/PlantStep/ems_ctrl.c 零改动确认；新文件 README 全登记）
- [ ] 15. 提交与汇报
  - ID: T-815
  - Requirement(s): NFR-703, AC-7
  - Files/areas: `feat/*` 分支逐文件 `git add`（禁 `git add .`）+ commit + push +
    §15 汇报模板
  - Notes: 提交范围 = 换型相关文件 + spec 三件套 + 报告/登记；slprj_v1_stale/
    Host/fault_out/ 保持不入库
  - Validation: Working tree clean（惯例未跟踪项除外）；Gate/汇报齐备

## Escalation Conditions（任一触发 → 停止并返回主 Agent/用户）

- analyze/generate 报告与 design §Summary 预检数字不符（CONV 落 SW、RAM/权重超池）；
- L0 差分 FAIL 排查两轮仍超 LSB_TCN 门（怀疑口径错配 = §22 事故同构 → 必须停）；
- REF 镜像占用 >2047K−64KB 且 float32 表预案仍超限；
- 时延合计 >60ms（FR-713 用户裁决：降级 f16 / 接受登记 / 终止）；
- 层② 非 S20 场景 FAIL 或 S20 大幅劣化（>2% 拍数超差，非边际）；
- 板会话任一阶段令牌外操作需求 / NOR 烧录校验不符；
- 闭环 gate FAIL 且机制排查指向设计层（非换型实现层）。

## Rollout Order and Dependencies

```text
T-801 → T-802 → T-803 → T-804 → T-805（阻断门）→ T-806 → T-807 → T-808 → T-809
→ 【离线全绿闸门】→ T-810（板① REF 层①，SRAM 令牌）→ T-811（板② NOR 烧录，
Level C 令牌）→ T-812（板③ NPU 层②）→ T-813（板④ 闭环/FR-508/时延）
→ T-814（收尾登记）→ T-815（提交汇报）
```

> 板会话内顺序 = design §Rollout 定案（REF 层① 不依赖 NOR 权重 → 先测；NPU 权重
> 烧录后再测 NPU 镜像）；任务 ID 与执行顺序以此处为准，design Rollout 编号仅作
> 阶段分组。

## Do Not Change（全程红线，diff 审查项）

- `HybridShip_ModelD_DCBus.slx` / `PlantStep.slx` / G-P1/G-P2 仪器链 / ref_G2 /
  baseline_manifest / fw_reference / v1.1 冻结件：零改动；
- `Appli/App/ems_ctrl.c`、`ems_proto.*`、workclf 链路（`ems_npu_workclf.c`、
  WC_TEST 路径）：零改动；
- 阈值（FR-504 LSB 形式 / 层② ≥99% / 闭环仪器阈值）：不放宽；
- NOR 布局（双 1MB mpool）、`.ioc`、Option Bytes、Boot/Security：零改动；
- 既有 MLP-Q 资产与 `ems_infer_ref.c`：保留不动（可逆性）。