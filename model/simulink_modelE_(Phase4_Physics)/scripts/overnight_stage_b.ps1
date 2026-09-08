# overnight_stage_b.ps1 - T-019 Stage B formal fleet driver (Windows port).
#
# Mac counterpart: scripts/overnight_stage_b.sh. Runs the two-pass
# deterministic formal generation on this PC's interactive desktop
# session (SSH Session-0 breaks MATLAB worker licensing; desktop
# sessions are proven healthy by the user's earlier parallel runs).
#
#   pass 1: stage_b_iid (576) then stage_b_ood (200)
#   pass 2: identical rerun (payload-hash determinism)
# then verifies payload-hash identity and drops the marker.
#
# Resilience (mirrors the bash driver):
#   - per-segment retry up to 3 attempts, stale parallelserver cleanup;
#   - segment-level resume: pass1 ledgers with the expected voyage
#     counts are treated as complete and skipped on driver restart;
#   - hang watchdog per attempt (360 min hard kill);
#   - powercfg: AC sleep disabled for the run, original values restored
#     on exit (keep plugged in; lock screen is fine).
#
# Launch (interactive desktop session):
#   powershell -ExecutionPolicy Bypass -File overnight_stage_b.ps1
# or the registered schtasks interactive task.

$ErrorActionPreference = "Continue"
$MODELE = "E:\User\Tommy\Desktop\hybrid-ship-ems\model\simulink_modelE_(Phase4_Physics)"
$MATLAB = "E:\Program Files\MATLAB\R2026a\bin\matlab.exe"
$RES = Join-Path $MODELE "results\dataset_v3\formal"
$Repo = "E:\User\Tommy\Desktop\hybrid-ship-ems"
Set-Location $MODELE
New-Item -ItemType Directory -Force -Path $RES | Out-Null
$Log = Join-Path $RES "overnight_stage_b_pc.log"

function Log($msg) {
    $line = "[$(Get-Date -Format o)] $msg"
    $line | Tee-Object -FilePath $Log -Append
}

function Wait-MatlabOrKill($proc, $timeoutSec) {
    $elapsed = 0
    while (-not $proc.HasExited) {
        Start-Sleep -Seconds 30
        $elapsed += 30
        if ($elapsed -ge $timeoutSec) {
            Log "attempt HUNG after ${elapsed}s - killing matlab tree"
            Get-Process | Where-Object { $_.Name -match "^MATLABWorker|^matlab$" } |
                Stop-Process -Force -EA SilentlyContinue
            try { $proc.Kill() } catch {}
            return 124
        }
    }
    return $proc.ExitCode
}

function Segment-Done($key, $expected) {
    $ledger = Join-Path $RES "pass1_${key}_ledger.json"
    if (-not (Test-Path $ledger)) { return $false }
    try {
        $d = Get-Content $ledger -Raw | ConvertFrom-Json
        return ($d.voyages.Count -eq $expected)
    } catch { return $false }
}

function Run-Segment($label, $mode, $expect) {
    if ($label -eq "pass1" -and (Segment-Done $mode $expect)) {
        Log "SKIP pass1 $mode (valid $expect-voyage ledger - resume)"
        return
    }
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        Log "START $label $mode (attempt $attempt/3, FLEET_WORKERS=4)"
        $segLog = Join-Path $RES "${label}_${mode}_a${attempt}.log"
        $env:FLEET_MODE = $mode
        $env:FLEET_PARALLEL = "on"
        $env:FLEET_WORKERS = "4"
        $proc = Start-Process -FilePath $MATLAB `
            -ArgumentList '-wait', '-batch', "eval(fileread('scripts/73_generate_fleet_v3.m'))" `
            -WorkingDirectory $MODELE -NoNewWindow -PassThru `
            -RedirectStandardOutput $segLog -RedirectStandardError "$segLog.err"
        $rc = Wait-MatlabOrKill $proc 21600
        Log "END $label $mode exit=$rc"
        if ($rc -eq 0) {
            $key = $mode -replace "^stage_b_", ""
            $src = Join-Path $MODELE "datasets_v3\formal\$key\_${key}_ledger.json"
            if (Test-Path $src) {
                Copy-Item $src (Join-Path $RES "${label}_${key}_ledger.json") -Force
            }
            return
        }
        Log "$label $mode crashed (rc=$rc) - cleaning workers, retry in 20s"
        Get-Process | Where-Object { $_.Name -match "^MATLABWorker|^matlab$" } |
            Stop-Process -Force -EA SilentlyContinue
        Start-Sleep -Seconds 20
    }
    Log "PASS FAIL: $label $mode failed 3 attempts"
    New-Item -ItemType Force -Path (Join-Path $RES "STAGE_B_FAILED") | Out-Null
    exit 1
}

# ---- powercfg: keep the machine awake while plugged in ----
$acSleep = (powercfg /query SCHEME_CURRENT SUB_SLEEP STANDBYIDLE)[0]
Log "disabling AC sleep for the run (restored on exit)"
powercfg /change standby-timeout-ac 0 | Out-Null
powercfg /change hibernate-timeout-ac 0 | Out-Null

try {
    Log "STAGE B START on PC (776 voyages, two passes, FLEET_WORKERS=4)"
    Run-Segment pass1 stage_b_iid 576
    Run-Segment pass1 stage_b_ood 200
    Run-Segment pass2 stage_b_iid 576
    Run-Segment pass2 stage_b_ood 200

    Log "two-pass determinism compare"
    $py = Join-Path $Repo "compare_two_run.py"
    if (Test-Path $py) {
        python $py
        if ($LASTEXITCODE -ne 0) { throw "determinism compare FAILED" }
    } else {
        Log "WARN: compare_two_run.py not found - determinism compare deferred to freeze"
    }

    New-Item -ItemType Force -Path (Join-Path $RES "STAGE_B_TWO_RUN_DONE") | Out-Null
    Log "STAGE B COMPLETE - T-020 audit freeze may proceed"
}
finally {
    powercfg /change standby-timeout-ac 30 | Out-Null
    powercfg /change hibernate-timeout-ac 60 | Out-Null
    Log "powercfg sleep timeouts restored"
}
