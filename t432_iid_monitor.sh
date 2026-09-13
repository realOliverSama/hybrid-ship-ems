#!/bin/bash
# T-432 Stage B IID progress monitor (macOS) — mirrors the Windows driver console UI
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BASE="$REPO_ROOT/model/simulink_modelE_(Phase4_Physics)"
FORMAL="$BASE/results/dataset_v3/formal"
L1="$FORMAL/pass1_stage_b_iid.log"
L2="$FORMAL/pass2_stage_b_iid.log"
IID="$BASE/datasets_v3/formal/iid"
DMRDIR="${TMPDIR:-/tmp}"

bar() { # bar <done> <total> [width]
  local done=$1 total=$2 w=${3:-22} fill i out=''
  if [ "$total" -le 0 ]; then fill=0; else fill=$(( done * w / total )); fi
  for ((i=0;i<fill;i++)); do out+='#'; done
  for ((i=fill;i<w;i++)); do out+='.'; done
  printf '[%s]' "$out"
}
fmt_hm() { printf '%02d:%02d' $(($1/3600)) $((($1%3600)/60)); }
count_log() { grep -c 'task .* done' "$1" 2>/dev/null || echo 0; }
count_mat() { ls "$1"/*.mat 2>/dev/null | wc -l | tr -d ' '; }
vol_free_gb() { df -g "$1" 2>/dev/null | awk 'NR==2{print $4}'; }

matlab_pid() { pgrep -f 'MATLAB_maca[64]' | head -1; }
start_epoch_of() { # epoch birth time of a pid's process
  ps -p "$1" -o lstart= 2>/dev/null | xargs -I{} date -j -f '%a %b %d %H:%M:%S %Y' '{}' '+%s' 2>/dev/null
}

export TERM=${TERM:-ansi}

while true; do
  clear
  printf '\033]0;T-432 IID Monitor\007'
  NOW=$(date '+%Y-%m-%d %H:%M:%S')
  echo "==== T-432 Stage B IID PROGRESS  $NOW ===="

  MPID=$(matlab_pid)
  t1=$(count_log "$L1"); m1=$(count_mat "$IID")
  # pass2-written mats only: files newer than the birth of the current pass2 log
  m2=0
  if [ -f "$L2" ]; then
    REF="${DMRDIR%/}/.t432_pass2_ref"
    touch -t "$(date -r "$(stat -f '%B' "$L2")" +%Y%m%d%H%M.%S)" "$REF" 2>/dev/null
    m2=$(find "$IID" -name 'FLTA*.mat' -newer "$REF" 2>/dev/null | wc -l | tr -d ' ')
  fi
  VERDICT1=$(grep -c 'FLEETV3_VERDICT=DONE' "$L1" 2>/dev/null)
  LEDGER1=0; [ -f "$IID/_iid_ledger.json" ] && LEDGER1=1
  P1_CONTRIB=0; [ "$VERDICT1" -ge 1 ] && P1_CONTRIB=576

  # --- Phase 1 ---
  if [ "$VERDICT1" -ge 1 ]; then
    echo "Phase 1 (pass1): $(bar 576 576) $m1/576  task $t1/144  DONE  ledger=$([ $LEDGER1 -eq 1 ] && echo yes || echo MISSING)"
    P1_STATE=done
  elif [ "$t1" -gt 0 ] && [ -n "$MPID" ]; then
    SEP=$(start_epoch_of "$MPID"); [ -z "$SEP" ] && SEP=$(stat -f '%B' "$L1" 2>/dev/null)
    [ -z "$SEP" ] && SEP=$(date +%s)
    EL=$(( $(date +%s) - SEP ))
    ETA=$(( EL * (144 - t1) / t1 ))
    echo "Phase 1 (pass1): $(bar $t1 144) $m1/576  task $t1/144  elapsed $(fmt_hm $EL) ETA ~$(fmt_hm $ETA)"
    P1_STATE=running
  elif [ -n "$MPID" ]; then
    echo "Phase 1 (pass1): $(bar 0 144) $m1/576  task $t1/144  elapsed --:-- ETA ~--:-- (warming up)"
    P1_STATE=running
  else
    echo "Phase 1 (pass1): $(bar $t1 144) $m1/576  task $t1/144  MATLAB EXITED (no DONE verdict)"
    P1_STATE=crashed
  fi

  # --- Phase 2 ---
  if [ -f "$L2" ]; then
    t2=$(count_log "$L2")
    VERDICT2=$(grep -c 'FLEETV3_VERDICT=DONE' "$L2" 2>/dev/null)
    if [ "$VERDICT2" -ge 1 ]; then
      echo "Phase 2 (pass2): $(bar 576 576) $m2/576  task $t2/144  DONE"
      P2_STATE=done
    elif [ "$t2" -gt 0 ]; then
      echo "Phase 2 (pass2): $(bar $t2 144) $m2/576  task $t2/144  running"
      P2_STATE=running
    else
      echo "Phase 2 (pass2): $(bar 0 144) $m2/576  task $t2/144  starting"
      P2_STATE=running
    fi
  else
    echo "Phase 2 (pass2): waiting (decision after pass1 review)"
    P2_STATE=waiting
  fi

  echo "Total: $((P1_CONTRIB + m2))/1152 voyages (2 x 576)"
  if [ -n "$MPID" ]; then echo "MATLAB: RUNNING (pid $MPID)"; else echo "MATLAB: EXITED"; fi
  echo "tmp free: $(vol_free_gb "${DMRDIR%/TemporaryItems*}" 2>/dev/null || vol_free_gb /var/folders) GB | Data vol free: $(vol_free_gb /System/Volumes/Data) GB"

  # --- STATE ---
  ST=''
  case "$P1_STATE" in
    running) ST='running (pass1)' ;;
    done)    if [ "$P2_STATE" = "waiting" ]; then ST='pass1 done, awaiting pass2 decision'; elif [ "$P2_STATE" = "running" ]; then ST='running (pass2)'; else ST='ALL DONE'; fi ;;
    crashed) ST='PASS1 CRASHED (no ledger; retry pending)' ;;
  esac
  echo ""
  echo "STATE: $ST  (refresh 10s, Ctrl+C to quit)"
  sleep 10
done
