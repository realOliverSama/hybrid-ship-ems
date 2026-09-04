%% ========================================================================
%  40_run_modelE_gate.m — T-412 Model E 整体物理 Gate（FR-425, design §5）
%  专用确定性 Plant 测试台：E01~E10 直接注入指令，不经 EMS 控制器（D-9）
%  判据：G1~G6（scripts/+gate_e/check_*.m），输出 results/gate_result.json
%  纪律：overall=false → error('GATE:*') 非零退出；Gate FAIL 禁止提交（NFR-408）；
%        PASS 后不自动冻结基线（design §5.1，交 T-013 用户评审）
%  用法（脚本名数字开头，按项目惯例执行）：
%    cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/40_run_modelE_gate.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));   % Phase 4-B T-001：控制参数在物理参数之后（FR-451）
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
model.write_config_manifest(ROOT, ModelE_Sensitivity_Profile());
MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
t_all = tic;
sims = {};

%% ================= E01 静水巡航稳态（law=0，深水 5000 m 全程）=================
fprintf('[GATE] E01 hull steady 11 kn ...\n');
SCN = SCENARIOS(1); SCN.initial_SOC = 0.60;
assignin('base', 'Bat_SOC_init', 0.60);
out = model.run_scenario_sim(MDL, SCN, 0, 6000);
ds01 = model.extract_logsout(out); sims{end+1} = struct('name', 'E01', 'ds', ds01);
iS = ds01.x_ship_m > 0.8 * max(ds01.x_ship_m);
E01 = struct('name', 'E01', 'ds', ds01, 'P_prop_kW', mean(ds01.P_prop_actual_kW(iS)), ...
    'R_steady_N', mean(ds01.R_hull_N(iS)), 'v_steady_kn', mean(ds01.v_ship_mps(iS)) * 1.944);
assert(max(ds01.battery_power_limited) == 0 && max(ds01.battery_discriminant_limit) == 0, ...
    'E01 正常巡航不应出现 power-limit flag');
E01_flags_zero = true;
% E01a：确定性两跑第一跑数据（logsout 快照）
E01a = ds01;

%% ================= E02 静水全功率 ramp（极速带 + 推力平衡）=================
fprintf('[GATE] E02 full-power ramp ...\n');
SCN2 = SCENARIOS(1);
SCN2.v_knots_v = [8.0 0 0 0 0 0];                     % 速度剖面查表 8 m/s → PI 饱和 → 推进指令打满 700 kW
SCN2.voyage_distance_m = 30000;                        % 距离放宽（时间终止为主）
SCN2.initial_SOC = 0.60;
assignin('base', 'Bat_SOC_init', 0.60);
out = model.run_scenario_sim(MDL, SCN2, 0, 2200);
ds02 = model.extract_logsout(out); sims{end+1} = struct('name', 'E02', 'ds', ds02);
iS2 = ds02.t > 0.85 * max(ds02.t);                    % 末段稳态（时间域）
E02 = struct('name', 'E02', 'R_steady_N', mean(ds02.R_hull_N(iS2)), ...
    'v_steady_kn', mean(ds02.v_ship_mps(iS2)) * 1.944);

%% ================= E03/E04 顺水 / 逆水方向性 =================
fprintf('[GATE] E03/E04 downstream / upstream ...\n');
[E03, srec] = gate_e.run_dir_sim(MDL, +1);  % 顺水
sims{end+1} = srec;
[E04, srec] = gate_e.run_dir_sim(MDL, -1);  % 逆水
sims{end+1} = srec;

