# T-305 跨机交接文档（Windows 主机 → MacBook Air M1）

> 生成：2026-08-29（Session：T-305 双根因修复后）。本文档供另一台电脑上的
> coding agent 在全新会话中接手 T-305 剩余工作。接手 agent 必须先完整阅读本文档，
> 并遵守仓库根目录 `AGENTS.md` 的全部纪律（尤其 §2 主 Agent/Subagent 职责、
> §7 Git 红线、§10 AI 红线）。

## 1. 任务背景（30 秒版）

- 项目：混合动力船舶 EMS，Phase 2.5 = 神经网络预测型 A-ECMS。
- Spec：`.specs/nn-predictive-ecms-phase25/`（requirements/design/tasks 三件套 +
  T-301 论证文档）。当前任务 **T-305**：TCN 主推架构训练 + 规模扫描。
- 数据集 v2 已就绪：400 航次闭环仿真（零拒绝）、155,087 窗口、
  split 280/60/60（seed 20260829）、fit-on-train 归一化。数据文件**已入库**，无需传输。
- T-305 扫描协议（主 Agent 定稿，勿改）：
  - 配置（**四组全跑，M1 统一协议**）：`tcn_f16 / tcn_f32 / tcn_f64 / tcn_f128`
    （kernel=3，dilations=[1,2,4,8,16,32,32]，R=381≥300，Adam 1e-3）
  - 训练参数：`--batch 512 --epochs 100`（统一 early-stopping：patience 10，
    val 决定实际停止；四组同协议保证可比性）
  - `--quantiles` 全开（q10/q50/q90 pinball）
  - **Windows 主机上的 f16（40 epoch）结果仅为跨机 sanity anchor，不进正式选模**；
    正式选模数据以 M1 上四配置统一协议结果为准

## 2. 关键历史（为什么代码长这样——勿"顺手改回"）

1. **GAP→末时刻**：`build_tcn` 原用 GlobalAveragePooling1D，会抹掉"当前时刻 t"
   的位置信息，导致输出塌缩到全局中位数（val RMSE 16.88 ≈ naive-median 16.87 实锤）。
   现为末时刻 slice（causal 卷积末步已聚合 R=381 全部历史）。**不要改回 GAP**。
2. **quantile_loss_3 广播 bug**：原实现 `(N,1) − (N,)` 广播成 (N,N) 逐对交叉矩阵，
   损失退化为全局中位数回归。已修为 `e = y1 − p` 逐样本 (N,3)。
   tiny-overfit 验证：修复前 TCN-pinball 16.54（塌缩），修复后 8.71（=MSE 版 8.64 水平）。
3. `--only` 单组过滤已支持 scoped 模式：`--arch tcn --only tcn_f16` 只跑一组。

## 3. M1 环境搭建

```bash
# 1) 拉取仓库（分支已含全部代码/数据/文档）
git clone -b models-research https://github.com/realOliverSama/hybrid-ship-ems.git
cd hybrid-ship-ems

# 2) Python 环境（macOS ARM）
python3 -m venv .venv_m1
source .venv_m1/bin/activate
pip install tensorflow tensorflow-metal   # M1 GPU 加速（比 Windows CPU 快 10~30x）
pip install numpy scipy pandas matplotlib

# 3) 自检：确认数据文件在位（应均为已入库文件）
ls "model/simulink_modelD_(EMS_NN_AECMS)/datasets/"        # fleet_windows_v2.npz 等
ls "model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/fleet_profiles/" | head  # 400 .mat

# 4) 冒烟测试（必做，~2 分钟）：tiny-overfit 应得 train RMSE ≈ 8.7 kWh
cd "model/simulink_modelD_(EMS_NN_AECMS)"
python python/train_predictor.py --arch tcn --only tcn_f16 --dataset v2 \
  --quantiles --no-plots --batch 512 --epochs 2 --epochs 不改，冒烟见下
```

冒烟测试标准做法（在仓库根目录的 python 里跑，2 epoch 全量约几分钟，
val RMSE 应显著 <16.5 kWh 即说明环境正常；若 ≈16.9 说明环境/代码异常，立即停）。

## 4. 接手后的正式工作

```bash
cd "model/simulink_modelD_(EMS_NN_AECMS)"
# f32（约几十分钟 on M1）
python python/train_predictor.py --arch tcn --only tcn_f32 --dataset v2 \
  --quantiles --no-plots --batch 512 --epochs 40 2>&1 | tee t305_f32_m1.log
# f64（启动前必须先过 f32 硬门，见下）
python python/train_predictor.py --arch tcn --only tcn_f64 --dataset v2 \
  --quantiles --no-plots --batch 512 --epochs 40 2>&1 | tee t305_f64_m1.log
```

### 异常硬门（用户 2026-08-29 设立，不得跳过）

- 任一配置 val RMSE > 14.7 kWh（persistence 水平）或 pred 接近常数
  （q_coverage 异常）→ **停止后续配置**，回报用户并做 tiny-overfit 复测；
- 正常预期：f16 ≈ Windows anchor 水平或更好，随 filters 增大单调改善或持平；
- 判定依据：`results/ecms_phase25/training_log_v2.csv` 各行的
  `val_rmse_kwh / val_mae_kwh / q_coverage / q_pinball_norm`。

### 记录要求（T-305 验收）

- 每组配置：val RMSE(q50)/MAE/R²/under_freq/q_coverage/q_pinball +
  params 数 + int8 估算大小（params×1 字节）——前 6 项脚本自动写
  `results/ecms_phase25/training_log_v2.csv`；
- 失败配置也必须留在 log/csv 中，不得删除；
- 完成后：spec 勾选（`.specs/nn-predictive-ecms-phase25/tasks.md` T-305）、
  开发日志（`项目开发进展.md` 顶部追加）、git commit+push（见 Git 纪律）。

## 5. Git 纪律（摘自 AGENTS.md §7，接手 agent 必须遵守）

- 允许：`git add <具体文件>`（逐个）、`git commit`、`git push origin models-research`
- 禁止：`git add .` / `git add -A` / `git reset --hard` / `git clean` / force push
- 结果文件提交范围：`results/ecms_phase25/training_log_v2.csv`、
  `results/ecms_phase25/models/` 下新产物、本机已推送的 f16 结果**不得覆盖**
  （M1 拉取最新分支后再开跑，避免 CSV 冲突；若冲突，保留两行并说明）

## 6. STOP 条件（出现任一立即停止并报告用户）

- tiny-overfit 冒烟 RMSE ≈17（环境/代码异常）
- f32 无改善且 tiny 复测也无法过拟合
- 需要改动 split/归一化/数据定义/loss 语义才能继续
- M1 上 TensorFlow-metal 不可用导致只能 CPU（可继续，但先报告预计时长）

## 7. 联络上下文

- 本机（Windows）状态：f16 修复版正在后台跑（40 epoch，~2h），完成后结果经
  `training_log_v2.csv` 推送至分支；M1 接手前先 `git pull` 拿到它。
- 完整规格：`.specs/nn-predictive-ecms-phase25/{requirements,design,tasks}.md`；
  数据集构建：`python/build_dataset_v2.py`；诊断历史：`项目开发进展.md` 2026-08-29 条目。
