# model/simulink_modelD_(EMS_NN_AECMS) — 目录索引（概括）

> Model D 研究副本：定时前馈 EMS + ECMS_NN（NN 预测增强 A-ECMS）研究工作区。
> 两级文档分工见仓库 `AGENTS.md` §20.1：本文件为概括索引，`README_Model.md` 为详细说明。

## 顶层内容

| 条目 | 类型/入库 | 说明 |
|---|---|---|
| `model/` | 目录（入库） | Simulink 模型与库（`HybridShip_ModelD_DCBus.slx` 等） |
| `scripts/` | 目录（入库） | 01~10 可重复执行构建/仿真脚本（`10_build_modelD.m` 为重建入口）；`20_build_plantstep_model.m`（T-601：装载主模型 SaveAs 原位变换构建被控对象子集变体 `HybridShip_ModelD_PlantStep.slx` + 结构审计 JSON，主模型零改动）；`21_run_v3_native_arms.m`（T-610：v3 原生闭环臂 law=7/5 × 两场景运行导出，22 列含逐拍 fuel，双遍 REPRO）；`22_run_gp1_gate.m`（T-602/603：G-P1 被控对象子集保真门禁，ref_G2 回放对照，双场景位级 PASS）；`23_codegen_plantstep.m`（T-604：S20/voyage002 双场景按场景烘焙 VOYAGE_*/SCN → ERT（回退 GRT）生成 PlantStep C 代码至 EMS01 Host/plantstep/<scenario>/ + build_manifest.json 指纹，模型只装载不保存）；`24_export_gp2_simref.m`（T-605 配套：解释执行运行采样 logsout 导出 G-P2 sim 参考 16 列 CSV，模型零改动） |
| `config/` | 目录（入库） | 参数表、工况定义（Model_Parameters / Model_Scenarios） |
| `python/` | 目录（入库） | 训练与导出脚本（负荷预测器等；ECMS_NN 训练管线在 phase25 交付链） |
| `datasets/` | 目录（精选入库） | 航次数据 raw / train / val / test（v2 内河工况扩展见 phase25） |
| `results/` | 目录（精选入库） | ecms_phase0/1/2/25 实验结果（phase25 = NPU 导向模型族 + EMS01 固件参考链） |
| `reports/` | 目录（精选入库） | 审计/对比报告 |
| `legacy_reference/` | 目录（只读副本） | 原模型 HybridShip0730.slx |
| `README_Model.md` | 文件（入库） | 模型工作区详细说明书（结构/控制律/预测模型清单/产物登记） |
| `项目结构.txt` / `debug_law7_eml_law7.txt` | 文件（入库） | 结构备忘与 law7 调试记录 |
| `HybridShip_ModelD_DCBus.slxc`、`slprj/` | 产物 | **永不入库**（编译缓存；.gitignore 排除） |

## results/ecms_phase25 速览（ECMS_NN 主线，详细见 README_Model.md §产物登记）

| 子目录 | 内容 |
|---|---|
| `models/` | MLP-Q（mlp_h64_32_lr3e-4_q）float/int8 tflite + 训练报告 |
| `classifier/` | workclf_v1（工况识别器）float/int8 + T-312 报告 |
| `gating/` | G2 门控仿真 run 缓存 |
| `fw_reference/` | **Simulink law=7 G2 参考轨迹**（ref_G2_*.csv，注入 7 信号 + P_gen_ref 一文件三用）+ L0 差分结果 |
| `deploy/` | EMS01 固件部署产物：**m1_deploy_report.md**（T-507/M1）、**m2_port_report.md**（T-508/M2）、`m1_infer_test/`、`m2_inject/`（三层比对 + FR-508 故障注入数据）、`m2_workclf_calib_report.md`（workclf int8 口径判别实验）、NOR 证据 |
| `closedloop/` | v3 真闭环产物（T-610/611 原生臂 native_arms：law=7 MLP-Q EML 内嵌 double + workclf G2 门控闭环 / law=5 SOC-Adaptive ECMS 基线，4×CSV + summary；`v3_closedloop_report.md` = T-616 四臂×两场景统一报告） |
| `plantstep/` | T-601 被控对象子集构建结构审计 JSON（plantstep_build_audit.json） |
| `fleet_profiles/` | 航次工况库（voyage_*.mat） |
| `segment_labels/` / `audit/` / `fleet/` | 段标签、审计、fleet 数据 |
