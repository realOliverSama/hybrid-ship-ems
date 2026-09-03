# EMS01 CHIL — TCN-f64 编译期换型 Requirements

Spec: `ems01-chil-tcn64-swap`
Status: requirements-approved（2026-09-02 用户批准；批准附言：测试口径与 MLP-Q 先例逐项对齐，见 Fixed Decisions 10）
Created: 2026-09-02
Brainstorm: `./brainstorm.md`
输入: v1 spec FR-511 验收条目（`../ems01-chil-ecms-v1/`）+ v3 收官状态（`../ems01-chil-v3-closedloop/`）
      + `docs/agent/AI_MODELING.md` §22.3（三口径契约）+ `docs/research_roadmap.md` §8.3
Branch: `models-research`（换型实施分支另建 `feat/*`）
工程目录: `firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS/`
模型目录: `model/simulink_modelD_(EMS_NN_AECMS)/`

## Overview

v3 soft-HIL 真闭环收官后，按 v1 spec FR-511 流程执行**编译期换型（档 1，非运行期热切换）**：
将 phase25 同签名候选预测模型 **TCN-f64**（165,851 参数，int8 tflite 219.7KB，float val
RMSE 9.8433 kWh 全场最优）换入 EMS01_CHIL_ECMS 固件的预测器槽位，走完整链路
（stedgeai 重生成 → 重构建 → fw_gate → SRAM 加载 → 一致性复测 → 闭环复测）。

换型价值定位（brainstorm 裁决）：**FR-511 换模型流程的首次真实兑现 + 更大模型（12M MACs
vs MLP-Q 21.5K）入闭环的平台能力验证**（roadmap §8.3/§8.4.7 议程），**不是**"预期更省油"
——int8 口径下现行 MLP-Q（val RMSE 9.9659）仍优于 TCN-f64 int8（10.0353），闭环结果无论
优劣均如实记录（研究诚信，roadmap §9）。

## Fixed Decisions（brainstorm 用户裁决，实施不得偏离）

1. **候选 = TCN-f64**（`models/tcn_f64/tcn_f64_int8.tflite`，QDQ int8，双输入
   [1,300,1]+[1,2]，输出 [1,3] q10/q50/q90）；TCN-f16 / h128-MLP / mse 变体不采用
   （mse 变体签名不符直接排除）。
2. **REF 后端 = 方案 R1**：开发 TCN C 前向（`ems_infer_tcn.c`），保留 (b) 臂与四臂
   矩阵完整性；层① 改两段式口径：① host gcc TCN-C vs keras float32 参考（数值容差门，
   LSB 形式）；② 板上 REF vs host gcc（同源代码，预期精确一致）。
3. **层① 容差门 = LSB 形式**：1 LSB = TCN q50 输出张量 scale × y_std（kWh），阈值在
   design 论证设定，不得为 PASS 事后放宽。
4. **换型只换预测模型**：workclf v1.1（归一化口径）零改动；EMS_GATE_MODE=2、
   GATE S1/S2 门控常数、FR-508 状态机语义全部不动。
5. **阈值不放宽；冻结资产零改动**（baseline_manifest / ref_G2 / fw_reference /
   v1.1 冻结件）；层② S20 98.877% 边际 FAIL 处置沿袭 (a) 接受登记。
6. **主模型 `HybridShip_ModelD_DCBus.slx` 与 `PlantStep.slx` 零改动**——被控对象与
   换型无关，G-P1/G-P2/PlantStep 仪器链免重跑；G-P 仪器门禁不重开。
7. **换型 = NOR mlpq 槽位（0x70200000）换入 TCN 权重 hex**；mpool 布局（双 1MB 池）
   零改动（analyze 预检：TCN 权重 161.3KB = 池 15.75%）；MLP-Q 全部资产保留，
   可按 FR-511 逆向换回。
8. **三口径契约（AI_MODELING §22.3）四条硬规则全部适用**：口径声明随交付、
   三口径一致检查、int8 标定集取部署口径样本、部署后层③式对拍。
9. **板操作纪律**：SRAM 加载走 embedded-debugger MCP + 单次令牌；NOR 权重烧录为
   Level C 单独阶段（专用外部加载器，仅 0x70200000 区域，禁 Mass Erase，禁通用
   MCP 擦写，用户在场授权）；Build/Gate/Debug/Flash 阶段隔离。
