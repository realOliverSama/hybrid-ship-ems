#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""watchdog_fleet v3 — T-302 舰队批跑看门狗（不入库）

v3 新增：挂死检测。v2 盲区：MATLAB 进程活着但 parsim 池死锁（worker 崩溃后
teardown 死锁），p.wait() 永不返回。v3 在 MATLAB 存活期间每 60 s 检查剖面进度，
25 分钟无新增 -> taskkill 进程树 -> 自动重启续跑（断点续跑跳过已存剖面）。
其余逻辑同 v2：完成判定 = voyage_001..400 全存在 -> 全量空转 pass 合并 summary -> 退出。
"""
import io
import json
import os
import subprocess
import sys
import time

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

ROOT = r"E:\User\Tommy\Desktop\hybrid-ship-ems\model\simulink_modelD"
PROF_DIR = os.path.join(ROOT, "results", "ecms_phase25", "fleet_profiles")
SUMMARY = os.path.join(PROF_DIR, "summary_seed20260829.json")
LOG = os.path.join(ROOT, "results", "ecms_phase25", "fleet_gen_chunks.log")
MATLAB = r"E:\Program Files\MATLAB\R2026a\bin\matlab.exe"
TARGET = 400
MAX_RESTARTS = 40
HANG_TIMEOUT_S = 25 * 60
POLL_S = 60


def log(msg):
    line = "[watchdog-v3 %s] %s" % (time.strftime("%H:%M:%S"), msg)
    print(line, flush=True)
    with open(LOG, "a", encoding="utf-8", errors="replace") as lf:
        lf.write(line + "\n")


def profile_count():
    return len([f for f in os.listdir(PROF_DIR) if f.startswith("voyage_") and f.endswith(".mat")])


def profiles_done():
    missing = [i for i in range(1, TARGET + 1)
               if not os.path.exists(os.path.join(PROF_DIR, "voyage_%03d.mat" % i))]
    return (not missing), missing


def summary_total():
    if not os.path.exists(SUMMARY):
        return -1
    try:
        d = json.load(open(SUMMARY, encoding="utf-8"))
        m = d.get("meta", {})
        return m.get("n_ok", 0) + m.get("n_rejected", 0) + m.get("n_skip", 0)
    except Exception as e:
        log("summary parse warn: %s" % e)
        return -1


def kill_matlab_tree():
    subprocess.run(["taskkill", "/F", "/IM", "matlab.exe", "/T"],
                   capture_output=True, text=True)
    time.sleep(5)
    log("matlab tree killed (hang recovery)")


def main():
    log("WATCHDOG V3 START (hang timeout %ds)" % HANG_TIMEOUT_S)
    for attempt in range(1, MAX_RESTARTS + 1):
        ok, missing = profiles_done()
        if ok:
            log("profiles 1..400 all exist")
            break
        log("attempt %d: missing %d (e.g. %s), launching chunk runner"
            % (attempt, len(missing), missing[:3]))
        p = subprocess.Popen(
            [MATLAB, "-batch", "eval(fileread('results/ecms_phase25/run_chunks.m'))"],
            cwd=ROOT, creationflags=subprocess.CREATE_NEW_CONSOLE)
        # ---- 带挂死检测的等待 ----
        last_n = profile_count()
        last_progress = time.time()
        while True:
            rc = p.poll()
            if rc is not None:
                log("matlab exited rc=%s" % rc)
                break
            time.sleep(POLL_S)
            n = profile_count()
            if n > last_n:
                last_n = n
                last_progress = time.time()
            elif time.time() - last_progress > HANG_TIMEOUT_S:
                log("HANG detected: no profile progress for %.0f min (profiles=%d) -> kill tree"
                    % ((time.time() - last_progress) / 60, n))
                kill_matlab_tree()
                break
        ok, missing = profiles_done()
        log("after run: missing=%d" % len(missing))
        if ok:
            break
        time.sleep(20)
    else:
        log("GAVE UP after %d attempts — manual review needed" % MAX_RESTARTS)
        return

    if summary_total() < TARGET:
        log("consolidation pass: FLEET_START=1 FLEET_N=400 (all skip)")
        p = subprocess.Popen(
            [MATLAB, "-batch",
             "assignin('base','FLEET_START',1); assignin('base','FLEET_N',400); "
             "eval(fileread('scripts/61_generate_fleet_profiles_v2.m'))"],
            cwd=ROOT, creationflags=subprocess.CREATE_NEW_CONSOLE)
        p.wait()
        log("consolidation done, summary total=%s" % summary_total())
    log("WATCHDOG V3 COMPLETE")


if __name__ == "__main__":
    main()
