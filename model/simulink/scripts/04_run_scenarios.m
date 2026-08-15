%% ========================================================================
%  04_run_scenarios.m — 批量运行全部工况 × EMS 模式（阶段13）
%  输出: datasets/raw/run_XXXX.mat / .csv / _metadata.json
%  metadata 含: scenario_id, target_speed, voyage_distance, river_current,
%  resistance_profile, resistance_change_position, resistance_change_amplitude,
%  initial_SOC, random_seed, model_version, parameter_file_hash
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
rawDir = fullfile(ROOT, 'datasets', 'raw');
if ~exist(rawDir, 'dir'), mkdir(rawDir); end

MDL = 'HybridShip_Model_MVP';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

paramHash = model.md5file(fullfile(ROOT, 'config', 'Model_Parameters.m'));
modelVer = 'Model_MVP_v1.0';
randomSeedBase = 42;

runIdx = 0;
total = numel(SCENARIOS) * 2;
for s = 1:numel(SCENARIOS)
    for mode = 0:1
        runIdx = runIdx + 1;
        SCN = SCENARIOS(s);
        SCN.random_seed = randomSeedBase + s;
        stopTimeLimit = SCN.voyage_distance_m / SCN.target_speed_mps + 1200;
        fprintf('[04] (%d/%d) %s EMS_MODE=%d dist=%.0f m ... ', runIdx, total, SCN.name, mode, SCN.voyage_distance_m);
        tic;
        try
            out = model.run_scenario_sim(MDL, SCN, mode, stopTimeLimit);
            ds = model.extract_logsout(out);
        catch err
            fprintf('失败: %s\n', err.message);
            continue;
        end
        wall = toc;
        % 1 s 重采样
        ds1 = model.resample_1s(ds);
        ds1.scenario_id = repmat(double(SCN.id), numel(ds1.t), 1);
        ds1.ems_mode = repmat(double(mode), numel(ds1.t), 1);
        fname = sprintf('run_%04d', runIdx);
        % .mat
        save(fullfile(rawDir, [fname '.mat']), 'ds1', 'SCN', 'mode');
        % .csv
        model.write_run_csv(fullfile(rawDir, [fname '.csv']), ds1);
        % metadata.json
        md = struct( ...
            'run_id', fname, ...
            'scenario_id', SCN.id, 'scenario_name', SCN.name, ...
            'ems_mode', mode, ...
            'target_speed', SCN.target_speed_mps, ...
            'voyage_distance', SCN.voyage_distance_m, ...
            'river_current', SCN.river_current_mps, ...
            'resistance_profile', SCN.resistance_profile, ...
            'resistance_change_position', SCN.resistance_change_position_m, ...
            'resistance_change_amplitude', SCN.resistance_change_amplitude, ...
            'resistance_change_ramp_time_s', SCN.resistance_change_ramp_time_s, ...
            'initial_SOC', SCN.initial_SOC, ...
            'random_seed', SCN.random_seed, ...
            'model_version', modelVer, ...
            'parameter_file_hash', paramHash, ...
            'sim_duration_s', ds.t(end), 'wall_time_s', wall, ...
            'sample_period_s', 1.0, ...
            'solver', 'ode3 fixed 0.01s');
        model.write_json(fullfile(rawDir, [fname '_metadata.json']), md);
        fprintf('完成 t_end=%.0f s (%.1f s)\n', ds.t(end), wall);
    end
end
fprintf('[04] 批量运行完成，共 %d 个航次数据 -> %s\n', runIdx, rawDir);
fprintf('[04] 注意：AI 模式当前预测源为 persistence（AI_Pred_Source=0），行为与 Baseline 一致；\n');
fprintf('[04]       接入真实预测前，两模式对比仅验证接口与回退逻辑。\n');