10. **测试口径与 MLP-Q 先例逐项对齐**（批准附言，2026-09-02）：层② ×200 向量（val
    固定 seed=20260831 抽样、原始物理量输入、板载 vs Python int8 reference）、
    INJECT 注入轨迹（S20 + voyage002）、闭环 harness（ems_closedloop.py lock-step、
    同 plantstep 二进制、同场景）、指标定义（fuel_raw/fuel_corr、SOC ±0.02 公平带、
    网格一致率、端点偏差）与 gate/metrics JSON schema 全部沿 v1/v3 既有测试形态，
    **不新造测试口径**——保证 TCN 结果与 MLP-Q/v3 收官数据同表可比，可直接并入
    导师报告 `docs/reports/EMS01_研究总结报告_soft-HIL真闭环_20260902.docx` 的
    既有图表体系（8 图 12 表）。

## Scope

### In Scope

- TCN-f64 部署链全部产物：源模型只读副本、口径声明表、stedgeai analyze/generate、
  `generate-n6-model.sh`、`network-data-tcnq.hex`、deploy JSON（TCN 权重单一来源）、
  `ems_params` 重生成（含 EMS_TCNQ_* 表）、`ems_infer_tcn.c` 新开发与 L0 对拍。
- NPU 后端双输入适配（`ems_infer_npu.c`，workclf 双输入先例模式）。
- 双镜像重构建（NPU 默认树 + build-ref2 REF 树）+ fw_gate ×2 + 指纹新鲜度核对。
- 一致性复测三件套：层①（两段式新口径）、层②（FR-504 ×200 LSB 新门禁 + INJECT
  网格一致率）、层③式（闭环 (a)vs(b)）。
- 闭环复测：(a) TCN-NPU + (b) TCN-REF × 2 场景（S20 / voyage002）= 4 组，用 v3
  harness；(b′)/(c) 引用 v3 收官权威数据；FR-508 抽测。
- 机器可读产物（gate JSON / metrics JSON）、报告与 roadmap 登记、§18 进展日志。

### Out of Scope

- 运行期模型热切换（v2 另立项）；NOR 双池布局变更；TCN 之外的候选换型。
- workclf / 门控 / FR-508 状态机的任何语义修改；任何模型重训或量化重标定
  （T-307 既有 int8 tflite 原样使用）。
- 主模型 / PlantStep / G-P1/G-P2 仪器链重跑；v1.1 冻结件与 baseline_manifest 重建。
- 层② S20 98.877% 收敛研究（另立项，roadmap §8.3）。
- Release NOR 全镜像编程（release-stm32n647-appli）——本次仅 SRAM Debug + 权重区
  烧录。

## User Stories

- 作为研究者，我要把 TCN-f64 按可复现脚本链换入固件，得到"更大模型在真闭环中的
  表现"数据点，以支撑论文中"更大预测模型的兑现空间"论述与后续模型路线决策。
- 作为工程验证者，我要 FR-511 换模型流程在双镜像 + 三层一致性 + 闭环复测上完整走通，
  形成"换型即换 hex"的工程惯例与机器可读验收证据。

## Functional Requirements

### A. 部署链（脚本驱动，NFR-303 沿袭）

- **FR-701**: THE 工程 SHALL 新增 `Model/tcn_q/` 目录承载 TCN-f64 部署链：源 tflite
  只读副本（sha256 溯源至 `models/tcn_f64/`）、`user_neuralart.json`（同 mlpq 配置）、
  `my_mpools/stm32n6.mpool`（同 mlpq）、`generate-n6-model.sh`（产物
  `network-data-tcnq.hex` @0x70200000），并归档 analyze/generate 报告。
- **FR-702**: THE 工程 SHALL 提供 TCN 权重部署 JSON 生成脚本（keras →
  `selected_model_weights_tcnq.json`，含口径声明字段），并扩展 `gen_fw_params.py`
  以其单一来源生成 `EMS_TCNQ_*` 双精度权重表 + 归一化常数（与 fleet_norm_v2.json
  逐位断言一致），沿 v1 R8 后端只做张量前向的职责边界。
- **FR-703**: THE 固件 SHALL 新增 `ems_infer_tcn.c`（TCN C 前向，double，输入
  xnorm[0:300] 窗 + xnorm[300:302] aux，输出 q3_norm[3]），复用 workclf Conv1D C
  模式扩展 dilation/残差/LastTimeStep；求和顺序显式声明并写入口径说明
  （TCN 无 EML 位级锚点，REF 求和顺序即层① host 参考的位级基准）。
- **FR-704**: THE NPU 后端 SHALL 适配 TCN 双输入（窗口 + aux 分缓冲填充），沿
  workclf `Set_User_Input_Buffer_x(0/1)` 先例；I/O 缓冲对齐与 cache 维护沿既有模式。
