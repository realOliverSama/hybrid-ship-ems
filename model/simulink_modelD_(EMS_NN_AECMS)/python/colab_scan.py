# -*- coding: utf-8 -*-
"""colab_scan.py — T-305 Colab VM 训练启动/轮询脚本（在 Colab VM 上执行）

经 colab-cli 使用：
    colab new -s t305 --gpu T4                      # 开 VM（首次弹浏览器授权）
    colab exec -s t305 -f python/colab_scan.py      # 启动模式：clone + 装依赖 + 后台训练
    colab exec -s t305 -f python/colab_scan.py -- poll   # 轮询模式：看进度
    colab download -s t305 <远程路径> <本地路径>      # 完成后取产物（在 M1 上执行）

启动模式做四件事：clone 仓库 → 确认 TF/GPU → 后台启动四配置训练（nohup）→ 打印日志路径。
轮询模式：tail 训练日志 + CSV 状态。
"""
import os
import subprocess
import sys

REPO = "https://github.com/realOliverSama/hybrid-ship-ems.git"
BRANCH = "models-research"
WORK = "/content/hybrid-ship-ems/model/simulink_modelD_(EMS_NN_AECMS)"
CMD = ("python python/train_predictor.py --arch tcn --dataset v2 --quantiles "
       "--no-plots --batch 512 --epochs 100")


def sh(cmd, cwd=None):
    r = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True)
    out = (r.stdout or "") + (r.stderr or "")
    return r.returncode, out


def launch():
    if not os.path.isdir("/content/hybrid-ship-ems"):
        code, out = sh("git clone -b %s %s /content/hybrid-ship-ems" % (BRANCH, REPO))
        print(code, out[-500:])
    os.chdir(WORK)
    code, out = sh("git pull origin %s" % BRANCH)
    print("git pull:", code, out[-200:])
    import tensorflow as tf
    print("TF:", tf.__version__)
    print("GPU:", tf.config.list_physical_devices("GPU"))
    # 依赖（Colab VM 预装 TF/scipy/pandas，缺啥补啥）
    sh("pip install -q scipy pandas 2>&1 | tail -1")
    # 后台启动训练（nohup 防连接断开）
    sh("rm -f t305_scan.log")
    code, out = sh("nohup %s > t305_scan.log 2>&1 & echo $!" % CMD)
    print("训练已后台启动 PID:", out.strip())
    print("轮询: colab exec -s t305 -f python/colab_scan.py -- poll")


def poll():
    os.chdir(WORK)
    code, out = sh("tail -n 20 t305_scan.log")
    print(out or "(日志为空)")
    code, out = sh("cut -c1-150 results/ecms_phase25/training_log_v2.csv 2>/dev/null || true")
    print("--- CSV ---")
    print(out or "(尚无完成行)")
    code, out = sh("pgrep -f train_predictor.py >/dev/null && echo RUNNING || echo NOT_RUNNING")
    print("进程:", out.strip())


if __name__ == "__main__":
    # colab-cli exec -f 会把 '-f'/文件路径残留在 VM 的 sys.argv 中，只认合法模式字
    modes = [a for a in sys.argv[1:] if a in ("launch", "poll")]
    mode = modes[0] if modes else "launch"
    {"launch": launch, "poll": poll}[mode]()
