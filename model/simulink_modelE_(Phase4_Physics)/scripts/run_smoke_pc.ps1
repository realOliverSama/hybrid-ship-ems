# run_smoke_pc.ps1 - one-off serial smoke on the PC desktop session
# (diagnostic for the Stage-B FAILED_GENERATION root cause).
$MODELE = "E:\User\Tommy\Desktop\hybrid-ship-ems\model\simulink_modelE_(Phase4_Physics)"
$MATLAB = "E:\Program Files\MATLAB\R2026a\bin\matlab.exe"
Set-Location $MODELE
$env:FLEET_MODE = "smoke"
$env:FLEET_PARALLEL = "on"
$env:FLEET_WORKERS = "4"
& $MATLAB -wait -batch "eval(fileread('scripts/73_generate_fleet_v3.m'))" > "$MODELE\results\dataset_v3\qualification_smoke\smoke_pc.log" 2> "$MODELE\results\dataset_v3\qualification_smoke\smoke_pc.err"
Write-Output ("SMOKE_EXIT=" + $LASTEXITCODE)
