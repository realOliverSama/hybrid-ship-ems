# Phase 4-E AutoDL canonical 环境传输/重建协议（T-007，DGN-E-15）

> 状态：T-007 首次资格化记录 + 后续 canonical 运行的标准协议。
> 凭证纪律：SSH 密码/密钥属用户侧管理，**不入库、不进脚本**（design Security 节）；
> 本文档只记录连接拓扑与流程。

## 1. 连接拓扑

| 项 | 值 |
|---|---|
| 主机 | `connect.cqa1.seetacloud.com`（AutoDL，实例名/计费属用户侧） |
| SSH 端口 | `18531`（用户侧若重启/换实例以 AutoDL 控制台为准） |
| 账号 | `root`（AutoDL 容器默认） |
| 认证 | SSH 公钥（`~/.ssh/id_ed25519.pub` 已装 `authorized_keys`；首次用密码安装——密码不入库） |
| GPU | 1× NVIDIA RTX 4090 D（24GB，sm_89；驱动 550.67，CUDA 12.4） |
| 磁盘 | 系统盘 30G（Base 镜像 ~9G）+ 数据盘 50G（`/root/autodl-tmp`）——**大件一律落数据盘** |
| OS / Python | Ubuntu 22.04；`/root/miniconda3/bin/python3.12`（3.12.3） |

## 2. canonical 环境身份（T-007 资格化产物）

- **venv**：`/root/autodl-tmp/ems_venv`（数据盘）；
  `python -m venv` 创建，`pip install 'tensorflow[and-cuda]==2.19.1' numpy==2.1.3 scipy pytest`
  ——TF 版本与本地 macOS 严格一致（FR-591）；CUDA 运行库经 pip `nvidia-*` 包（cuDNN/cuBLAS 等），
  依赖驱动 550.67 兼容（驱动向下兼容 CUDA 12.x runtime）。
- **pip 缓存**：安装完成后 `rm -rf ~/.cache/pip`（否则 ~3GB 占用系统盘——用户磁盘约束 2026-09-13）。
- **代码树**：`/root/autodl-tmp/hybrid-ship-ems`（见 §3）。
- **正式证据**：`env_manifest_cuda.json` / `env_smoke_cuda.json` / `determinism_probe_cuda.json`
  / `requirements_tf_cuda_lock.txt`（本地仓库 `results/predictor_v3/` + `config/predictor_v3/env/`）。

## 3. 代码传输协议（git commit 钉版）

1. 本地仓库工作树提交干净（`git status` 无未提交改动——改动只能回本仓库走 git，**禁现场改码**）；
2. `git archive HEAD <路径清单> | ssh -p <port> root@<host> "tar -x -C /root/autodl-tmp/hybrid-ship-ems"`
   ——钉版 = archive 的 commit SHA；每次 canonical 运行记录该 SHA（FR-592/DGN-E-15）。
3. **传输路径清单（T-007 资格化，HEAD=131624bf）**：
   `AGENTS.md` + `model/simulink_modelE_(Phase4_Physics)/{README.md, README_Model.md,
   predictor_v3/, dataset_v3/, config/, results/}`。
   **不传输**：`firmware/` vendor 大件（对 E 无用）、`datasets_v3/formal/{iid,ood}/` raw .mat
   （345MB——OOD 权威消费面 = T-003 物化窗口 npz；IID 训练消费 windows npz；raw 语料仅本地审计）、
   历史 `datasets/fleet_windows_v2.npz`（SUPERSEDED）。
4. 若需 GB 级传输（批量 checkpoint 回传等）：SSH 通道实测 **~0.7MB/s**（AutoDL 平台限速，
   官方文档将公网网盘中转列为大文件推荐）——改走 AutoDL 网盘中转（上传→实例内 `autodl-fs` 拷至
   `/root/autodl-tmp`），中转后仍以 sha256 校验为准。

## 4. 数据传输协议（rsync + hash 校验）

传输清单（T-007 执行）+ 校验：

| 本地路径（相对仓库） | 远端落位（相对 /root/autodl-tmp/hybrid-ship-ems） | sha256 |
|---|---|---|
| `model/simulink_modelE_(Phase4_Physics)/datasets_v3/formal/windows_v3_formal.npz` | 同相对路径 | `203dc92d…91497`（冻结值一致即 PASS） |
| `model/simulink_modelE_(Phase4_Physics)/artifacts/predictor_v3/data/ood_windows_v3_formal.npz` | 同相对路径 | `fea3f842…`（对照 `results/predictor_v3/ood_windows_integrity.json` 的 file_sha256） |

- **OOD npz 解封前仅不透明字节复制 + hash 校验**（tasks gate R1）——远端不得反序列化/科学查看；
  远端运行 `ood_windows_integrity.json` 的 `file_sha256` 对照即可确认完整性。
- 校验命令（远端）：`sha256sum <path>`，对照本地冻结值；不一致即停（禁带病运行）。
- JSON/config 工件（norm ext/三 config/results JSON）经 git archive 钉版自带，无需 rsync。

## 5. 远端运行约定

```bash
# 环境变量（写入 ~/.bashrc 或运行前 export）
export EMS_PREDICTOR_V3_ROOT=/root/autodl-tmp/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)
export PYTHONHASHSEED=<run 固定值>   # 进程启动前设置（FR-594）

# 运行（predictor_v3 包以 Model E 根为工作目录）
cd "$EMS_PREDICTOR_V3_ROOT"
/root/autodl-tmp/ems_venv/bin/python -m predictor_v3.env_tools --device cuda
/root/autodl-tmp/ems_venv/bin/python config/predictor_v3/env/env_smoke_generic.py \
    --platform cuda --npz datasets_v3/formal/windows_v3_formal.npz \
    --out results/predictor_v3/env_smoke_cuda.json
```

- 训练中间产物（checkpoint/TensorBoard）只写
  `experiments/predictor_v3/`（数据盘；`.gitignore` 已覆盖）。
- 回传：结果 rsync/scp 回本地 `results/predictor_v3/` 精选入库（回传后本地复算 hash）。

## 6. 再资格化协议（FR-591）

canonical 环境版本变更（TF/numpy/CUDA 栈/驱动/镜像更换/新实例）后：
1. 重跑 CUDA 可复现探针（`env_tools --device cuda`，双跑）；
2. 以一个 frozen config 复跑对拍（与变更前同 config 同 seed 的 val 指标对照）；
3. 重发 `env_manifest_cuda.json`（新版本字段）——在此之前不得出 result-bearing 证据。

## 7. T-007 资格化执行记录（2026-09-13）

1. 公钥安装 → 免密 SSH；环境侦查（4090D/550.67/CUDA 12.4/Ubuntu 22.04/Py3.12.3/磁盘）；
2. venv + `tensorflow[and-cuda]==2.19.1` 安装（数据盘），GPU matmul 验证通过，pip cache 清除；
3. 代码：git archive HEAD=131624bf 核心子集（137MB 落盘）；
   数据：npz×2 rsync + sha256 对照（见 §4 表）；
4. CUDA 兼容 smoke（`env_smoke_generic.py --platform cuda`）→ `env_smoke_cuda.json`；
5. CUDA 确定性探针（`env_tools --device cuda` 双跑）→ `determinism_probe_cuda.json`；
6. `requirements_tf_cuda_lock.txt` = 远端 `pip freeze` 全量 + 关键 wheel sha256；
7. 全部证据 JSON 回传本地入库。
