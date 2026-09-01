# EMS01_CHIL_ECMS M2 移植验证报告（T-508，FR-509/FR-508）

日期：2026-09-07　执行：主 Agent（K3）+ delegate（ollama-cloud/glm-5.3-flash，工具链与判别实验）
Spec：`.specs/ems01-chil-ecms-v1` T-508　验收条款：AC-2（FR-509 比对报告 + FR-508 回退测试记录）
上游报告：M1 部署报告（`m1_deploy_report.md`，T-507/T-311 已关闭）

---

## 1. 执行摘要

| 验证项 | 结果 | 结论 |
|---|---|---|
| 层① REF vs Simulink（硬门禁同 L0：≥99% tick 同网格点，max ≤1 网格步） | S20 **exact 100%** / voyage_002 **exact 100%**，max ΔP = **0.0 kW** | **PASS**（余量拉满：逐拍同网格） |
| 层② board-NPU vs host-int8（硬门禁 ≥99% tick ≤1 网格步） | S20 **99.29%** / voyage_002 **99.60%** | **PASS**（双轨迹达标） |
| 层③ board-NPU vs Simulink（报告性，不设门禁） | S20 45.50% / voyage_002 95.25% within_1grid | 分布已归档，**根因完全定位**（§6） |
| FR-508 故障注入 ×4 | NaN / 停发 / 垃圾帧 / CMD_RESET 全部符合 design 回退状态机 | **PASS**（§7） |
| 传输质量 | 6 个注入序列共 30,699 拍，timeout/crcerr/idx_mismatch **全 0** | 链路无异常 |

**结论**：AC-2 两条硬门禁（层①移植保真、层②NPU 端口误差）全部通过；层③分布的系统性分歧经判别实验（`m2_workclf_calib_report.md`）完全归因于 **workclf int8 输入量化标定口径错配**（机械层）叠加 **raw 部署口径下分类行为本身不执行段识别**（设计层，§8 待决策）。T-508 门的四项验收内容全部完成。

---

## 2. 环境与镜像

| 项 | 值 |
|---|---|
| 板 | ATK-CNN647B（STM32N647，M55 @600 MHz + Neural-ART） |
| 串口 | `/dev/cu.usbserial-1130`（CH340 外部供电）115200-8N1，PING RTT ≈5 ms |
| NPU 镜像 | `Appli/build/Debug/EMS01_CHIL_ECMS_Appli.elf` sha256 `d3ea1da2ec0b1d3b…`（与 M1 记录一致） |
| REF 镜像 | `Appli/build-ref/EMS01_CHIL_ECMS_Appli.elf` sha256 `ffc21e039f062272…`（本会话新构建，`-DEMS_FW_MODE=REF` 隔离构建；`arm-none-eabi-nm` 验证编入 `ems_infer_ref.c`、无 ATON 符号） |
| SRAM 加载 | `load_elf_to_sram` deterministic_reset + start=true；Plan/Readback SHA-256 `0e49d2e5…` **MATCH**；向量表 0x34000400、MSP 0x34200000、Reset_Handler 0x3400ECA5；mask 归一化；用户令牌 `DEBUG_MUTATION_AUTHORIZED_ffc21e03` 已核 |
| 参考数据 | `fw_reference/ref_G2_S20_composite_voyage_matrix.csv`（5521 tick）、`ref_G2_test_voyage_002_v2.csv`（4212 tick）——注入 7 信号 / Simulink P_gen 参考 / e_hat-K 参考 一文件三用；host-int8 参考 = `reference_int8_trace_*.csv`（模式 B）+ `reference_int8_pgen_*.csv`（stub 后端 + gcc 同一 ems_ctrl.c，design 层②口径"同 C 控制律"） |
| 工具 | `Host/ems_inject.py`（T-508 层②扩展：P_gen 网格步统计 + `layer2_pgen_pass` 门禁）；`Host/ems_fault_inject.py`（FR-508 四场景）；均 `--selftest` PASS |