%% ================= E05 浅水 H/T 六档（单调性 + 域）=================
fprintf('[GATE] E05 shallow H/T tiers ...\n');
E05 = struct('cases', struct('HT_ratio', {}, 'R_hull_N', {}, 'domain_warning', {}));
for k = 31:36
    SCNs = SCENARIOS(k); SCNs.voyage_distance_m = 1200; SCNs.initial_SOC = 0.60;
    assignin('base', 'Bat_SOC_init', 0.60);
    out = model.run_scenario_sim(MDL, SCNs, 0, 1500);
    dss = model.extract_logsout(out); sims{end+1} = struct('name', sprintf('E05_%d', k), 'ds', dss);
    iSs = dss.x_ship_m > 0.8 * max(dss.x_ship_m);
    E05.cases(end+1) = struct('HT_ratio', mean(dss.HT_ratio(iSs)), ...
        'R_hull_N', mean(dss.R_hull_N(iSs)), 'domain_warning', max(dss.domain_warning)); %#ok<SAGROW>
end

%% ================= E06 DG 启停时序 + aborted-start + C_start =================
fprintf('[GATE] E06 genset start/stop sequence ...\n');
% assignin 式注入（与 run_scenario_sim 同通道；SiN.setVariable 对 From Workspace
% 数据源在本环境非确定，2026-09-04 夜班 Gate 调试结论）
gate_e.gate_assign_e06_inputs();
set_param(MDL, 'StopTime', '160');
out = sim(MDL);
ds06 = model.extract_logsout(out); sims{end+1} = struct('name', 'E06', 'ds', ds06);
E06 = gate_e.analyze_e06(ds06);

%% ================= E07 双机与燃油区分（1×500 vs 2×250 同总负荷）=================
fprintf('[GATE] E07 fuel 1x500 vs 2x250 ...\n');
[E07, srec] = gate_e.run_e07(MDL, 500, 0);
sims{end+1} = srec;
[E07b, srec] = gate_e.run_e07(MDL, 250, 250);
sims{end+1} = srec;
E07.fuel_1x500_kg = E07.fuel_total_kg; E07.fuel_2x250_kg = E07b.fuel_total_kg;

%% ================= E08 电池充放/SOP（大负荷冲击逼近 SOP 边界）=================
fprintf('[GATE] E08 battery SOP shock ...\n');
SCN8 = SCENARIOS(1); SCN8.voyage_distance_m = 2000; SCN8.hotel_kW = 800;  % 大负荷冲击
SCN8.initial_SOC = 0.35;                                   % 低 SOC → SOP 降额带
assignin('base', 'Bat_SOC_init', 0.35);
out = model.run_scenario_sim(MDL, SCN8, 0, 2500);
ds08 = model.extract_logsout(out); sims{end+1} = struct('name', 'E08', 'ds', ds08);
iS8 = ds08.x_ship_m > 0.5 * max(ds08.x_ship_m);
% OCV 开路估计（SOC→LUT）对照 V_bat
soc_seg = ds08.SOC(iS8); vb_seg = ds08.V_bat_V(iS8);
ocv_seg = interp1(Bat_SOC_LUT_nodes, Bat_OCV_LUT_V, min(max(soc_seg, 0.1), 0.9));
% power-limit 一致性：flag 置位时 P_bat 必须被压回（P_actual ≤ 请求 且 flag=1 与
% P < P_dis_avail 同时出现才算语义正确——power-limit semantics 非 clamp 硬塞）
fl = ds08.battery_power_limited;
onSteps = fl > 0.5;
if any(onSteps)
    consistent = all(ds08.P_bat_actual_kW(onSteps) <= ds08.P_bat_dis_avail_kW(onSteps) + 1e-6);
else
    consistent = true;
end
E08 = struct('name', 'E08', 'soc_start', ds08.SOC(1), 'soc_end', ds08.SOC(end), ...
    'soc_decreased', ds08.SOC(end) < ds08.SOC(1), ...
    'vt_below_ocv_ratio', mean(vb_seg ./ ocv_seg), ...
    'P_bat_bus_kW', ds08.P_bat_actual_kW, 'P_dis_avail_kW', ds08.P_bat_dis_avail_kW, ...
    'flags_consistent', consistent, 'e01_flags_zero', 1);

