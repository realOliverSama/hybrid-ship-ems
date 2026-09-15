#!/bin/bash
# T-432 IID pass2 crash-recovery watchdog:
#   MATLAB exited + no DONE verdict  -> preserve crash log, clean .dmr temp, relaunch pass2 (<=5 retries)
#   MATLAB exited + DONE verdict     -> archive pass2_iid_ledger.json, clean .dmr, stop caffeinate, retire
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="$REPO_ROOT/model/simulink_modelE_(Phase4_Physics)"
FORMAL="$BASE/results/dataset_v3/formal"
LOG="$FORMAL/pass2_stage_b_iid.log"
IID="$BASE/datasets_v3/formal/iid"
MATLAB=/Applications/MATLAB_R2026a.app/bin/matlab
MAX_RETRY=5
N=0
# singleton lock: only one watchdog may run (double relaunch = concurrent writers)
LOCK=/tmp/t432_iid_watchdog.lock
if ! mkdir "$LOCK" 2>/dev/null; then
  echo "WATCHDOG: another instance holds $LOCK — exiting"
  exit 0
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
while true; do
  sleep 300
  pgrep -f 'MATLAB_maca[64]' >/dev/null 2>&1 && continue
  if grep -q 'FLEETV3_VERDICT=DONE' "$LOG" 2>/dev/null; then
    cp "$IID/_iid_ledger.json" "$FORMAL/pass2_iid_ledger.json" 2>/dev/null
    rm -rf "${TMPDIR}"*.dmr 2>/dev/null
    pkill -f 'caffeina[te]' 2>/dev/null
    echo "WATCHDOG: PASS2 DONE at $(date '+%m-%d %H:%M:%S'); ledger archived; dmr cleaned; caffeinate stopped"
    break
  fi
  N=$((N+1))
  if [ "$N" -gt "$MAX_RETRY" ]; then
    echo "WATCHDOG: retry budget exhausted ($N) at $(date '+%m-%d %H:%M:%S'); MANUAL INTERVENTION NEEDED"
    break
  fi
  mv "$LOG" "$LOG.crash_$N" 2>/dev/null
  rm -rf "${TMPDIR}"*.dmr 2>/dev/null
  pgrep -f 'caffeina[te]' >/dev/null 2>&1 || nohup caffeinate -is >/dev/null 2>&1 &
  cd "$BASE" || exit 1
  FLEET_MODE=stage_b_iid FLEET_PARALLEL=on FLEET_WORKERS=2 nohup "$MATLAB" -batch "eval(fileread('scripts/73_generate_fleet_v3.m'))" > "$LOG" 2>&1 &
  echo "WATCHDOG: crash #$N detected at $(date '+%m-%d %H:%M:%S'); dmr cleaned; pass2 relaunched (log reset)"
done
