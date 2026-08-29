# -*- coding: utf-8 -*-
"""colab_scan.py — T-305 Colab VM 训练启动/轮询脚本 v2（免费版回收容忍设计）

背景：v1 单进程跑四配置（~2.5-4h）在免费版 Colab 上被运行时回收（08:53Z terminated），
进度全部丢失。v2 改为 A+B 组合：
  A. 断点续跑：四配置逐个跑（--only 单组过滤），每完成一个立即同步 Google Drive
     （/content/drive/MyDrive/t305_backup/），并以 done_<cfg>.flag 作断点标记；
     VM 被回收后重启最多损失"正在跑的当前配置"。
  B. 保活：由主 Agent 侧监控 subagent 高频 exec 轮询维持内核活跃（尽力而为）。

经 colab-cli 使用：
    colab new -s t305 --gpu T4                       # 开 VM（凭据已持久化）
    colab drivemount -s t305 /content/drive          # 挂载 Drive（持久化层）
    colab exec -s t305 -f python/colab_scan.py       # 启动模式：clone+恢复断点+驱动循环
    colab exec -s t305 -f python/colab_scan.py poll  # 无效：CLI 不传参，见 poll 专用脚本
"""
import os
import subprocess
import sys

BRANCH = "models-research"
REPO = "https://github.com/realOliverSama/hybrid-ship-ems.git"
WORK = "/content/hybrid-ship-ems/model/simulink_modelD_(EMS_NN_AECMS)"
BK = "/content/drive/MyDrive/t305_backup"
CONFIGS = ["tcn_f16", "tcn_f32", "tcn_f64", "tcn_f128"]

DRIVER_SH = r"""#!/bin/bash
cd /content/hybrid-ship-ems/model/simulink_modelD_(EMS_NN_AECMS) || exit 1
BK=__BK__
RES=results/ecms_phase25
mkdir -p "$BK/models"
for cfg in __CONFIGS__; do
  if [ -f "$BK/done_$cfg.flag" ]; then
    echo "[driver] $cfg 已完成（Drive 断点命中），跳过" >> t305_scan.log
    continue
  fi
  echo "[driver] ==== START $cfg $(date -u +%FT%TZ) ====" >> t305_scan.log
  python python/train_predictor.py --arch tcn --dataset v2 --quantiles --no-plots \
      --batch 512 --epochs 100 --only "$cfg" >> t305_scan.log 2>&1
  rc=$?
  if [ $rc -eq 0 ]; then
    cp -f "$RES/training_log_v2.csv" "$BK/training_log_v2.csv"
    mkdir -p "$BK/models/$cfg"
    cp -f "$RES/models/$cfg/"* "$BK/models/$cfg/" 2>/dev/null
    touch "$BK/done_$cfg.flag"
    echo "[driver] $cfg DONE rc=0，已同步 Drive $(date -u +%FT%TZ)" >> t305_scan.log
  else
    echo "[driver] $cfg FAILED rc=$rc，停止后续配置" >> t305_scan.log
    break
  fi
done
echo "[driver] ===== 全部配置结束 $(date -u +%FT%TZ) =====" >> t305_scan.log
"""


def sh(cmd, cwd=None):
    r = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    out = (r.stdout or "") + (r.stderr or "")
    return r.returncode, out


def launch():
    if not os.path.isdir("/content/hybrid-ship-ems"):
        code, out = sh("git clone -b %s %s /content/hybrid-ship-ems" % (BRANCH, REPO))
        print("clone:", code, out[-300:])
    os.chdir(WORK)
    code, out = sh("git pull origin %s" % BRANCH)
    print("git pull:", code, out[-200:])
    import tensorflow as tf
    print("TF:", tf.__version__)
    print("GPU:", tf.config.list_physical_devices("GPU"))
    sh("pip install -q scipy pandas 2>&1 | tail -1")
    # 持久化层检查（Drive 需先经 colab drivemount 挂载）
    if not os.path.ismount("/content/drive"):
        raise SystemExit("FATAL: /content/drive 未挂载，请先执行 colab drivemount -s t305 /content/drive")
    os.makedirs(BK, exist_ok=True)
    # 断点恢复：把 Drive 上的已完成产物还原进工作区（CSV 行 + 模型）
    code, out = sh("cp -f %s/training_log_v2.csv results/ecms_phase25/ 2>/dev/null; "
                   "cp -rf %s/models/* results/ecms_phase25/models/ 2>/dev/null; true" % (BK, BK))
    flags = sh("ls %s/done_*.flag 2>/dev/null" % BK)[1].split()
    print("断点标记:", flags or "无（从头开始）")
    # 写驱动脚本并后台启动
    driver = DRIVER_SH.replace("__BK__", BK).replace("__CONFIGS__", " ".join(CONFIGS))
    with open("t305_driver.sh", "w") as f:
        f.write(driver)
    sh("rm -f t305_scan.log")
    code, out = sh("nohup bash t305_driver.sh >> t305_scan.log 2>&1 & echo $!")
    print("驱动循环已后台启动 PID:", out.strip())
    print("配置顺序:", CONFIGS)
    print("产物备份目录:", BK)


def poll():
    os.chdir(WORK)
    code, out = sh("tail -n 25 t305_scan.log")
    print(out or "(日志为空)")
    code, out = sh("cut -c1-200 results/ecms_phase25/training_log_v2.csv 2>/dev/null || true")
    print("--- CSV（截断 200 列）---")
    print(out or "(尚无完成行)")
    code, out = sh("ls %s/done_*.flag 2>/dev/null | xargs -n1 basename 2>/dev/null" % BK)
    print("Drive 断点:", out.strip() or "无")
    code, out = sh("pgrep -f train_predictor.py >/dev/null && echo RUNNING || echo NOT_RUNNING")
    print("进程:", out.strip())


if __name__ == "__main__":
    # colab-cli exec -f 会把 '-f'/文件路径残留在 VM 的 sys.argv 中，只认合法模式字
    modes = [a for a in sys.argv[1:] if a in ("launch", "poll")]
    mode = modes[0] if modes else "launch"
    {"launch": launch, "poll": poll}[mode]()