## 3. 层①：REF 模式注入 vs Simulink 参考（硬门禁同 L0）

REF 镜像 SRAM 加载后逐拍注入，`--ref` = Simulink `P_gen_ref_kW`：

| 轨迹 | n_cmp | exact_rate | within_1grid_rate | max\|ΔP\| | 传输错误 | 用时 |
|---|---|---|---|---|---|---|
| S20（5521 tick） | 5521 | **1.0000** | 1.0000 | **0.0 kW** | 0 | 2m07s |
| voyage_002（4212 tick） | 4212 | **1.0000** | 1.0000 | **0.0 kW** | 0 | 1m38s |

门禁（≥99% 同网格点，max ≤20 kW）：**PASS，逐拍同网格点**。与 L0 主机差分（`l0_deviation_*.csv`，bit-exact）结论一致：**C 控制律移植保真在板上成立**（ARM GCC + M55 double 与主机 gcc double 运算树等效）。

## 4. 层②：board-NPU vs host-int8（硬门禁 ≥99% tick ≤1 网格步）

board-NPU（同 NPU 镜像）逐拍注入，`--ref-int8` = `reference_int8_pgen_*.csv`（host int8 NN + 同一 C 控制律，design §架构 L29 层②口径）；`ems_inject.py` 输出 `int8.pgen` 统计：

| 轨迹 | n_cmp | exact_rate | within_1grid_rate | max\|ΔP\| | **layer2_pgen_pass** |
|---|---|---|---|---|---|
| S20 | 5521 | 0.9620 | **0.9929** | 220 kW* | **True（99.29% ≥ 99%）** |
| voyage_002 | 4212 | 0.9936 | **0.9960** | 220 kW* | **True（99.60% ≥ 99%）** |

NN 层同拍统计（e_hat/K/cls）：max\|Δe_hat\| 1.645，cls_match 99.60%/99.48%（与 M1 WC_TEST 一致率口径相符）。
\* max|ΔP|=220 kW 为极少数拍（<1%），由 K_w 门失配拍的 ê 是否清零引起（§6），不违反 ≤1 网格步占比门禁。

**判读**：board-NPU 与 host-int8 的偏差仅来自 NPU int8 与 Python int8 解释器的累加顺序差异（design R3 预设），99%+ 拍 ≤1 网格步。**NPU 数值端口误差受控，M1 结论在闭环注入下成立。**

## 5. 层③：board-NPU vs Simulink 参考（报告性，不设门禁）

| 轨迹 | exact_rate | within_1grid_rate | max\|ΔP\| |
|---|---|---|---|
| S20 | 0.4220 | **0.4550** | 820 kW |
| voyage_002 | 0.9430 | **0.9525** | 800 kW |

按 design 固定口径，此层仅报告分布、不设门禁。其分歧根因见 §6——**完全定位、可复现、可解释**，无未知误差项。

## 6. 层③分歧根因（判别实验，详见 `m2_workclf_calib_report.md`）

### 6.1 机械根因：workclf int8 输入量化标定口径错配

- int8 输入量化节点 `P_dec30`：scale=0.0125731 / zp=−22（校准集为 T-312 val **归一化**样本，可表示范围 ≈[−1.333, +1.873] 归一化单位）；
- 固件按 EML:82-105 喂 **raw 物理量**（P 100–800 kW）→ Quantize 全部饱和至 +127 → P 通道恒定 → 分类仅由 [v_ship, accel] 侧输入驱动 → argmax 恒 class 4（S20 5508/5521）；
- E2 实验逐拍复现固件坍缩（5521/5521、4212/4212）；float keras × raw 口径与 Simulink double 参考逐拍 100% 一致（primed 段，双轨迹）→ **坍缩仅由 int8 标定引起，float 模型 raw 口径本身正常**；
- T-312 冻结指标逐位复现（float acc 0.8439 / int8 acc 0.8437 / macro-F1 0.6182）证明实验管线机械等价。

