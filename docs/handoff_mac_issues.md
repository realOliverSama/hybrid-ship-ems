# Mac 端 Agent 交接 — Phase 2 遗留环境依赖问题 × 2

> 来源：Windows 端 debug/MATLAB-debug 会话（已合并至 feat/adaptive-ecms-methanol @ 1dba0264）。
> 这两个问题均为**环境依赖**（Windows 本机缺 .venv_tf / MATLAB R2026a Windows 行为），
> 在 Mac 端可能表现不同或直接可解。以下提供完整上下文、复现步骤与期望产出。

---

## 问题 2：T1 Python 交叉校验无法运行（.venv_tf 缺失）

### 背景

`model/simulink_modelD/scripts/+ecms2/test_law7.m` 的 **T1 测试**是 law=7 NN 前向的
权威位一致性校验：`ecms2.nn_predict_ehat`（MATLAB 实现）vs Python 推理
（`train_predictor.py` 同款 `_np_mlp_forward`），100 组随机输入，
归一化空间 max|Δ| < 1e-5。Phase 2 收尾时在原机器上全绿（证据 commit 511f28ae）。

当前 Windows 机器上 `.venv_tf` Python 环境整体缺失（已确认
`model/simulink/.venv_tf/` 不存在，Scripts/bin/python.exe 均无），
T1 在 line ~126 assert 失败：

```
T1 FAIL: .venv_tf python not found under E:\...\model\simulink\.venv_tf
```

test_law7.m 已修复为跨平台探测（Windows `Scripts\python.exe` / Unix `bin\python`
自动适配），只差环境本身。

### Mac 端要做的事

1. **重建 .venv_tf 环境**：在 `model/simulink/` 下按项目惯例创建 Python venv
   （TensorFlow/Keras 训练链路所用依赖见 `model/simulink/python/train_predictor.py`
   头部 import；若仓库有 requirements 文件以其为准）。
   注意 test_law7.m 的 T1 调用方式是直接执行 venv 内 python 解释器跑一段内联
   numpy 推理脚本（不 import tensorflow，仅需 numpy），所以最小依赖 = numpy。
2. **重跑 T1 验证**：

   ```bash
   cd model/simulink_modelD
   # MATLAB 内:
   r = ecms2.test_law7(ROOT);   % ROOT 缺省自动探测
   % T1 通过判据: max|Δ| < 1e-5（归一化空间，100 组随机输入）
   ```

3. **期望产出**：`results/ecms_phase2/test_law7.json` 中
   `t1_max_delta_y_norm < 1e-5` 且整体 pass=true（T2/T3 需要 .slx 与权重 mat 就绪）。

### 注意事项

- T1 只需要 numpy（纯前向矩阵运算）；不需要 GPU/TensorFlow。
- 若 Mac 上 numpy 版本与训练时不同，浮点求和顺序差异可能导致 |Δ| 略大于 0，
  但阈值 1e-5 容差充足，历史上实测 |Δ|=1.2e-14。
- **T1 失败绝不可通过放宽阈值解决**——它是内联化等价性的最终锚点；
  当前内联版的等价性由 S01 黄金值逐位对比临时承载，T1 补跑后才算完整闭环。

---

## 问题 3：parsim SetupFcn 报错（批量并行仿真降级串行）

### 背景

Phase 2 的 T-201 fleet-200 数据集生成使用 Parallel Computing Toolbox 的 parsim
批量仿真。PCT 安装后 parsim 池可用，但 **SetupFcn 报错导致降级为串行**
（200 个航次串行跑，耗时大幅增加）。当时教训记录："p 码 exist 返回 6≠2"。

具体机制：模型引用的某个工具箱函数是 p-code（`.p` 文件），`exist('name','file')`
对 p-code 返回 6（p-code 类）而非 2（普通 m 文件）——SetupFcn 内部若用
`exist(...) == 2` 判断文件存在性会误判，进而抛错。

Phase 2 日志原文（项目开发进展.md 2026-08-25 条目）：
"T-201 随机航次生成器 + 200 航次剖面（零降额；PCT 安装后 parsim 池可用但
SetupFcn 报错降级串行，教训：p 码 exist 返回 6≠2）"

### Mac 端要做的事

1. **定位 SetupFcn**：parsim 的 SetupFcn 通常在批量实验脚本中定义
   （搜索 `model/simulink_modelD/scripts/` 下含 `parsim` 或 `SetupFcn` 关键字的
   脚本；fleet 生成相关为 `50_voyage_generator.m` / `51_generate_fleet_profiles.m` 一带）。
2. **修复 exist 误判**：将形如 `exist(fname,'file') == 2` 的判断改为
   兼容 p-code 的形式，例如：
   ```matlab
   if exist(fname, 'file') == 2 || exist(fname, 'file') == 6
   ```
   或统一改用 `which(fname)` 非空判断。
3. **验证并行恢复**：重跑 fleet 批量生成（或小规模 10 航次子集），
   确认 parsim 池正常工作不再降级串行。
4. **期望产出**：parsim 正常并行执行的日志证据 + 修正后的脚本提交。

### 注意事项

- 该问题与平台无关（Windows/Mac 都会触发），Mac 上大概率可直接复现并修复。
- 修复时不要改动 fleet 数据集本身的内容语义——只修 SetupFcn 的存在性判断逻辑。
- Phase 3 若有大规模参数矩阵实验，此修复是前置条件（否则又是数小时串行）。

---

## 附：本轮已完成（无需 Mac 处理）

- law=7 内联化产品落地：根因（R2026a EMChart 跨块变量缺陷）+ 两颗衍生 bug
  全部击破，全链验证全绿（law=7 S01 fuel 与兜底版逐位一致）。
- 新发现已知问题：**MATLAB R2026a 下，同进程内先跑 10b smoke 再跑 law=7 仿真
  会导致 law=7 行为退化**（fuel 85.83 vs 正确 91.23，bisect 实证 SMOKE-STAGE POISON；
  变体切换+EMChart 组合行为，疑似 R2026a 层面）。规避方法已固化：功能验证在前、
  smoke 收尾；必要时清 slprj 缓存。Mac 上请留意是否复现（不同版本/平台可能不同），
  若复现请补充现象到 `reports/law7_inline_root_cause.md` §8。
