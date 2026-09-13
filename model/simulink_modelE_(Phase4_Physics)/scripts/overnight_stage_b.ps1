# overnight_stage_b.ps1 - Stage B formal fleet driver (Windows)
# AMENDMENT_03/04 two-phase workflow: Phase S (allocator) + Phase R (simulation)
# 3 workers, 16GB RAM machine.  Run from PowerShell as admin.

$ErrorActionPreference = "Continue"
$MODELE = "E:\User\Tommy\Desktop\hybrid-ship-ems\model\simulink_modelE_(Phase4_Physics)"
$MATLAB = "E:\Program Files\MATLAB\R2026a\bin\matlab.exe"
$RES = Join-Path $MODELE "results\dataset_v3\formal"
$IID_DIR = Join-Path $MODELE "datasets_v3\formal\iid"
$OOD_DIR = Join-Path $MODELE "datasets_v3\formal\ood"
$LOG = Join-Path $MODELE "overnight_stage_b.log"
$TOTAL_VOYAGES = 1552   # (576 IID + 200 OOD) x 2 passes

function Log($msg) {
    $line = "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] $msg"
    $line | Tee-Object -FilePath $Log -Append
    Write-Host $line
}

function Get-LedgerCount($pass, $key) {
    $f = Join-Path $RES "${pass}_${key}_ledger.json"
    if (-not (Test-Path $f)) { return -1 }
    try {
        $d = Get-Content $f -Raw | ConvertFrom-Json
        return $d.voyages.Count
    } catch { return -1 }
}

function Get-LatestTaskCount {
    $latest = Get-ChildItem "$RES\pass*_stage_b_*_a*.log" -EA SilentlyContinue |
        Sort-Object LastWriteTime -Desc | Select-Object -First 1
    if ($latest) {
        return (Select-String -Path $latest.FullName -Pattern "task.*done" -EA SilentlyContinue).Count
    }
    return 0
}

function Start-StageSegment($label, $mode, $expect) {
    # Skip if ledger already exists with correct count
    $count = Get-LedgerCount $label $mode.Replace("stage_b_","")
    if ($count -eq $expect) {
        Log "SKIP $label $mode (valid $expect-voyage ledger - resume)"
        return
    }
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        Log "START $label $mode (attempt $attempt/3, FLEET_WORKERS=3)"
        $env:FLEET_MODE = $mode
        $env:FLEET_PARALLEL = "on"
        $env:FLEET_WORKERS = "3"
        $segLog = Join-Path $RES "${label}_${mode}_a${attempt}.log"
        $proc = Start-Process -FilePath $MATLAB `
            -ArgumentList '-wait', '-batch', "eval(fileread('scripts/73_generate_fleet_v3.m'))" `
            -WorkingDirectory $MODELE -NoNewWindow -PassThru `
            -RedirectStandardOutput $segLog -RedirectStandardError "$segLog.err"
        $proc.WaitForExit()
        $rc = $proc.ExitCode
        Log "END $label $mode exit=$rc"
        if ($rc -eq 0) {
            $key = $mode.Replace("stage_b_","")
            $src = Join-Path $MODELE "datasets_v3\formal\$key\_${key}_ledger.json"
            if (Test-Path $src) {
                Copy-Item $src (Join-Path $RES "${label}_${key}_ledger.json") -Force
            }
            return
        }
        Log "$label $mode crashed (rc=$rc) - cleaning, retry in 20s"
        Get-Process | Where-Object { $_.Name -match "^MATLABWorker|^matlab$" } |
            Stop-Process -Force -EA SilentlyContinue
        Start-Sleep -Seconds 20
    }
    Log "PASS FAIL: $label $mode failed 3 attempts"
    New-Item -ItemType Force -Path (Join-Path $RES "STAGE_B_FAILED") | Out-Null
    exit 1
}

# ---- main ----
Log "STAGE B START on PC (776 voyages x 2 passes, 3 workers, 16GB RAM)"

# Disable sleep
powercfg /change standby-timeout-ac 0
powercfg /change hibernate-timeout-ac 0

try {
    # Phase S: allocator runs inside gen_iid/gen_ood on first invocation
    # (plan file is auto-created if missing, frozen and reused by pass2)

    Start-StageSegment "pass1" "stage_b_iid" 576
    Start-StageSegment "pass1" "stage_b_ood" 200
    Start-StageSegment "pass2" "stage_b_iid" 576
    Start-StageSegment "pass2" "stage_b_ood" 200

    Log "two-pass determinism compare"
    $p1i = Get-LedgerCount "pass1" "iid"
    $p2i = Get-LedgerCount "pass2" "iid"
    $p1o = Get-LedgerCount "pass1" "ood"
    $p2o = Get-LedgerCount "pass2" "ood"
    Log "pass1 IID=$p1i pass2 IID=$p2i pass1 OOD=$p1o pass2 OOD=$p2o"

    # Python determinism check
    $cmpScript = Join-Path $MODELE "scripts\compare_two_run.py"
    if (Test-Path $cmpScript) {
        $pyOut = & python $cmpScript 2>&1
        $pyOut | ForEach-Object { Log "compare: $_" }
        if ($LASTEXITCODE -ne 0) {
            Log "DETERMINISM FAIL"
            New-Item -ItemType Force -Path (Join-Path $RES "STAGE_B_FAILED") | Out-Null
            exit 1
        }
    }

    New-Item -ItemType Force -Path (Join-Path $RES "STAGE_B_TWO_RUN_DONE") | Out-Null
    Log "STAGE B COMPLETE - two-run determinism verified"
}
finally {
    powercfg /change standby-timeout-ac 30
    powercfg /change hibernate-timeout-ac 60
    Log "powercfg restored"
}
