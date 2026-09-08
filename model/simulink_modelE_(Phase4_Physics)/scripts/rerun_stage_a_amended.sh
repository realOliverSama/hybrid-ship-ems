#!/bin/bash
# rerun_stage_a_amended.sh — T432_ACTIVATION_AMENDMENT_02 requalification driver.
#
# Final Ruling 2026-09-08 §4: the original Stage-A PASS does not qualify the
# amended producer schema (v1.1.0, +9 H2 truth-stream columns). This driver
# reruns the frozen 92-voyage Stage-A plan under the amended schema with the
# required two-pass determinism check, then drops a marker.
#
# Resilience (user directive 2026-09-08: auto-restart on crash, max speed):
#   - per-pass retry up to 3 attempts (a crashed pass is re-run in full; the
#     generation is overwrite-idempotent) with stale parallelserver cleanup;
#   - pass-level resume: if pass1 archive exists and holds 92 voyages, pass1
#     is skipped so a crash in pass2 costs only pass2;
#   - memory pre-check before each pass (warn-and-wait while low);
#   - FLEET_WORKERS=3 (memory-bound cap is 3 by default; 4 sanctioned by the
#     user unless repeated crashes — then fall back to 3).
#
# Preconditions (checked here, enforced by the requalification sequence):
#   schema v1.1.0 on disk + validators PASS + full regression PASS
#   + old Stage-A corpus AND old manifest archives quarantined (caller action;
#     stale pass1 archives would defeat the resume logic).
#
# Launch: caffeinate -is bash scripts/rerun_stage_a_amended.sh > stage_a_rerun.log 2>&1 &
set -u
MODELE="/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelE_(Phase4_Physics)"
MATLAB="/Applications/MATLAB_R2026a.app/bin/matlab"
RES="$MODELE/results/dataset_v3/qualification"
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

pass1_done () {  # 92-voyage pass1 archive present?
    [ -f "$RES/stage_a_manifest_pass1.json" ] || return 1
    python3 - "$RES/stage_a_manifest_pass1.json" <<'EOF'
import json, sys
try:
    m = json.load(open(sys.argv[1]))
    n = len(m['iid']['voyages']) + len(m['ood']['voyages'])
    sys.exit(0 if n == 92 else 1)
except Exception:
    sys.exit(1)
EOF
}

run_pass () {  # $1=pass label
    local label="$1"
    if [ "$label" = "pass1" ] && pass1_done; then
        echo "[$(date -u +%FT%TZ)] SKIP pass1 (valid 92-voyage archive exists — resume)"
        return 0
    fi
    local attempt=1
    while [ $attempt -le 3 ]; do
        mem_wait
        echo "[$(date -u +%FT%TZ)] START $label stage_a (attempt $attempt/3, FLEET_WORKERS=3)"
        FLEET_MODE=stage_a FLEET_PARALLEL=off FLEET_WORKERS=3 "$MATLAB" -batch \
            "eval(fileread('scripts/73_generate_fleet_v3.m'))" \
            > "$RES/${label}_stage_a_a${attempt}.log" 2>&1 &
        local mpid=$!
        # hang watchdog: a healthy pass finishes well under 100 min; the
        # nanomsg assertion seen 2026-09-08 can leave parpool alive but
        # dead-connected (zero tasks done, rc never delivered) - kill it.
        local elapsed=0 rc=0
        while kill -0 $mpid 2>/dev/null; do
            sleep 30; elapsed=$((elapsed+30))
            if [ $elapsed -ge 9000 ]; then
                echo "[$(date -u +%FT%TZ)] $label attempt $attempt HUNG ${elapsed}s — killing"
                pkill -9 -f "MATLAB -parallelserver" 2>/dev/null
                kill -9 $mpid 2>/dev/null
                rc=124
                break
            fi
        done
        kill -0 $mpid 2>/dev/null || true
        wait $mpid 2>/dev/null || rc=$( [ $rc -ne 0 ] && echo $rc || echo $? )
        echo "[$(date -u +%FT%TZ)] END $label stage_a exit=$rc"
        if [ $rc -eq 0 ]; then
            return 0
        fi
        echo "[$(date -u +%FT%TZ)] $label crashed (rc=$rc) — cleaning stale parallel servers, retry in 20s"
        pkill -f "MATLAB -parallelserver" 2>/dev/null
        sleep 20
        attempt=$((attempt+1))
    done
    echo "[$(date -u +%FT%TZ)] STAGE-A RERUN FAIL: $label failed 3 attempts (see $RES/${label}_stage_a_a*.log)"
    touch "$RES/STAGE_A_RERUN_FAILED"
    exit 1
}

echo "[$(date -u +%FT%TZ)] STAGE-A AMENDED RERUN START (92 voyages x 2 passes, FLEET_WORKERS=3)"
run_pass pass1
cp "$RES/stage_a_manifest.json" "$RES/stage_a_manifest_pass1.json"
run_pass pass2

echo "[$(date -u +%FT%TZ)] two-pass determinism compare (92/92 payload hash)"
python3 - <<'EOF'
import json, sys
RES = 'results/dataset_v3/qualification'
def rows(p):
    m = json.load(open(f'{RES}/stage_a_manifest{p}.json'))
    out = {}
    for seg in ('iid', 'ood'):
        for r in m[seg]['voyages']:
            out[r['voyage_id']] = r['payload_hash']
    return out
a, b = rows('_pass1'), rows('')
assert len(a) == 92 and len(b) == 92, f'count {len(a)}/{len(b)} != 92'
diff = [v for v in a if a[v] != b.get(v)]
print(f'stage_a: 92 voyages, payload-hash identical {92-len(diff)}/92')
if diff:
    print('DETERMINISM FAIL:', diff[:10]); sys.exit(1)
print('DETERMINISM PASS: payload hashes identical across passes')
EOF
rc=$?
if [ $rc -ne 0 ]; then touch "$RES/STAGE_A_RERUN_FAILED"; exit 1; fi
touch "$RES/STAGE_A_RERUN_DONE"
echo "[$(date -u +%FT%TZ)] STAGE-A AMENDED RERUN COMPLETE — C/D FORMAL Gate next"