%% ================= E09 组合暂态（全机停机 + 欠压保护链）=================
fprintf('[GATE] E09 combined transient (all-genset stop + undervolt protection) ...\n');
gate_e.gate_assign_e09_inputs();
set_param(MDL, 'StopTime', '260');
out = sim(MDL);
ds09 = model.extract_logsout(out); sims{end+1} = struct('name', 'E09', 'ds', ds09);
E09 = struct('name', 'E09', 'ds', ds09);
E09.protection_triggered = any(ds09.P_prop_limit_kW < evalin('base', 'Mot_P_nom') - 1e-6) || any(ds09.P_unserved_kW > 1e-6);
fprintf('[GATE]   protection chain triggered: %d\n', E09.protection_triggered);

%% ================= E10 law=0/law=5 兼容 smoke（不作物理判据，D-9）=================
fprintf('[GATE] E10 law=0/5 compatibility smoke ...\n');
SCN10 = SCENARIOS(1); SCN10.voyage_distance_m = 600;
e10_ok = true; e10_note = {};
for law = [0 5]
    try
        out = model.run_scenario_sim(MDL, SCN10, law, 900);
        dst = model.extract_logsout(out);
        if ~all(isfinite(dst.V_dc_V)), error('NaN in V_dc'); end
        e10_note{end+1} = sprintf('law=%d OK', law); %#ok<SAGROW>
    catch e
        e10_ok = false;
        e10_note{end+1} = sprintf('law=%d FAIL: %s', law, e.message); %#ok<SAGROW>
    end
end
E10 = struct('ok', e10_ok, 'note', {e10_note});

%% ================= E01b 确定性第二跑（G6）=================
fprintf('[GATE] E01b determinism re-run ...\n');
SCN = SCENARIOS(1); SCN.initial_SOC = 0.60;
assignin('base', 'Bat_SOC_init', 0.60);
out = model.run_scenario_sim(MDL, SCN, 0, 6000);
ds01b = model.extract_logsout(out);
E01b = ds01b;

%% ================= 判据聚合 =================
fprintf('[GATE] aggregating G1~G6 ...\n');
G1 = gate_e.check_g1_structure(ROOT, sims);
G2 = gate_e.check_g2_hull_shallow(E01, E02, E03, E04, E05);
G3 = gate_e.check_g3_genset(E06, E07);
G4 = gate_e.check_g4_battery(E08, true, E01_flags_zero);
G5 = gate_e.check_g5_energy_closure(struct('name', 'E01', 'ds', ds01), struct('name', 'E09', 'ds', ds09));
G6 = gate_e.check_g6_runtime(E01a, E01b, toc(t_all));

overall = G1.pass && G2.pass && G3.pass && G4.pass && G5.pass && G6.pass && E10.ok;

checks = struct('G1', {G1}, 'G2', {G2}, 'G3', {G3}, 'G4', {G4}, 'G5', {G5}, 'G6', {G6}, 'E10_compat_smoke', E10);
res = struct('schema', 'modelE_gate_result_v1', ...
    'spec', 'phase4-a2-model-e-construction / T-412', ...
    'generated_at', char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ')), ...
    'overall', overall, 'checks', {checks});
if ~exist(fullfile(ROOT, 'results'), 'dir'), mkdir(fullfile(ROOT, 'results')); end
fid = fopen(fullfile(ROOT, 'results', 'gate_result.json'), 'w');
fwrite(fid, jsonencode(res, 'PrettyPrint', true)); fclose(fid);
fprintf('[GATE] gate_result.json written. overall=%d\n', overall);
gate_e.print_summary(checks);
close_system(MDL, 0);
if ~overall
    error('GATE:ModelE_T412_gate FAIL——禁止提交（NFR-408）；禁止改参数凑 PASS，须回到 design 修订');
end
fprintf('[GATE] Model E 整体物理 Gate PASS（基线冻结交 T-013 用户评审）\n');

%% ================= 局部函数 =================
