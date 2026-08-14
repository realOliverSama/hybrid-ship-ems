# AGENTS.md — hybrid-ship-ems

本文件是本仓库对所有编码 Agent（Pi、Kimi 等）的**强制性行为约束**。
任何 Agent 在本仓库工作时必须先读本文件，且本文件优先级高于会话中的临时指令冲突。
注意：本文件是行为约定，破坏性操作另由 Pi safety extension 与 GitHub protected branch 做技术强制。

---

## 1. 项目概述

混合动力船舶能量管理（EMS）研究项目，三条主线：

| 目录 | 内容 | 关键点 |
|---|---|---|
| `model/simulink/` | Simulink EMS 模型、场景、参数、AI 对比实验 | 全部通过脚本驱动，禁止手工 GUI 改动 |
| `model/simulink/python/` | MLP/LSTM 负荷预测训练与导出（.keras/.tflite） | 使用 `.venv_tf` 虚拟环境 |
| `firmware/stm32n6/` | AlienTek STM32N647 厂商包 + 自有固件工程 | vendor 包只读；部署走专门 skill |
| `data/`、`experiments/` | 本地原始数据与实验暂存 | **不入库** |
| `docs/` | 项目文档 | |

Simulink 流水线入口（按序执行）：

```text
model/simulink/scripts/
  01_audit_legacy_model.m   → 02_build_modelA.m   → 03_run_smoke_tests.m
  04_run_scenarios.m        → 05_export_dataset.m → 06_validate_results.m
  07_run_ai_comparison.m
```

## 2. Git 纪律（最高优先级）

### 🟢 可自动执行

- 创建 `feat/*`、`fix/*`、`chore/*` 分支
- `git status / diff / log / fetch`
- **逐个显式** `git add <file>`（仅限本 session 修改的文件）
- `git commit`（禁止 `--no-verify`）
- `git push origin <feature-branch>`
- 创建 Pull Request

### 🟡 必须用户明确批准

- 删除/移动重要源文件或 dataset
- 修改：`.gitignore`、`.gitattributes`、`AGENTS.md`、`.pi/`、`.github/`、CI、工具链/依赖版本
- merge 到 `main`；创建/删除 tag；创建 GitHub Release
- 大规模目录重构；引入新的大型 dependency；更新 vendor STM32 包
- 对**未 push 且确认为 disposable** 的分支做 `reset`（默认不做）

### 🔴 技术层面禁止（无任何例外）

```bash
git push --force / -f / --force-with-lease
git reset --hard
git clean -fd / -fdx
git checkout .  /  git restore .
git add .  /  git add -A
git filter-repo
git commit --no-verify
rm -rf <项目内任何路径>   # 删除一律用精确路径的单文件 rm 或移至废纸篓
```

### 回退规则

- 已 push 的 commit → 只允许 `git revert`
- `reset` 仅在「未 push + disposable 分支 + 用户批准」三条件同时满足时才可考虑

### Tag 策略

日常实验用 commit SHA 定位，不打 tag。Tag 仅用于里程碑（如 `model-a-v1.0`、`thesis-experiment-final`），需用户批准。

## 3. 标准任务流程

```text
1. git checkout -b feat/<task>
2. 修改代码/模型（Simulink 全部脚本化）
3. 运行门槛测试（见 §4），FAIL 则继续修，禁止带着 FAIL 提交
4. git add <逐个明确文件>
5. git diff --cached --stat && git status --short   # 自查无夹带
6. git commit -m "<type>(<scope>): <描述>"
7. git push origin feat/<task>
8. 按 §6 模板向用户汇报
```

commit message 沿用仓库现有风格：`type(scope): 中文描述`，type ∈ {feat, fix, chore, docs, test, refactor}。

## 4. 质量门槛（commit 前置条件）

- `03_run_smoke_tests.m` 全部通过
- 场景回归 S01–S05（及新增场景）不允许退化；判定依据为**硬阈值**（speed RMSE、SOC 上下界、power balance 残差、仿真 NaN/Inf=0 等），阈值定义于 `model/simulink/scripts/06_validate_results.m` 及 scenario 配置
- 修改 `.slx` 时：必须先做结构差异自查（Simulink Model Comparison 或等效脚本），确认没有意外改动无关 block/参数，再提交
- 任一项 FAIL → 停止提交，报告用户或继续修复

### 机器可读门禁（禁止主观解读终端输出）

- 阈值集中于 `config/ModelA_GateThresholds.m`（修改 = 改门禁标准，🟡 类）
- 回归基线 `config/gate_baseline_smoke.json`（入库的 golden reference；重建需 🟡 批准）
- 每次跑 `03` 产出 `results/gate_result.json`（运行时产物，不入库）：`overall=true` 才允许提交
- `03`/`06` 在 FAIL 时以 `error('GATE:*')` 终止——Agent 必须以工具调用的失败为准，不得无视

## 5. 产物入库策略

| 产物 | 策略 |
|---|---|
| `model/simulink/datasets/`、`results/`、`reports/` | 精选的实验产物**入库**，随对应 commit 一起提交 |
| `data/`、`experiments/` | 本地暂存，**不入库** |
| `slprj/`、`_jitprj/`、`_sfprj/`、`*.mex*`、codegen | 永不入库（.gitignore 已保证） |
| `firmware/**/Binary/*.hex`（fsbl/network-data/appli） | vendor 源资产，**入库** |
| `firmware/**/flash/*.hex`、`build*/`、`*.elf/*.map` | 生成产物，永不入库 |
| `.env`、`.venv*/` | 永不入库 |

## 6. 任务汇报模板

每次任务结束必须输出：

```text
Task:        <一句话>
Branch:      feat/xxx
Commit:      <short-sha>
Changed:     <N files + 关键路径>
Validation:  S01..S0x PASS/FAIL 矩阵（FAIL 不允许到达此步）
Results:     <关键指标对比>
Generated:   <新增报告/数据路径>
Remote:      pushed: origin/feat/xxx
Main:        unchanged
Working tree: clean        # git status --short 为空才可写 clean
Destructive operations: none   # 以 .pi/safety-audit.log（本地，不入库）为取证依据
```

## 7. STM32 固件红线

- vendor 包（`firmware/stm32n6/04_SourceCode/SoftwarePackage/`）视为**只读**，除修复性 patch 并经批准外不修改
- 调试/加载走 `debug-stm32n647-sram` skill：只做 SRAM 加载，**禁止**用通用 MCP flash 工具擦写外部 NOR
- 烧录 Release 走 `release-stm32n647-appli` skill：禁止 mass erase，保留既有 FSBL
- 固件构建走 `prepare-stm32n647-project` skill（离线，不碰板子）

## 8. 其他

- 本文件的任何修改本身属于 🟡 类，需用户批准
- 技术强制层：`.pi/extensions/hybrid-ship-safety.ts`（hard-block / confirm-gate），
  其拦截记录写入 `.pi/safety-audit.log`；扩展文件的修改同属 🟡 类
- 多 Agent 并行工作时，各自使用独立 feature 分支，禁止跨 session 代提交他人改动