- **FR-705**: THE `ems_ctrl.c` SHALL 保持归一化/滑窗/反归一化单份实现（R8），预测器
  前向调用按编译期选择（mlpq | tcnq）切换；非 TCN 路径（workclf、门控、ECMS）零改动。
- **FR-706**: THE 换型 SHALL 通过 `stedgeai generate`（`--no-inputs-allocation
  --no-outputs-allocation`，同 mlpq 参数）产出 N6 生成代码与 NOR 权重 hex，objcopy
  基址 0x70200000；analyze 报告（HW coverage/RAM/fallback 清单）归档并核对预算。

### B. 构建与门禁

- **FR-707**: THE 双镜像 SHALL 重构建（NPU 默认树 Appli/build/Debug + REF 树
  build-ref2 `-DEMS_FW_MODE=REF`），fw_gate 7/7 ×2 exit 0；产物指纹新鲜度核对
  （ELF mtime ≥ 源 mtime，M2 §11.5 惯例）。
- **FR-708**: THE int8 标定与三口径一致检查 SHALL 形成机器可读报告：训练口径 ==
  部署喂入口径 == int8 标定集口径三方逐项核对（TCN：标定集 = T-307 val 前 1000
  归一化样本 == 固件 xnorm 喂入口径），随交付归档；不一致禁止部署。

### C. 一致性复测（L0 → 板上）

- **FR-708b (L0)**: THE TCN C 前向 SHALL 通过 host 差分对拍：gcc 编译 REF 链 vs
  Python 参考（keras float32 前向），测试集 = val 抽样窗口 + 随机窗口（固定 seed），
  机器可读报告归档；FAIL 阻断上板。
- **FR-709 (层①)**: THE 板上 REF 镜像 SHALL 复测两段：① host gcc TCN-C vs keras
  float32 参考，门禁 |Δq50| ≤ 1 LSB_TCN（1 LSB = TCN q50 输出 scale × y_std）；
  ② 板上 REF vs host gcc（同源双编译），P_gen 决策链网格一致率 100%（同 v1 L0
  精确语义）；阈值不得为 PASS 调整。
- **FR-710 (层②)**: THE 板上 NPU 镜像 SHALL 复测 FR-504 ×200（val 固定 seed=20260831
  向量，原始物理量输入，板载 vs Python int8 reference，门禁按 TCN 输出 LSB 重推导：
  mean ≤1.5 LSB 且 p99 ≤6 LSB 且 max ≤8 LSB 且无符号偏置且无发散）+ INJECT 注入
  轨迹 NPU vs host-int8 网格一致率 ≥99%（S20 边际 FAIL 时沿袭 (a) 接受登记，
  阈值不放宽）。
- **FR-711 (层③式)**: THE 部署后闭环验证 SHALL 含 (a) vs (b) 对拍（v3 harness，
  同被控对象同协议）——零混杂移植代价口径延续 v3（SOC 终点差 / fuel 相对偏差 /
  网格一致率，阈值沿 v3 design §5 仪器级①②同族形式，design 固定）。

### D. 闭环复测与登记

- **FR-712**: THE 闭环复测 SHALL 以 v3 harness 执行 (a) TCN-NPU 与 (b) TCN-REF ×
  S20/voyage002 共 4 组（lock-step，REPRO 断言）；(b′)/(c) 引用
  `Host/results/closedloop/closedloop_metrics.json` v3 收官权威数据；公平性按
  SOC ±0.02 带判定（同 v3 §8.2）；TCN 闭环结果以增量条目并入该 metrics JSON
  （同 schema，Fixed Decisions 10）。
- **FR-713**: THE 时延 SHALL 按 FR-505 修订版预算实测：TCN 推理时延 + workclf
  合计 ≤60 ms（1 Hz 控制拍）；超限不放宽 → Escalate（用户裁决：候选降级 TCN-f16
  或接受登记或终止换型）。
- **FR-714**: THE FR-508 回退状态机 SHALL 抽测（至少 NaN 输入、停发超时、CMD_RESET
  恢复三场景，mock 或板上口径之一，语义与 v3 收官记录同构）。
- **FR-715**: THE 验收产物 SHALL 登记：换型报告（层①②③ + 闭环 + 资源/时延表，
  表格结构沿导师报告既有行列口径，便于同表并入）、closedloop metrics 增量条目、
  roadmap §8 换型小节、§18 进展日志；报告含"口径声明随交付"节（§22.3 规则 1）。

## Non-Functional Requirements

