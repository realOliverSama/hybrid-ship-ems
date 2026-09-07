#!/bin/bash
# overnight_stage_b.sh — T-019 Stage B formal fleet overnight driver
# (T432_ACTIVATION_AMENDMENT_01 + D1-D5 Final Ruling execution sequence).
#
# Runs the two-pass deterministic formal generation:
#   pass 1: stage_b_iid (576) then stage_b_ood (200)
#   pass 2: identical rerun (payload-hash determinism, FR-537/AC-438)
# then verifies payload-hash identity across passes and drops a marker.
#
# Launch: nohup bash overnight_stage_b.sh > overnight_stage_b.log 2>&1 &
# The MATLAB batch processes are immune to agent-subscription switches.
set -u
MODELE="/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)"
MATLAB="/Applications/MATLAB_R2026a.app/bin/matlab"
RES="$MODELE/results/dataset_v3/formal"
cd "$MODELE" || exit 1
mkdir -p "$RES"

run_pass () {  # $1=pass label  $2=mode
    echo "[$(date -u +%FT%TZ)] START $1 $2"
    FLEET_MODE="$2" FLEET_PARALLEL=on "$MATLAB" -batch \
        "eval(fileread('scripts/73_generate_fleet_v3.m'))" \
        > "$RES/${1}_${2}.log" 2>&1
    local rc=$?
    echo "[$(date -u +%FT%TZ)] END $1 $2 exit=$rc"
    if [ $rc -ne 0 ]; then
        echo "[$(date -u +%FT%TZ)] PASS FAIL (see $RES/${1}_${2}.log)"
        touch "$RES/STAGE_B_FAILED"
        exit 1
    fi
    # archive the pass ledger (pass 2 overwrites in-place)
    cp "$MODELE/datasets_v3/formal/iid/_iid_ledger.json" \
       "$RES/${1}_iid_ledger.json" 2>/dev/null
    cp "$MODELE/datasets_v3/formal/ood/_ood_ledger.json" \
       "$RES/${1}_ood_ledger.json" 2>/dev/null
}

echo "[$(date -u +%FT%TZ)] STAGE B START (776 voyages, two passes, parallel 3 workers)"
run_pass pass1 stage_b_iid
run_pass pass1 stage_b_ood
run_pass pass2 stage_b_iid
run_pass pass2 stage_b_ood

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
