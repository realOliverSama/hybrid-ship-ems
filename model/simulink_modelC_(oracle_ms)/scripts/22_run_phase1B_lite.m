%% ========================================================================
%  22_run_phase1B_lite.m — T-009-lite 稀疏参数域确认（64 次仿真）
%  Spec: multistep-predictive-ems（用户裁决 2026-08-21：放弃 472 次全量，
%        改为稀疏域确认——目标是验证"合理参数域内有效区域为空"）
%  固定矩阵（主 Agent 已定，不得更改）:
%    λ_P ∈ {0.8, 0.9, 1.0, 1.25} × R_gen ∈ {20, 30, 40, 60} = 16 点
%    τ_gen = 10 s 固定（最弱惯性端）；SOC0 = 0.60；H = 45 s 固定；
%    infoMode = 0（traj, CMS）；场景 S07 + S09；每点 C0+CMS 成对
%    genEff = struct('Gen_P_max', λ_P*800, 'Gen_Ramp_Up', R_gen,
%                    'Gen_Ramp_Down', R_gen, 'tau_gen', 10)
%  输出:
%    results/ms_phase1/phase1B_lite/ds_lite_<lP>_<Rg>_<scn>_<ctrl>.mat
%    results/ms_phase1/phase1B_lite/runs.csv（write_msff_csv 契约，anchor='lite_tau10'）
%    results/ms_phase1/phase1B_lite/run_lite_<ts>.log
%  失败纪律: 失败落盘 NaN 行 + 原因；连续 3 次失败中止。
%  本脚本只执行与采集；"区域为空"结论由主 Agent 判定（22b 只陈述事实）。
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
model.load_mlp_weights(ROOT);
run(fullfile(ROOT,'config','Model_Scenarios.m'));

MDL = 'HybridShip_ModelCms_DCBus';
outDir = fullfile(ROOT,'results','ms_phase1','phase1B_lite');
if ~exist(outDir,'dir'), mkdir(outDir); end
csvPath = fullfile(outDir,'runs.csv');
if exist(csvPath,'file'), delete(csvPath); end
TS = datestr(now,'yyyymmdd_HHMMSS');
logPath = fullfile(outDir,sprintf('run_lite_%s.log',TS));
lof = fopen(logPath,'w');

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

% ---------------- 稀疏网格（16 点） ----------------
lambdaP = [0.8 0.9 1.0 1.25];
Rgens   = [20 30 40 60];
scenList = [7 9];
H = 45;               % 固定 horizon [s]
stopTimeLimit = 5000/5.66 + 1800;

% 条件补测记录（max|ΔP_gen_actual| > 1 kW 时触发 CMS H60）
h60List = cell(0,2);   % {scn, expIDprefix}

total = 0; okCnt = 0; failCnt = 0; consecFail = 0;
for lp = lambdaP
    for rg = Rgens
        genEff = struct('Gen_P_max', lp*800, 'Gen_Ramp_Up', rg, ...
                        'Gen_Ramp_Down', rg, 'tau_gen', 10);
        for sc = scenList
            SCN = SCENARIOS(sc);
            sname = sprintf('S%02d', sc);
            orc = load(fullfile(ROOT,'results','ms_phase1',sprintf('oracle_%s.mat',sname)));
            D = orc.D_ms;

            % ---- C0（law=0）----
            ctrl = 'C0';
            expID = sprintf('lite_lP%.2f_Rg%02d_%s_C0', lp, rg, sname);
            meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
                'anchor','lite_tau10', 'lambda_P',lp, 'R_gen',rg, 'tau_gen',10, ...
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
                fprintf('[L] %-34s law=0: fuel=%.2f Vdc_min=%.1f SOC_end=%.4f wall=%.1fs\n', ...
                    expID, m.fuel_kg, m.Vdc_min, m.SOC_end, wall);
                fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f wall=%.1f\n', ...
                    expID, m.fuel_kg, m.Vdc_min, m.SOC_end, wall);
            catch err
                wall = toc(t0);
                total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
                fprintf('[L] %-34s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
                fprintf(lof, '%s FAIL: %s\n', expID, err.message);
                model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
                if consecFail >= 3
                    fprintf('[L] **** 连续 3 次失败，中止 ****\n');
                    error('GATE:LITE: 连续 3 次失败 (详见 %s)', logPath);
                end
            end

            % ---- CMS（law=3, traj, H=45）----
            ctrl = 'CMS';
            expID = sprintf('lite_lP%.2f_Rg%02d_%s_CMS_H45', lp, rg, sname);
            meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
                'anchor','lite_tau10', 'lambda_P',lp, 'R_gen',rg, 'tau_gen',10, ...
                'SOC0',SCN.initial_SOC, 'horizon_s',H, 'info_mode','traj');
            t0 = tic;
            try
                out = model.run_ms_sim(MDL, SCN, 3, H, 0, D, stopTimeLimit, genEff);
                ds = model.extract_logsout(out);
                m = model.msff_metrics(ds);
                wall = toc(t0);
                % 预测有效性检查（escalate 条件：pred_valid 全程 0）
                if isfield(ds,'MSFF_pred_valid') && ~any(ds.MSFF_pred_valid > 0)
                    error('LITE: %s 全程 pred_valid=0，oracle 通道未生效', expID);
                end
                save(fullfile(outDir,['ds_' expID '.mat']), 'ds', 'm', 'meta', 'genEff');
                model.write_msff_csv(csvPath, meta, m, 'ok');
                okCnt = okCnt + 1; consecFail = 0; total = total + 1;
                fprintf('[L] %-34s law=3 H=45: fuel=%.2f Vdc_min=%.1f SOC_end=%.4f ff_act=%.1fs wall=%.1fs\n', ...
                    expID, m.fuel_kg, m.Vdc_min, m.SOC_end, m.ff_active_s, wall);
                fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f ff_act=%.2f wall=%.1f\n', ...
                    expID, m.fuel_kg, m.Vdc_min, m.SOC_end, m.ff_active_s, wall);
            catch err
                wall = toc(t0);
                total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
                fprintf('[L] %-34s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
                fprintf(lof, '%s FAIL: %s\n', expID, err.message);
                model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
                if consecFail >= 3
                    fprintf('[L] **** 连续 3 次失败，中止 ****\n');
                    error('GATE:LITE: 连续 3 次失败 (详见 %s)', logPath);
                end
            end
        end
        if mod(total, 10) == 0 && total > 0
            fprintf('[L] 进度: %d/64 ok=%d fail=%d\n', total, okCnt, failCnt);
        end
    end
end

fclose(lof);
fprintf('[L] 完成: 总=%d ok=%d fail=%d 日志=%s\n', total, okCnt, failCnt, logPath);
if failCnt > 0
    error('GATE:LITE: 存在失败运行，数据不完整（详见 %s）', logPath);
end