- **NFR-701 实时性**: TCN-f64 NPU 推理与 REF C 前向实测时延记录在案；与 workclf
  合计满足 FR-505 预算（≤60 ms）。
- **NFR-702 资源**: NPU 权重落 octoFlash 0x70200000 1MB 池（analyze 预检 15.75%），
  activations 落 npuRAM3（预检 131.3KB = 29.31%）；REF 镜像 ems_params 增量
  （~166K double 参数）不得使 SRAM 布局超限（构建 + gate + 加载实证）。
- **NFR-703 可复现**: 全链脚本化（相对路径，§20.2），命令行可重跑；部署 JSON /
  ems_params / hex 均带 sha256 溯源；新文件登记所在目录 README（两级文档分工）。
- **NFR-704 阶段隔离**: 硬件操作逐级授权（SRAM 加载令牌、NOR 权重烧录 Level C
  令牌，用户在场）；板会话前所有离线验证（L0/构建/gate）完成。

## Acceptance Criteria

- [ ] AC-1（FR-701/702/706/708）：`Model/tcn_q/` 部署链产物齐备且 sha256 溯源完整；
      analyze 报告显示 CONV/FC 全 NPU（SW 仅数据搬运）、RAM/权重在预算内；三口径
      一致检查机器可读报告 PASS。
- [ ] AC-2（FR-703/704/705/708b）：`ems_infer_tcn.c` + 双输入 NPU 适配 + ctrl 编译期
      选择实现完成；L0 host 差分对拍机器可读 PASS（|Δq50| ≤ 1 LSB_TCN）。
- [ ] AC-3（FR-707）：双镜像 fw_gate 7/7 ×2 exit 0，指纹新鲜度记录在案。
- [ ] AC-4（FR-709/710）：层① 两段式复测 PASS（板上 REF vs host gcc 网格一致 100%）；
      层② FR-504 ×200 新 LSB 门禁 PASS + INJECT 网格一致率报告（边际 FAIL 沿袭
      (a) 接受登记）。
- [ ] AC-5（FR-711/712）：闭环 4 组（(a)(b) × 2 场景）gate JSON 机器可读；(a)vs(b)
      层③式对拍阈值 PASS；(b′)/(c) 引用 v3 权威数据完成对比表（TCN vs MLP-Q vs
      law=5，公平性带判定）。
- [ ] AC-6（FR-713/714）：时延实测入预算（或按 Escalation 用户裁决留痕）；FR-508
      抽测记录语义符合。
- [ ] AC-7（FR-715）：换型报告 + roadmap/日志/README 登记完成；主 Agent §14 验收
      （git diff 范围核对：仅预测器槽位相关变更）。

## Edge Cases

- **TCN 推理输出非有限**：沿 ems_infer 返回值契约（非 0 → FR-508 FALLBACK ê=0），
  REF/NPU 同语义。
- **双输入缓冲装配错误**（窗口/aux 错位）：L0 对拍 + 层② ×200 覆盖（输入装配错误
  会产生大偏差，被门禁捕获）。
- **NOR 权重区写入失败/校验不符**：烧录后 CRC32 核对（沿 FR-514 记录惯例），失败
  停止不加载。
- **层② S20 边际 FAIL 复现**：沿 (a) 接受登记，机器可读记录，不放宽。
- **TCN 闭环结果劣于 MLP-Q**：如实记录（研究诚信），换型结论为"平台能力验证"；
  后续模型选型由用户另行裁决（本 spec 不做 Winner 更换决策）。

## Dependencies and Constraints

- 输入资产（只读）：`models/tcn_f64/tcn_f64_{keras,int8.tflite}`（sha256 溯源）、
  `datasets/fleet_norm_v2.json`、v1 deploy 链先例（`Model/mlp_q/`）、v3 harness 与
  收官 metrics、T-307 int8 报告。
- 工具链：ST Edge AI Core v2.2.0（锁定）、arm-none-eabi objcopy、`prepare-stm32n647-project`
  构建、fw_gate、embedded-debugger MCP（SRAM）、STM32CubeProgrammer + ATK 外部加载器
  （NOR 权重区）。
- 硬件前置：SRAM 加载与 NOR 权重烧录需用户在场（板会话规划在 tasks 阶段明确）。
- 顺序依赖：L0 差分 PASS → 双镜像构建+gate → 板会话（SRAM 加载 → 层①② → NOR
  烧录 → 闭环）→ 报告登记。

## Open Questions

- [ ] 板会话排程与授权令牌申请时机（tasks 阶段定，用户确认）。
- [ ] （无其他——候选、REF 方案、层①口径、闭环矩阵、预算处置均已裁决）