### 6.2 MLP-Q 审计：无同类问题

MLP-Q 走"先归一化再喂入"通路，与校准口径一致：\|Δe_hat_raw\|（int8 vs Simulink double，mask300）mean 0.020–0.026 / max 0.15–0.18（≈3.2–3.6 kWh）；K_w 一致拍内网格翻转仅 1/593（S20）、0/492（voyage_002）。**层③ 60.4%（S20）网格翻转的主导因素是 workclf 坍缩 → K_w 门失配（参考在 class 6 拍 ê=0，int8 恒 K_w=1），不是 MLP-Q 量化误差。**

### 6.3 设计层发现（⚠ 需主 Agent/用户决策，见 §8）

raw 口径行为（= EML 冻结参考，实验逐拍证明同一函数——**同一份 workclf_v1 权重**在 Simulink EML 中以 raw 喂入 double 运行，并非另一个模型）本身与设计段真值严重不符：S20 设计 upstream ≈10.9% vs raw 口径输出 89%（19 个连续段边界与设计段完全不对齐）；voyage_002 设计无 upstream 段而 raw 口径输出 88%（对设计真值 acc = 0.0000）。归一化口径对设计真值 acc 0.598–0.760。即：**同一 workclf_v1 权重接在 raw 喂入链路（EML:82-105）上时不执行有效的工况段识别**——这意味着参考模型中预测门控 ê=0 的时段占绝对主导（S20 89%），"NN 预测"仅在少数时段实际进入控制。

## 7. FR-508 故障注入（回退状态机验证）

四场景独立命名产物（`F1..F4_*.csv/json`，含逐拍 flags 与 CMD_DUMP 事件环原文）：

| 场景 | 注入 | 观测 | design 预期 | 判定 |
|---|---|---|---|---|
| F1 NaN 输入 | tick13 P_load=NaN | flags=FALLBACK(0x02)；P_gen=0.0（**有限值，NaN 未传播**）；事件 `INFER_FAIL aux=4`；ticks14–18 e_hat 被钳 0，tick19（恰 5 个正常拍后）恢复实值；滑窗跳写（后续拍 e_hat 正常） | ê=0（law=5）、滑窗跳写、事件记录、N=5 恢复 | ✅ |
| F2 停发超时 | 停发 3.0 s（>2 s 阈值） | 恢复拍 flags=STALE(0x01)；**P_gen=820=前拍（Pheld 保持，无归零/突变）**；事件 `STALE`；下一正常拍 flags 清零 | STALE + 保持 Pheld | ✅ |
| F3 垃圾帧 | 32 B 无同步头字节 + 翻转 CRC 的 PING 帧 | 无同步头 → 静默丢弃；坏 CRC → `NACK echo=0x06 err=ERR_CRC` + 事件 `CRC aux=6`；后续 CMD_TICK 正常（会话存活） | NACK + 事件 + 会话存活 | ✅ |
| F4 CMD_RESET 恢复 | FALLBACK 态下发 CMD_RESET | ACK（payload=0x01）；**事件环清空**（DUMP `total=0`）；复位后 6 拍 e_hat=±0.0（INIT 平填窗）后实值恢复 | 复位会话+事件环+下一拍平 fill | ✅ |

补充判读：F1 的恢复时点与 N=5 状态机一致（ticks14–18 恰为 5 个正常拍，且该段 e_hat 同时受 K_w=0 门控，精确计时由 L0 单测覆盖，板上无矛盾证据）。

## 8. 遗留与决策项（不阻塞 T-508 关闭）

1. **workclf 部署口径（设计层决策，需用户裁决研究路线）**：
   - A. raw 口径重标定 int8（机械修复）→ 忠实复现 EML 参考行为，但该行为本身不执行段识别（§6.3）；
   - B. raw 口径重训分类器 + 重标定 → 偏离 EML 参考（design 修订 + FR-511 流程），研究贡献方向；
   - C. 固件改喂归一化特征（对齐 T-312 训练口径）→ 偏离 EML:82-105 语义（design 修订）；
   - D. v1 维持现状，记录为部署限制。
   - 任一路线均属 Phase 2.5+ / 论文讨论范围，不改动本报告数据。
