%% ========================================================================
%  03_run_smoke_tests.m — 5 项烟雾测试 + 结果图（阶段15/16）
%  T01 静水定速 / T02 顺水 / T03 逆水 / T04 阻力渐增 / T05 阻力渐减
%  输出: results/<scenario>/*.png, reports/smoke_summary.md
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
run(fullfile(ROOT, 'config', 'Model_GateThresholds.m'));
if ~exist(fullfile(ROOT, 'results'), 'dir'), mkdir(fullfile(ROOT, 'results')); end
if ~exist(fullfile(ROOT, 'reports'), 'dir'), mkdir(fullfile(ROOT, 'reports')); end

MDL = 'HybridShip_Model_MVP';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));

% ---- 烟雾测试工况定义（距离缩短以便快速验证） ----
TESTS = struct('id', {}, 'name', {}, 'scenario_id', {}, 'mode', {}, ...
    'distance_m', {}, 'stopTimeLimit', {}, 'checkDownstream', {}, 'checkUpstream', {}, ...
    'checkResInc', {}, 'notes', {});
TESTS(1) = struct('id', 1, 'name', 'T01_still_water', 'scenario_id', 1, 'mode', 0, ...
    'distance_m', 1200, 'stopTimeLimit', 3600, ...
    'checkDownstream', false, 'checkUpstream', false, 'checkResInc', false, ...
    'notes', '静水：静止起步、定速巡航、航程终止');
TESTS(2) = struct('id', 2, 'name', 'T02_downstream', 'scenario_id', 2, 'mode', 0, ...
    'distance_m', 1200, 'stopTimeLimit', 3600, ...
    'checkDownstream', true, 'checkUpstream', false, 'checkResInc', false, ...
    'notes', '顺水：相对水速与阻力方向验证');
TESTS(3) = struct('id', 3, 'name', 'T03_upstream', 'scenario_id', 3, 'mode', 0, ...
    'distance_m', 1200, 'stopTimeLimit', 3600, ...
    'checkDownstream', true, 'checkUpstream', true, 'checkResInc', false, ...
    'notes', '逆水：负荷/阻力增加验证');
TESTS(4) = struct('id', 4, 'name', 'T04_res_inc', 'scenario_id', 4, 'mode', 0, ...
    'distance_m', 2200, 'stopTimeLimit', 3600, ...
    'checkDownstream', false, 'checkUpstream', false, 'checkResInc', true, ...
    'notes', '阻力平滑增加：发电机爬坡 + 电池补偿');
TESTS(5) = struct('id', 5, 'name', 'T05_res_dec', 'scenario_id', 5, 'mode', 0, ...
    'distance_m', 2600, 'stopTimeLimit', 3600, ...
    'checkDownstream', false, 'checkUpstream', false, 'checkResInc', false, ...
    'notes', '阻力平滑降低：发电机降功率');

allPass = true;
results = cell(numel(TESTS), 1);
gateMetrics = cell(numel(TESTS), 1);
for k = 1:numel(TESTS)
    T = TESTS(k);
    SCN = SCENARIOS(T.scenario_id);
    SCN.voyage_distance_m = T.distance_m;
    if T.scenario_id == 4
        SCN.resistance_change_position_m = 800;   % 触发点提前，保证测试时长内完成
    elseif T.scenario_id == 5
        SCN.resistance_change_position_m = 1200;
    end
    fprintf('[03] ===== %s (S%02d, EMS_MODE=%d) =====\n', T.name, T.scenario_id, T.mode);
    try
        out = model.run_scenario_sim(MDL, SCN, T.mode, T.stopTimeLimit);
        ds = model.extract_logsout(out);
        % 基本检查
        chk = model.check_run(ds, SCN, T.mode, T.stopTimeLimit);
        % 门禁指标（供回归对比与基线持久化）
        gateMetrics{T.id} = model.gate_metrics(ds); %#ok<AGROW>
        % 跨工况检查：顺水 vs 逆水
        if T.checkDownstream || T.checkUpstream
            if T.scenario_id == 2
                dsDown = ds;
            elseif T.scenario_id == 3
                dsUp = ds;
            end
        end
        % 阻力增加检查（T04）
        if T.checkResInc
            tTrig = find(ds.resistance_multiplier > 1.01, 1, 'first');
            if isempty(tTrig)
                chk.results{end+1, 1} = 'res_demand_rise';
                chk.results{end, 2} = false;
                chk.results{end, 3} = '阻力未触发';
                chk.ok = false;
            else
                mBefore = mean(ds.P_prop_demand_kW(max(tTrig - 200, 1):tTrig - 1));
                mAfter = mean(ds.P_prop_demand_kW(min(tTrig + 200, numel(ds.t)):end));
                pass = mAfter > mBefore + 10;
                chk.results{end+1, 1} = 'res_demand_rise';
                chk.results{end, 2} = pass;
                chk.results{end, 3} = sprintf('P_demand 前=%.1f kW 后=%.1f kW', mBefore, mAfter);
                if ~pass, chk.ok = false; end
            end
        end
        results{k} = chk;
        % 结果图
        outdir = fullfile(ROOT, 'results', T.name);
        model.plot_smoke(ds, SCN, outdir);
        fprintf('[03]   结果图 -> %s\n', outdir);
        for r = 1:size(chk.results, 1)
            tag = 'PASS'; if ~chk.results{r, 2}, tag = 'FAIL'; allPass = false; end
            fprintf('[03]   [%s] %-22s %s\n', tag, chk.results{r, 1}, chk.results{r, 3});
        end
    catch err
        fprintf('[03]   [ERROR] %s: %s\n', T.name, err.message);
        allPass = false;
        chk = struct('ok', false, 'results', {{'sim_exception', false, err.message}}, 'details', '');
        results{k} = chk;
    end
end

% ---- 跨工况汇总检查：T02 顺水负荷 < T03 逆水负荷 ----
if numel(results) >= 3 && ~isempty(results{2}) && ~isempty(results{3})
    p2 = mean(dsDown.P_load_total_kW);
    p3 = mean(dsUp.P_load_total_kW);
    pass = p2 < p3;
    if pass, tg = 'PASS'; else, tg = 'FAIL'; end
    fprintf('[03] [%s] 顺水负荷 %.1f kW < 逆水负荷 %.1f kW\n', tg, p2, p3);
    results{2}.results{end+1, 1} = 'downstream_lower_load';
    results{2}.results{end, 2} = pass;
    results{2}.results{end, 3} = sprintf('T02=%.1f kW vs T03=%.1f kW', p2, p3);
    results{3}.results{end+1, 1} = 'downstream_lower_load';
    results{3}.results{end, 2} = pass;
    results{3}.results{end, 3} = sprintf('T02=%.1f kW vs T03=%.1f kW', p2, p3);
    if ~pass, allPass = false; end
end

% ---- 质量门：基线回归对比 + 机器可读裁决 ----
baselinePath = fullfile(ROOT, GATE.baseline_file);
baseline = [];
if exist(baselinePath, 'file')
    baseline = jsondecode(fileread(baselinePath));
    fprintf('[03] 门禁基线: %s（回归模式）\n', baselinePath);
else
    fprintf('[03] 门禁基线不存在，本次运行将建立: %s\n', baselinePath);
end
newBaseline = struct();
for k = 1:numel(TESTS)
    if isempty(gateMetrics{k}), continue; end
    gres = model.gate_compare(TESTS(k).name, gateMetrics{k}, baseline, GATE);
    for r = 1:size(gres, 1)
        tag = 'PASS'; if ~gres{r, 2}, tag = 'FAIL'; allPass = false; end
        fprintf('[03]   [%s] %-22s %s\n', tag, [TESTS(k).name '/' gres{r, 1}], gres{r, 3});
        results{k}.results{end+1, 1} = gres{r, 1};
        results{k}.results{end, 2} = gres{r, 2};
        results{k}.results{end, 3} = gres{r, 3};
    end
    newBaseline.(TESTS(k).name) = gateMetrics{k};
end
if isempty(baseline)
    fid = fopen(baselinePath, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s', jsonencode(newBaseline, 'PrettyPrint', true));
    fclose(fid);
    fprintf('[03] 已建立门禁基线（请随提交入库）: %s\n', baselinePath);
end

% 机器可读裁决文件（Agent 以此为准，禁止主观解读终端输出）
gateVerdict = struct('overall', allPass, 'timestamp', datestr(now, 'yyyy-mm-ddTHH:MM:SS'), 'tests', {{}});
for k = 1:numel(TESTS)
    for r = 1:size(results{k}.results, 1)
        gateVerdict.tests{end+1} = struct('test', TESTS(k).name, ...
            'check', results{k}.results{r, 1}, 'pass', logical(results{k}.results{r, 2}), ...
            'detail', results{k}.results{r, 3}); %#ok<AGROW>
    end
end
fid = fopen(fullfile(ROOT, 'results', 'gate_result.json'), 'w', 'n', 'UTF-8');
fprintf(fid, '%s', jsonencode(gateVerdict, 'PrettyPrint', true));
fclose(fid);
fprintf('[03] 门禁裁决: %s\n', fullfile(ROOT, 'results', 'gate_result.json'));

% ---- 汇总报告 ----
fid = fopen(fullfile(ROOT, 'reports', 'smoke_summary.md'), 'w', 'n', 'UTF-8');
fprintf(fid, '# 烟雾测试汇总 (smoke_summary.md)\n\n');
fprintf(fid, '| 测试 | 检查项 | 结果 | 说明 |\n|---|---|---|---|\n');
for k = 1:numel(TESTS)
    chk = results{k};
    for r = 1:size(chk.results, 1)
        tag = 'PASS'; if ~chk.results{r, 2}, tag = '**FAIL**'; end
        fprintf(fid, '| %s | %s | %s | %s |\n', TESTS(k).name, chk.results{r, 1}, tag, chk.results{r, 3});
    end
end
if allPass, tl = '全部通过'; else, tl = '存在失败项，见上表'; end
fprintf(fid, '\n**总体结论: %s**\n', tl);
fclose(fid);
fprintf('[03] 汇总报告: %s\n', fullfile(ROOT, 'reports', 'smoke_summary.md'));
if allPass, tl2 = '全部通过'; else, tl2 = '存在失败项'; end
fprintf('[03] 烟雾测试%s\n', tl2);

% ---- 门禁出口：FAIL 必须以 error 终止（Agent 工具调用确定性失败） ----
if ~allPass
    error('GATE:SmokeFailed', ...
        '[03] 质量门 FAIL：烟雾测试存在失败项，禁止提交。详见 results/gate_result.json');
end
