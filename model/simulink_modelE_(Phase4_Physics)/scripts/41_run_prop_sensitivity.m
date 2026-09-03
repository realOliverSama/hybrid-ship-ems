%% ========================================================================
%  41_run_prop_sensitivity.m — T-011 螺旋桨参数族 ±20% OAT 敏感性扫掠（FR-417）
%  一次一参数（OAT）：K_Fw/K_Fv/K_Qw/K_Qv/Wake_w_deep × {0.8, 1.0, 1.2} = 15 cases
%  工况：S01 静水深水（1500 m 短程）；产出机器可读 JSON + 敏感性报告（结论稳健域）
%  纪律：Q1 裁决——其余敏感性档位（P_min/ramp/τ/C_start）只登记不扫掠；
%        作用于 T-005 重标定后的 Model E nominal（K 值来自参数文件，禁止沿用 Model D 值）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
MDL = 'HybridShip_ModelE';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

params = {'K_Fw', 'K_Fv', 'K_Qw', 'K_Qv', 'Wake_w_deep'};
base = struct();
for k = 1:numel(params)
    base.(params{k}) = eval(params{k});
end

SCN = SCENARIOS(1);                       % S01 静水深水
SCN.voyage_distance_m = 1500;
factors = [0.8, 1.0, 1.2];

results = struct('schema', 'modelE_prop_sensitivity_v1', ...
    'generated_at', char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ')), ...
    'design_point', struct('V_design_mps', V_design_mps, 'V_design_kn', V_design_kn, ...
    'P_prop_elec_nom_kW', mean([])), ...
    'cases', {{}});
icase = 0;
summary = {};
for ip = 1:numel(params)
    for f = factors
        icase = icase + 1;
        pname = params{ip};
        % 先恢复全部参数 nominal 再注入当前因子（防 OAT 参数泄漏跨 case 污染）
        for k2 = 1:numel(params)
            assignin('base', params{k2}, base.(params{k2}));
        end
        assignin('base', pname, base.(pname) * f);     % OAT 注入（Parameter scope 自 base 解析）
        SCN.water_depth_m = 20.0;                       % 深水（隔离桨参数效应）
        SCN.initial_SOC = 0.60;
        assignin('base', 'Bat_SOC_init', SCN.initial_SOC);
        out = model.run_scenario_sim(MDL, SCN, 0, 2500);
        ds = model.extract_logsout(out);
        iSteady = ds.x_ship_m > 1000;                   % 稳态段（最后 1/3 航程）
        r = struct();
        r.case_id = sprintf('%s_x%.1f', pname, f);
        r.parameter = pname;
        r.factor = f;
        r.v_steady_mps = mean(ds.v_ship_mps(iSteady));
        r.v_steady_kn = r.v_steady_mps * 1.944;
        r.P_prop_elec_kW = mean(ds.P_prop_actual_kW(iSteady));
        r.R_hull_N = mean(ds.R_hull_N(iSteady));
        r.fuel_total_kg = ds.fuel_total_kg(end);
        r.SOC_end = ds.SOC(end);
        results.cases{icase} = r; %#ok<AGROW>
        summary{end+1} = sprintf('| %s | %.2f | %.3f | %.1f | %.1f | %.3f |', ... %#ok<AGROW>
            r.case_id, f, r.v_steady_kn, r.P_prop_elec_kW, r.R_hull_N, r.fuel_total_kg);
        fprintf('[41] case %2d/15 %s: v=%.2f kn P=%.1f kW fuel=%.3f kg\n', ...
            icase, r.case_id, r.v_steady_kn, r.P_prop_elec_kW, r.fuel_total_kg);
    end
end
% 恢复 nominal（防污染后续会话）
for k = 1:numel(params)
    assignin('base', params{k}, base.(params{k}));
end

% nominal 档（factor=1.0）均值
allCases = [results.cases{:}];                 % struct 数组
inom = [allCases.factor] == 1.0;
nom = struct('v_kn', mean([allCases(inom).v_steady_kn]), ...
    'P_kW', mean([allCases(inom).P_prop_elec_kW]), ...
    'fuel_kg', mean([allCases(inom).fuel_total_kg]));
results.design_point.P_prop_elec_nom_kW = nom.P_kW;

% ---- 机器可读 JSON ----
if ~exist(fullfile(ROOT, 'results'), 'dir'), mkdir(fullfile(ROOT, 'results')); end
jsonPath = fullfile(ROOT, 'results', 'prop_sensitivity.json');
fid = fopen(jsonPath, 'w'); fwrite(fid, jsonencode(results, 'PrettyPrint', true)); fclose(fid);
fprintf('[41] JSON: %s\n', jsonPath);

% ---- 敏感性报告（脚本渲染 + 结论稳健域保留块由 T-013 评审后定稿）----
repPath = fullfile(ROOT, '..', '..', 'docs', 'phase4', 'a2_model_e', 'prop_sensitivity_report.md');
fid = fopen(repPath, 'w', 'n', 'UTF-8');
fprintf(fid, '# 螺旋桨参数族 ±20%% OAT 敏感性报告（T-011，FR-417）\n\n');
fprintf(fid, '> 生成：41_run_prop_sensitivity.m（%s）；工况 S01 静水深水 1500 m；nominal 档 = T-005 重标定值\n\n', ...
    results.generated_at);
fprintf(fid, '| case | factor | v_steady [kn] | P_prop_elec [kW] | R_hull [N] | fuel [kg] |\n');
fprintf(fid, '|---|---|---|---|---|---|\n');
for i = 1:numel(summary), fprintf(fid, '%s\n', summary{i}); end
fprintf(fid, '\nnominal 档均值: v=%.2f kn, P=%.1f kW, fuel=%.3f kg\n\n', nom.v_kn, nom.P_kW, nom.fuel_kg);
fprintf(fid, '## 敏感性幅度（相对 nominal，factor 0.8/1.2）\n\n');
for ip = 1:numel(params)
    rows = allCases((ip-1)*3+1 : ip*3);
    fprintf(fid, '- **%s**: v ∈ [%.2f, %.2f] kn（nominal %.2f）；P ∈ [%.1f, %.1f] kW；fuel ∈ [%.3f, %.3f] kg\n', ...
        params{ip}, min([rows.v_steady_kn]), max([rows.v_steady_kn]), nom.v_kn, ...
        min([rows.P_prop_elec_kW]), max([rows.P_prop_elec_kW]), min([rows.fuel_total_kg]), max([rows.fuel_total_kg]));
end
fprintf(fid, '\n## 结论稳健域\n\n<!-- MANUAL:conclusion_robustness -->\n（待 T-013 用户评审：基于上表对速度-功率曲线与油耗结论的影响幅度，界定 keep_simplified 简化桨的结论稳健域；±20%% OAT 全部 15 cases 零失败。）\n<!-- /MANUAL:conclusion_robustness -->\n');
fclose(fid);
fprintf('[41] 报告: %s\n', repPath);
fprintf('T011-SENSITIVITY: 15 cases ALL RUN, zero failures\n');