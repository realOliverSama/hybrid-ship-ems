%% ========================================================================
%  34_dp_convergence_check.m — ECMS Phase 0：DP 网格收敛检查（T-012，FR-011）
%  验证对象：S07 × tier2（中档 d25=+50%）× 连续口径 × P_min=400 kW
%  基准网格（dSOC=0.005, dP=20） vs 加密网格（dSOC=0.0025, dP=10）
%  判据：全程燃油（前向轨迹 fuel_total_kg）差 < 0.5%（FR-011）
%  产出：
%    results/ecms_phase0/convergence_S07.json   机器可读收敛证据（入库）
%  不通过 → error('ECMS:CONV', ...) 非零退出（按任务约束须 Escalate，
%   不得自行加密基准网格后继续）。
%  执行（数字开头脚本须 eval(fileread()) 方式运行）：
%    cd model/simulink_modelD && /Applications/MATLAB_R2026a.app/bin/matlab -batch \
%      "eval(fileread('scripts/34_dp_convergence_check.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

fprintf('\n[34] ===== ECMS Phase 0 — DP 网格收敛检查（T-012，FR-011） =====\n');
fprintf('[34] ROOT = %s\n', ROOT);

cfg  = Ecms_Phase0_Config();
fam  = load(fullfile(ROOT, 'results', 'ecms_phase0', 'bsfc', 'bsfc_family.mat'));
SCN  = 'S07_bridge_maneuver';
TIER = cfg.tier_default;          % 中档 d25=+50%
PMIN = 400;

prof = ecms.load_profile(ROOT, SCN);
assert(~prof.derate_event, 'ECMS:CONV', '%s 剖面含 derate_event，已作废（FR-006）', SCN);
assert(prof.energy_closure.pass, 'ECMS:CONV', '%s 剖面能量闭合未过', SCN);

md5str = @(s) sprintf('%02x', typecast(java.security.MessageDigest.getInstance('MD5').digest(uint8(s)), 'uint8'));
cfgHash = @(c) md5str(sprintf('mode=%s;Pmin=%.6g;R=%.6g;Cap=%.6g;Pl=%.6g;Cs=%.6g;dSOC=%.6g;dP=%.6g;S0=%.6g;band=%.6g;Pmax=%.6g;dt=%.6g;keq=%.6g;tier=%d', ...
    c.mode, c.P_min_stable_kW, c.R_gen_kWps, c.Bat_Cap_kWh, c.Bat_P_limit_kW, ...
    c.C_start_kg, c.dSOC, c.dP_kW, c.SOC0, c.SOC_terminal_band, c.P_max_kW, ...
    c.dt_s, c.k_eq_bsfc_gkWh, TIER));

%% ======================= 基准网格求解 =======================
cfgB = Ecms_Phase0_Config(struct('mode', 'continuous', 'P_min_stable_kW', PMIN));
probB = ecms.dp_problem(prof, cfgB, fam, TIER);
tB = tic; solB = ecms.dp_solve(probB); wallB = toc(tB);
assert(solB.feasible, 'ECMS:CONV', ...
    '基准网格 S07 tier%d cont pmin%d INFEASIBLE: %s', TIER, PMIN, solB.infeasible_reason);
chkB = ecms.dp_checks(probB, solB, prof, cfgB, TIER);
fprintf('[34] 基准网格  Nsoc=%d Np=%d | fuel=%.4f kg | wall=%.2f s | checks.pass=%d | saving=%.4f%%\n', ...
    probB.Nsoc, probB.Np, solB.fuel_total_kg, wallB, chkB.pass, chkB.saving_pct);

%% ======================= 加密网格求解 =======================
cfgF = Ecms_Phase0_Config(struct('mode', 'continuous', 'P_min_stable_kW', PMIN, ...
    'dSOC', cfg.convergence.dSOC, 'dP_kW', cfg.convergence.dP_kW));
probF = ecms.dp_problem(prof, cfgF, fam, TIER);
tF = tic; solF = ecms.dp_solve(probF); wallF = toc(tF);
assert(solF.feasible, 'ECMS:CONV', ...
    '加密网格 S07 tier%d cont pmin%d INFEASIBLE: %s', TIER, PMIN, solF.infeasible_reason);
chkF = ecms.dp_checks(probF, solF, prof, cfgF, TIER);
fprintf('[34] 加密网格  Nsoc=%d Np=%d | fuel=%.4f kg | wall=%.2f s | checks.pass=%d | saving=%.4f%%\n', ...
    probF.Nsoc, probF.Np, solF.fuel_total_kg, wallF, chkF.pass, chkF.saving_pct);

%% ======================= 收敛判定与 JSON =======================
fuelBase = solB.fuel_total_kg;
fuelFine = solF.fuel_total_kg;
diffPct  = abs(fuelFine - fuelBase) / fuelBase * 100;
tolPct   = cfg.convergence.fuel_diff_tol * 100;      % 0.5%
passConv = diffPct < tolPct;

out = struct();
out.pass          = passConv;
out.criterion     = '全程燃油（前向轨迹）差 < 0.5%（FR-011 / design §4.5-4）';
out.scenario      = SCN;
out.tier          = TIER;
out.mode          = 'continuous';
out.P_min_stable_kW = PMIN;
out.fuel_base_kg  = fuelBase;
out.fuel_refined_kg = fuelFine;
out.fuel_diff_pct = diffPct;
out.tol_pct       = tolPct;
out.grid_base     = struct('Nsoc', probB.Nsoc, 'Np', probB.Np, 'dSOC', cfgB.dSOC, 'dP_kW', cfgB.dP_kW);
out.grid_refined  = struct('Nsoc', probF.Nsoc, 'Np', probF.Np, 'dSOC', cfgF.dSOC, 'dP_kW', cfgF.dP_kW);
out.checks_pass_base    = chkB.pass;
out.checks_pass_refined = chkF.pass;
out.saving_pct_base     = chkB.saving_pct;
out.saving_pct_refined  = chkF.saving_pct;
out.baseline_feasible   = chkB.baseline.feasible;
out.wall_time_s   = struct('base', wallB, 'refined', wallF);
out.config_hash  = cfgHash(cfgF);
out.generated    = datestr(now, 'yyyy-mm-dd HH:MM:SS');

jsonPath = fullfile(ROOT, 'results', 'ecms_phase0', 'convergence_S07.json');
ecms.write_json_deep(jsonPath, out);
fprintf('[34] 已写入 %s\n', jsonPath);

%% ======================= 结果 =======================
fprintf('\n[34] ===== 收敛判定 =====\n');
fprintf('[34] 基准 fuel=%.4f kg | 加密 fuel=%.4f kg | 差=%.4f%%（容差 %.2f%%）\n', ...
    fuelBase, fuelFine, diffPct, tolPct);
if passConv
    fprintf('[34] 收敛检查 PASS（FR-011）——基准网格充分性成立，可继续主矩阵。\n');
else
    fprintf('[34] 收敛检查 FAIL（FR-011）。\n');
    error('ECMS:CONV', ['收敛检查不通过：S07 tier%d 连续 pmin%d 基准网格 fuel=%.4f kg，' ...
        '加密网格 fuel=%.4f kg，差 %.4f%% ≥ 容差 %.2f%%。', ...
        ' 按任务约束须 Escalate（T-012），不得自行加密基准网格后继续。'], ...
        TIER, PMIN, fuelBase, fuelFine, diffPct, tolPct);
end