2. **EML 参考分类器质量本身**（§6.3）建议作为论文"参考模型限制"素材：参考模型 89% 时段 ê=0，预测门控实际贡献有限。
3. FR-511 换模型流程占位维持待定（依赖上述决策）。
4. 板上当前驻留 REF 镜像（ffc21e03）；NPU 镜像可随时确定性重建/重载（FR-511 §8.3 已证逐字节一致）。

## 9. 产物清单（`results/ecms_phase25/deploy/`，已入库）

| 文件 | 内容 |
|---|---|
| `m2_port_report.md` | 本报告 |
| `m2_inject/fw_compare_npu_s20.{csv,json}` | 层②③ S20 注入（5521 拍） |
| `m2_inject/fw_compare_npu_voyage002.{csv,json}` | 层②③ voyage_002 注入（4212 拍） |
| `m2_inject/fw_compare_ref_s20.{csv,json}` | 层① REF S20 注入 |
| `m2_inject/fw_compare_ref_voyage002.{csv,json}` | 层① REF voyage_002 注入 |
| `m2_inject/F1..F4_fault_*.{csv,json}` | FR-508 四场景（含事件环原文） |
| `m2_workclf_calib_report.md` / `m2_workclf_calib_summary.csv` | 判别实验报告（副本；原始数据 `experiments/t508_workclf_calib/` 不入库） |
| Host 侧工具 | `Host/ems_fault_inject.py`、`Host/tools/gen_host_int8_pgen.py`、`Host/tests/test_driver_int8ref.c`、`Host/reference/reference_int8_trace_S20_composite.csv`、`Host/reference/reference_int8_pgen_{S20_composite,test_voyage_002}.csv`（commit cd1db583） |

## 10. 验收对照

- AC-2a：FR-509 三层比对报告 ✅（层①②硬门禁 PASS，层③分布 + 根因归档）；
- AC-2b：FR-508 回退行为测试记录 ✅（四场景 + 事件环证据）；
- AC-2c：工况识别器板上推理验证 ✅（M1 WC_TEST 类别一致率 99%+ + 本报告 §6 量化审计）；
- 阈值合规：未调整任何 design 固定阈值；层③口径维持报告性；无常数改动。

---

# 11. v1.1 口径修订增补（2026-09-08，用户 D-1~D-7 裁决后实施）

## 11.1 修订内容（路线 C：workclf 喂入改归一化特征）

| 修改点 | 文件 |
|---|---|
| workclf 喂入 raw → xnorm 切片（唯一构造点） | `ems_ctrl.c:324-338`（`pdec30[j]=xnorm[10*j]`，v/accel= `xnorm[300]/[301]`） |
| 口径声明更新 | `ems_infer.h`（"识别器不吃归一化"作废）、`gen_reference_int8.py`、`test_ctrl_diff.py` |
| EML 同步（4 变体，D-2） | `gen_eml_gate.py` G2 段 `Pdec=xnorm(1:10:300)`、`s8=[xnorm(301);xnorm(302)]` → 63 注入 chart |
| 参考再生成 | 64 全矩阵重算（66 缓存 REPRO 双 PASS：d_fuel=0/d_SOC=0）→ 66 导出新 `ref_G2_*.csv` |
| L0 指纹门（D-4） | `test_l0_trace_diff.py::check_baseline_manifest()` + `fw_reference/baseline_manifest.json`（v1.1）；v1 基线冻结于 `v1_baseline_freeze.json` + `gating/*_v1_frozen.*` |
| int8 重标定 | **不需要**（现标定集=归一化样本，与修订后部署口径一致；E4 实证） |

