#!/bin/bash
# overnight_stage_b.sh — T-019 Stage B formal fleet overnight driver
# (T432_ACTIVATION_AMENDMENT_01 + D1-D5 Final Ruling execution sequence).
#
# Runs the two-pass deterministic formal generation:
#   pass 1: stage_b_iid (576) then stage_b_ood (200)
#   pass 2: identical rerun (payload-hash determinism, FR-537/AC-438)
# then verifies payload-hash identity across passes and drops a marker.
#
# Resilience (user directive 2026-09-08: auto-restart on crash, max speed):
#   - per-segment retry up to 3 attempts (crashed segment re-run in full;
#     generation is overwrite-idempotent) with stale parallelserver cleanup;
#   - segment-level resume: pass1_{iid,ood} ledgers with the expected voyage
#     counts are treated as "segment complete" and skipped on driver restart,
#     so a late crash costs only the remaining segments;
#   - memory pre-check before each segment (warn-and-wait while low);
#   - FLEET_WORKERS=3 (sanctioned by user 2026-09-08 unless repeated crashes).
#
# Launch: nohup caffeinate -is bash overnight_stage_b.sh > overnight_stage_b.log 2>&1 &
# The MATLAB batch processes are immune to agent-subscription switches.
set -u
MODELE="/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)"
MATLAB="/Applications/MATLAB_R2026a.app/bin/matlab"
RES="$MODELE/results/dataset_v3/formal"
cd "$MODELE" || exit 1
mkdir -p "$RES"

mem_wait () {  # warn-and-wait while free memory is critically low
    local i=0
    while [ $i -lt 10 ]; do
        free_pct=$(memory_pressure -Q 2>/dev/null | grep -o '[0-9]*%' | head -1 | tr -d '%')
        if [ -n "$free_pct" ] && [ "$free_pct" -ge 15 ]; then
            echo "[$(date -u +%FT%TZ)] memory free ${free_pct}% — OK"
            return 0
        fi
        echo "[$(date -u +%FT%TZ)] memory free ${free_pct:-?}% <15% — waiting 60s"
        sleep 60
        i=$((i+1))
    done
    echo "[$(date -u +%FT%TZ)] memory still low — proceeding anyway (user directive: max speed)"
}

segment_done () {  # $1=key (iid|ood)  $2=expected voyage count
    local ledger="$RES/pass1_$1_ledger.json"
    [ -f "$ledger" ] || return 1
    python3 - "$ledger" "$2" <<'EOF'
import json, sys
try:
    d = json.load(open(sys.argv[1]))
    sys.exit(0 if len(d['voyages']) == int(sys.argv[2]) else 1)
except Exception:
    sys.exit(1)
EOF
}

run_pass () {  # $1=pass label  $2=mode  $3=expected count
    local label="$1" mode="$2" expect="$3"
    if [ "$label" = "pass1" ] && segment_done "$mode" "$expect"; then
        echo "[$(date -u +%FT%TZ)] SKIP pass1 $mode (valid $expect-voyage ledger — resume)"
        return 0
    fi
    local attempt=1
    while [ $attempt -le 3 ]; do
        mem_wait
        echo "[$(date -u +%FT%TZ)] START $label $mode (attempt $attempt/3, FLEET_WORKERS=3)"
        FLEET_MODE="$mode" FLEET_PARALLEL=on FLEET_WORKERS=3 "$MATLAB" -batch \
            "eval(fileread('scripts/73_generate_fleet_v3.m'))" \
            > "$RES/${1}_${2}_a${attempt}.log" 2>&1 &
        local mpid=$!
        # hang watchdog: healthy segments finish well under 240 min at 3
        # workers; the nanomsg assertion (2026-09-08) can leave parpool
        # alive but dead-connected - kill and retry instead of hanging.
        local elapsed=0 rc=0
        while kill -0 $mpid 2>/dev/null; do
            sleep 30; elapsed=$((elapsed+30))
            if [ $elapsed -ge 18000 ]; then
                echo "[$(date -u +%FT%TZ)] $label $mode attempt $attempt HUNG ${elapsed}s — killing"
                pkill -9 -f "MATLAB -parallelserver" 2>/dev/null
                kill -9 $mpid 2>/dev/null
                rc=124
                break
            fi
        done
        kill -0 $mpid 2>/dev/null || true
        wait $mpid 2>/dev/null || true
        rc=${rc:-$?}
        echo "[$(date -u +%FT%TZ)] END $label $mode exit=$rc"
        if [ $rc -eq 0 ]; then
            # archive the segment ledger (pass 2 overwrites in-place)
            cp "$MODELE/datasets_v3/formal/${mode##stage_b_}/_${mode##stage_b_}_ledger.json" \
               "$RES/${1}_${mode##stage_b_}_ledger.json" 2>/dev/null
            return 0
        fi
        echo "[$(date -u +%FT%TZ)] $label $mode crashed (rc=$rc) — cleaning stale parallel servers, retry in 20s"
        pkill -f "MATLAB -parallelserver" 2>/dev/null
        sleep 20
        attempt=$((attempt+1))
    done
    echo "[$(date -u +%FT%TZ)] PASS FAIL: $label $mode failed 3 attempts (see $RES/${1}_${2}_a*.log)"
    touch "$RES/STAGE_B_FAILED"
    exit 1
}

echo "[$(date -u +%FT%TZ)] STAGE B START (776 voyages, two passes, FLEET_WORKERS=3)"
run_pass pass1 stage_b_iid 576
run_pass pass1 stage_b_ood 200
run_pass pass2 stage_b_iid 576
run_pass pass2 stage_b_ood 200

echo "[$(date -u +%FT%TZ)] two-pass determinism compare"
python3 - <<'EOF'
import json, sys
RES = 'results/dataset_v3/formal'
def rows(p, key):
    d = json.load(open(f'{RES}/{p}_{key}_ledger.json'))
    return {r['voyage_id']: r['payload_hash'] for r in d['voyages']}
bad = []
for key, n in (('iid', 576), ('ood', 200)):
    a, b = rows('pass1', key), rows('pass2', key)
    assert len(a) == n and len(b) == n, f'{key} count {len(a)}/{len(b)} != {n}'
    diff = [v for v in a if a[v] != b.get(v)]
    print(f'{key}: {n} voyages, payload-hash identical {n-len(diff)}/{n}')
    bad += [f'{key}:{v}' for v in diff]
if bad:
    print('DETERMINISM FAIL:', bad[:10]); sys.exit(1)
print('DETERMINISM PASS: payload hashes identical across passes')
EOF
rc=$?
if [ $rc -ne 0 ]; then touch "$RES/STAGE_B_FAILED"; exit 1; fi
touch "$RES/STAGE_B_TWO_RUN_DONE"
echo "[$(date -u +%FT%TZ)] STAGE B COMPLETE — T-020 audit freeze may proceed"
