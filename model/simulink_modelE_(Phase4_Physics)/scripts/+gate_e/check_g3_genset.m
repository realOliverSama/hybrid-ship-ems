function r = check_g3_genset(e06, e07)
% gate_e.check_g3_genset — G3 机组判据（design §5.3）
%   OFF→STARTING(25±1 s)→SYNC(5±0.5 s)→ON 时序；ON 态 P≥P_min 或 below_min_flag；
%   启停一次 C_start=0.6±0.01 kg；1×500 vs 2×250 燃油可区分（差异 > 1%）；
%   启动缺口不虚构（STARTING/SYNC 期间 P=0）
r.name = 'G3_genset';
r.gate_class = 'physics_invariant';   % 时序量级为 external_literature（字段内分级）
r.checks = {};

% 时序（整数 tick 判据换算秒，采样 0.01 s）
ok_tstart = abs(e06.t_starting_s - 25) <= 1;
r.checks{end+1} = struct('item', 'STARTING_25_pm_1_s', 'pass', ok_tstart, ...
    'gate_class', 'external_literature', 'value_s', e06.t_starting_s); %#ok<SAGROW>
ok_tsync = abs(e06.t_sync_s - 5) <= 0.5;
r.checks{end+1} = struct('item', 'SYNC_5_pm_0.5_s', 'pass', ok_tsync, ...
    'gate_class', 'external_literature', 'value_s', e06.t_sync_s); %#ok<SAGROW>
ok_tcool = abs(e06.t_cooldown_s - 30) <= 1;
r.checks{end+1} = struct('item', 'COOLDOWN_30_pm_1_s', 'pass', ok_tcool, 'value_s', e06.t_cooldown_s); %#ok<SAGROW>

% 启动期间输出为零（缺口不虚构）
ok_gap = all(e06.P_during_startup_kW == 0);
r.checks{end+1} = struct('item', 'startup_gap_zero_output_no_fiction', 'pass', ok_gap); %#ok<SAGROW>

% 启动燃油一次性：C_start=0.6±0.01 kg（fuel_2 在 STARTING+SYNC 段增量）
ok_cstart = abs(e06.C_start_measured_kg - 0.6) <= 0.01;
r.checks{end+1} = struct('item', 'C_start_0.6_pm_0.01_kg', 'pass', ok_cstart, ...
    'value_kg', e06.C_start_measured_kg); %#ok<SAGROW>

% ON 态 P 域（稳段）：P≥P_min 或 below_min_flag（爬坡过渡段为 registered 过渡，链B notes）
ok_pmin = all(e06.P_on_steady_kW >= 125 - 1e-6 | e06.below_min_steady == 1);
r.checks{end+1} = struct('item', 'ON_state_steady_P_ge_Pmin_or_flag', 'pass', ok_pmin, ...
    'steady_min_kW', min([e06.P_on_steady_kW])); %#ok<SAGROW>

% 经济性可区分：1×500 vs 2×250 同总负荷燃油差 > 1%
diff_pct = abs(e07.fuel_1x500_kg - e07.fuel_2x250_kg) / e07.fuel_1x500_kg * 100;
ok_econ = diff_pct > 1;
r.checks{end+1} = struct('item', 'fuel_1x500_vs_2x250_distinguishable_gt_1pct', 'pass', ok_econ, ...
    'diff_pct', diff_pct); %#ok<SAGROW>

r.pass = all(cellfun(@(c) c.pass, r.checks));
end