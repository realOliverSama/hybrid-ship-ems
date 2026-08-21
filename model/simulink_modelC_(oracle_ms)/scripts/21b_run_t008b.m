%% ========================================================================
%  21b_run_t008b.m — T-008B 机理门禁：弱机点 W1 C0 vs CMS（8 次仿真）
%  Spec: multistep-predictive-ems（T-008 补充验证，用户裁决 2026-08-21）
%  固定配置（主 Agent 已定）:
%    W1 = Gen_P_max=640 (λ_P=0.8), Gen_Ramp_Up/Down=20, tau_gen=10
%    场景: S07 + S09；每场景 {C0, CMS H20, CMS H45, CMS H60}
%  输出:
%    results/ms_phase1/t008b/ds_t008b_W1_<scn>_<ctrl>.mat
%    results/ms_phase1/t008b/runs.csv（write_msff_csv 契约，anchor='W1'）
%    results/ms_phase1/t008b/run_t008b_<ts>.log
%  失败纪律: 失败落盘 NaN 行 + 原因；连续 2 次失败中止。
%  本脚本只做执行与数据采集；机理解读由主 Agent 完成（21c 只陈述事实）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
model.load_mlp_weights(ROOT);
run(fullfile(ROOT,'config','Model_Scenarios.m'));

MDL = 'HybridShip_ModelCms_DCBus';
outDir = fullfile(ROOT,'results','ms_phase1','t008b');
if ~exist(outDir,'dir'), mkdir(outDir); end
csvPath = fullfile(outDir,'runs.csv');
if exist(csvPath,'file'), delete(csvPath); end
TS = datestr(now,'yyyymmdd_HHMMSS');
logPath = fullfile(outDir,sprintf('run_t008b_%s.log',TS));
lof = fopen(logPath,'w');

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

% ---------------- 固定矩阵 ----------------
genEff = struct('Gen_P_max',640,'Gen_Ramp_Up',20,'Gen_Ramp_Down',20,'tau_gen',10);
scenList = [7 9];
horizons = [20 45 60];
stopTimeLimit = 5000/5.66 + 1800;

total = 0; okCnt = 0; failCnt = 0; consecFail = 0;
for sc = scenList
    SCN = SCENARIOS(sc);
    sname = sprintf('S%02d', sc);
    orc = load(fullfile(ROOT,'results','ms_phase1',sprintf('oracle_%s.mat',sname)));
    D = orc.D_ms;

    % ---- C0 ----
    ctrl = 'C0'; expID = sprintf('t008b_W1_%s_C0', sname);
    meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
        'anchor','W1', 'lambda_P',0.8, 'R_gen',20, 'tau_gen',10, ...
        'SOC0',SCN.initial_SOC, 'horizon_s',0, 'info_mode','-');
    t0 = tic;
    try
        out = model.run_ms_sim(MDL, SCN, 0, 0, 0, D, stopTimeLimit, genEff);
        ds = model.extract_logsout(out);
        m = model.msff_metrics(ds);
        wall = toc(t0);
        save(fullfile(outDir,['ds_' expID '.mat']), 'ds', 'm', 'meta', 'genEff');
        model.write_msff_csv(csvPath, meta, m, 'ok');
        okCnt = okCnt + 1; consecFail = 0; total = total + 1;
        fprintf('[B] %-26s %s law=0: fuel=%.2f Vdc_min=%.1f SOC_end=%.4f t_end=%.0f wall=%.1fs\n', ...
            expID, SCN.name, m.fuel_kg, m.Vdc_min, m.SOC_end, ds.t(end), wall);
        fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f t_end=%.1f wall=%.1f\n', ...
            expID, m.fuel_kg, m.Vdc_min, m.SOC_end, ds.t(end), wall);
    catch err
        wall = toc(t0);
        total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
        fprintf('[B] %-26s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
        fprintf(lof, '%s FAIL: %s\n', expID, err.message);
        model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
        if consecFail >= 2
            fprintf('[B] **** 连续 2 次失败，中止 ****\n');
            error('GATE:T008B: 连续 2 次失败 (详见 %s)', logPath);
        end
    end

    % ---- CMS (law=3, traj) × horizons ----
    for h = horizons
        ctrl = 'CMS'; hh = sprintf('H%02d', h);
        expID = sprintf('t008b_W1_%s_%s_%s', sname, ctrl, hh);
        meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
            'anchor','W1', 'lambda_P',0.8, 'R_gen',20, 'tau_gen',10, ...
            'SOC0',SCN.initial_SOC, 'horizon_s',h, 'info_mode','traj');
        t0 = tic;
        try
            out = model.run_ms_sim(MDL, SCN, 3, h, 0, D, stopTimeLimit, genEff);
            ds = model.extract_logsout(out);
            m = model.msff_metrics(ds);
            wall = toc(t0);
            % 预测有效性检查（escalate 条件：pred_valid 全程 0）
            if isfield(ds,'MSFF_pred_valid') && ~any(ds.MSFF_pred_valid > 0)
                error('T008B: %s 全程 pred_valid=0，oracle 通道未生效', expID);
            end
            save(fullfile(outDir,['ds_' expID '.mat']), 'ds', 'm', 'meta', 'genEff');
            model.write_msff_csv(csvPath, meta, m, 'ok');
            okCnt = okCnt + 1; consecFail = 0; total = total + 1;
            fprintf('[B] %-26s %s law=3 H=%3d traj: fuel=%.2f Vdc_min=%.1f SOC_end=%.4f ff_act=%.1fs t_end=%.0f wall=%.1fs\n', ...
                expID, SCN.name, h, m.fuel_kg, m.Vdc_min, m.SOC_end, m.ff_active_s, ds.t(end), wall);
            fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f ff_act=%.2f t_end=%.1f wall=%.1f\n', ...
                expID, m.fuel_kg, m.Vdc_min, m.SOC_end, m.ff_active_s, ds.t(end), wall);
        catch err
            wall = toc(t0);
            total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
            fprintf('[B] %-26s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
            fprintf(lof, '%s FAIL: %s\n', expID, err.message);
            model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
            if consecFail >= 2
                fprintf('[B] **** 连续 2 次失败，中止 ****\n');
                error('GATE:T008B: 连续 2 次失败 (详见 %s)', logPath);
            end
        end
    end
end

fclose(lof);
fprintf('[B] 完成: 总=%d ok=%d fail=%d 日志=%s\n', total, okCnt, failCnt, logPath);
if failCnt > 0
    error('GATE:T008B: 存在失败运行，数据不完整（详见 %s）', logPath);
end