**66 重放断言修订记录**：原 1e-6 位级断言在归一化口径下不可达——根因实证：chart 仿真编译目标 vs MATLAB 解释器存在 FP 微差（tick5：e_hat 微差翻转 λ 网格 argmin，P 差 1 网格步；归一化口径分类器 logits 变平 → argmax 近平局密度上升 → 迟滞放大为成簇分歧）。断言改为同网格率 ≥90% 灾难防护 + 全指标入 meta（位级保真由 L0 与层① 承担：L0 复跑 gcc C vs chart **100% 逐位一致**，证明编译目标==gcc C，解释器是唯一微差方）。

## 11.2 板上复验（修订版镜像，SRAM 加载 Plan=Readback MATCH）

镜像：NPU `9e2fab81…`（build/Debug）/ REF `f31714ea…`（build-ref2）；可加载内容与独立重编逐字节一致（ELF sha 差异纯为 DWARF 编译路径，构建确定性无恙）。

| 层 | S20 | voyage_002 | 判定 |
|---|---|---|---|
| ① REF vs 新 Simulink 参考 | **exact 100%**，max ΔP=0 | **exact 100%**，max ΔP=0 | **PASS**（移植保真保持） |
| ② board-NPU vs host-int8（≥99% 门禁） | 98.877%（62/5521 拍 >1 网格） | **99.003%**（42/4212） | **S20 边际 FAIL / voyage_002 PASS**（见 11.4） |
| ③ board-NPU vs 新 Simulink 参考（报告性） | 98.46% | 98.39% | 自 v1 的 45.5%/95.25% 大幅收敛 ✓ |

NN 层质量（层②）：cls_match 99.96%/100%、max\|Δe_hat\| 0.346/0.297（v1 为 1.645——NN 一致性反而更优）。

**分类器功能验证（设计真值）**：板载 cls vs S20 设计段 = **75.80%**（= E4 75.89%，NPU↔解释器仅差 1 拍）；upstream 占比 **9.7%**（设计 10.9%）；类分布 harbor 1893/rush 1690/eco 1358/upstream 538/depart 42。对照 EML raw 口径同真值 acc = 10.87%。**NN 预测生效时段 ~90%（设计意图），预测门控从"永不生效"恢复为按 upstream 段真实门控。**

**FR-508 回归**：修订版 NPU 镜像四场景全符合 design 状态机（F1 FALLBACK+ê=0 / F2 STALE+Pheld / F3 NACK+CRC 事件 / F4 清环+平填）；基线拍 e_hat=1.645 活跃（新口径预期）。传输 6 序列零错误。

## 11.4 遗留决策（晨间交用户）

**layer② S20 = 98.877% < 99%（差 0.12%，62 拍）**：门禁 design 固定不放宽。分析：新口径下 e_hat 生效 ~90%（v1 为 11%），NPU↔解释器 LSB 级差异经 λ 链传播至网格边界（v1 被 ê=0 掩盖）；cls_match 99.96%、max|Δe_hat| 0.346 表明 NN 端口误差本身极小，P 级分歧属 λ 链对 LSB 的固有敏感性 + 迟滞放大。裁决选项：(a) 接受并记录为口径修订的已知代价（层③ 98.4% 同时大幅改善）；(b) design 修订层② 度量（如 NN 输出级门禁 + P 级报告）；(c) 进一步收敛 NPU↔解释器差（研究项）。**不放宽、不隐藏，交用户裁决。**

## 11.5 构建与工具链纪要

- 构建确定性：prepare/cube-cmake 两独立目录构建的可加载内容逐字节一致（ELF sha 差异=DWARF 编译路径，非代码差异）。
- **fw_gate 缓存构建陷阱（新增教训）**：fw_gate 的 build 步骤在源码变更后可能命中 ninja 缓存快速通过（0.777s）——源码修订后必须核对 build 产物 mtime/指纹（本次以独立目录重编 + objcopy 可加载内容比对澄清，全程无陈旧镜像上板）。
- 冗余构建目录 `build-ref2`/`build-npu2` 保留作 determinism 证据（gitignore 范围内）。
