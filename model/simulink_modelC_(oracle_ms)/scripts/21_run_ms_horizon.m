%% ========================================================================
%  21_run_ms_horizon.m — Phase 1A: horizon × 锚点 × 三对照（T-007, FR-010/013A）
%  Spec: multistep-predictive-ems
%  T-004R 重写：全部 per-run 变量经 Simulink.SimulationInput.setVariable 注入
%  （model.run_ms_sim），不再依赖 base workspace 变更检测（T-007 根因修复）。
%  实验矩阵（固定）:
%    锚点:   Fast(R=60,τ=2) / Nominal(R=40,τ=5) / Slow(R=20,τ=10)；λ_P=1.25 (GP=1000)
%    场景:   S04（渐进代表）/ S07（动态代表）
%    对照:   C0 (law=0, horizon 无关) / C_P (law=3, peak) / C_MS (law=3, traj)
%    horizon: {10,15,20,30,45,60} s
%    总 run = 3×2×(1 + 6 + 6) = 78
%  硬门禁: Nominal×S04 C0 与冻结基线 S04,E0 全字段相对差 < 1e-6，FAIL 立即中止。
%  失败纪律: 单 run 失败落盘 NaN 行 + 原因，不重试伪装成功；连续 3 次失败中断。
%  输出:
%    results/ms_phase1/phase1A/runs.csv          78 行全量
%    results/ms_phase1/phase1A/ds_<exp_id>.mat  每 run 的 ds
%    results/ms_phase1/phase1A/summary_1A.csv   每组合汇总 + Δ%
%    results/ms_phase1/phase1A/run_1A_<ts>.log  执行日志
%  执行: cd model/simulink_modelC_(oracle_ms) && matlab -batch "eval(fileread('scripts/21_run_ms_horizon.m'))"
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
model.load_mlp_weights(ROOT);   % 与 12/20c 一致，双保险
run(fullfile(ROOT,'config','Model_Scenarios.m'));

MDL = 'HybridShip_ModelCms_DCBus';
FROZEN_METRICS = fullfile(fileparts(ROOT), 'simulink_modelC', 'results', 'phase1', 'S04', 'E0.mat');

% 最小批次（T-004R 验收用）：V1A_MINI=1 时只跑 Nominal×S04 的 C0+CP H30+CMS H30
MINI = strcmp(getenv('V1A_MINI'), '1');

outDir = fullfile(ROOT,'results','ms_phase1','phase1A');
if ~exist(outDir,'dir'), mkdir(outDir); end
csvPath = fullfile(outDir,'runs.csv');
if exist(csvPath,'file'), delete(csvPath); end   % 每次重跑从空表开始，避免陈旧行
TS = datestr(now,'yyyymmdd_HHMMSS');
logPath = fullfile(outDir,sprintf('run_1A_%s.log',TS));
lof = fopen(logPath,'w');

load_system(fullfile(ROOT,'model',[MDL '.slx']));
assignin('base','EMS_WORK_MODE',2);

% ---------------- 实验矩阵 ----------------
anchors = struct( ...
    'fast',    struct('GP_kW',1000,'R_gen',60,'tau_gen',2), ...
    'nominal', struct('GP_kW',1000,'R_gen',40,'tau_gen',5), ...
    'slow',    struct('GP_kW',1000,'R_gen',20,'tau_gen',10));
if MINI
    anchorOrder = {'nominal'};
    scenList = [4];
    horizons = [30];
else
    anchorOrder = {'nominal','fast','slow'};   % Nominal 先行（含回归门禁）
    scenList = [4 7];
    horizons = [10 15 20 30 45 60];
end
stopTimeLimit = 5000/5.66 + 1800;

% 冻结回归参考（S04,E0 全精度）
F = load(FROZEN_METRICS);
FREF = F.m;
REGFIELDS = {'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min','E_bat_dis_kWh', ...
             'P_bat_rms_kW','P_bat_peak_abs_kW','t_sat_s','E_unserved_kWh','v_rmse_mps'};

total = 0; okCnt = 0; failCnt = 0; consecFail = 0; regPass = false;

for ai = 1:numel(anchorOrder)
    an = anchorOrder{ai};
    A = anchors.(an);
    genEff = struct('Gen_P_max', A.GP_kW, 'Gen_Ramp_Up', A.R_gen, ...
        'Gen_Ramp_Down', A.R_gen, 'tau_gen', A.tau_gen);

    for si = 1:numel(scenList)
        sc = scenList(si);
        SCN = SCENARIOS(sc);
        sname = sprintf('S%02d', sc);
        orc = load(fullfile(ROOT,'results','ms_phase1',sprintf('oracle_%s.mat',sname)));
        D = orc.D_ms;

        % ---- C0（law=0，与 horizon 无关）----
        ctrl = 'C0'; law = 0; H = 0; infoMode = '-';
        expID = sprintf('1A_%s_%s_C0_H00', an, sname);
        meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
            'anchor',an, 'lambda_P',A.GP_kW/800, 'R_gen',A.R_gen, 'tau_gen',A.tau_gen, ...
            'SOC0',SCN.initial_SOC, 'horizon_s',H, 'info_mode',infoMode);
        t0 = tic;
        try
            out = model.run_ms_sim(MDL, SCN, law, 0, 0, D, stopTimeLimit, genEff);

            ds = model.extract_logsout(out);
            m = model.msff_metrics(ds);
            wall = toc(t0);
            save(fullfile(outDir,['ds_' expID '.mat']), 'ds', 'm', 'meta');
            model.write_msff_csv(csvPath, meta, m, 'ok');
            okCnt = okCnt + 1; consecFail = 0; total = total + 1;
            fprintf('[1A] %-26s %s law=%d: fuel=%.2f Vdc_min=%.1f Ppk=%.0f SOC_end=%.4f wall=%.1fs\n', ...
                expID, SCN.name, law, m.fuel_kg, m.Vdc_min, m.P_bat_peak_abs_kW, m.SOC_end, wall);
            fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f wall=%.1f\n', ...
                expID, m.fuel_kg, m.Vdc_min, m.SOC_end, wall);
            % ---- Nominal×S04 C0 回归门禁 ----
            if strcmp(an,'nominal') && sc == 4 && ~regPass
                maxrel = 0; worst = '';
                for k = 1:numel(REGFIELDS)
                    f = REGFIELDS{k};
                    d = abs(FREF.(f)-m.(f)) / max(abs(FREF.(f)),1e-12);
                    if d > maxrel, maxrel = d; worst = f; end
                end
                regPass = maxrel < 1e-6;
                fprintf('[1A] 回归 Nominal×S04 C0: max-rel-diff=%.3g (worst=%s) -> %s\n', ...
                    maxrel, worst, string(regPass));
                fprintf(lof, 'REGRESSION max-rel-diff=%.3g (worst=%s) -> %s\n', maxrel, worst, string(regPass));
                if ~regPass
                    fprintf('[1A] **** 回归 FAIL: 与冻结 S04,E0 不一致 (worst=%s) ****\n', worst);
                    error('GATE:1A: Nominal×S04 C0 回归失败 (worst=%s maxrel=%.3g)', worst, maxrel);
                end
            end
        catch err
            wall = toc(t0);
            total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
            fprintf('[1A] %-26s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
            fprintf(lof, '%s FAIL: %s\n', expID, err.message);
            model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
            if consecFail >= 3
                fprintf('[1A] **** 连续 3 次失败，中止 ****\n');
                error('GATE:1A: 连续 3 次失败 (详见 %s)', logPath);
            end
        end

        % ---- C_P 与 C_MS（law=3）----
        pairList = {'CP','peak'; 'CMS','traj'};
        for h = horizons
            hh = sprintf('H%02d', h);
            for pairIdx = 1:size(pairList,1)
                ctrl = pairList{pairIdx,1}; infoMode = pairList{pairIdx,2};
                expID = sprintf('1A_%s_%s_%s_%s', an, sname, ctrl, hh);
                meta = struct('exp_id',expID, 'scenario',SCN.name, 'controller',ctrl, ...
                    'anchor',an, 'lambda_P',A.GP_kW/800, 'R_gen',A.R_gen, 'tau_gen',A.tau_gen, ...
                    'SOC0',SCN.initial_SOC, 'horizon_s',h, 'info_mode',infoMode);
                t0 = tic;
                try
                    out = model.run_ms_sim(MDL, SCN, 3, h, double(strcmp(infoMode,'peak')), D, stopTimeLimit, genEff);
                    ds = model.extract_logsout(out);
                    m = model.msff_metrics(ds);
                    wall = toc(t0);
                    save(fullfile(outDir,['ds_' expID '.mat']), 'ds', 'm', 'meta');
                    model.write_msff_csv(csvPath, meta, m, 'ok');
                    okCnt = okCnt + 1; consecFail = 0; total = total + 1;
                    fprintf('[1A] %-26s %s law=3 H=%3d %-4s: fuel=%.2f Vdc_min=%.1f Ppk=%.0f SOC_end=%.4f wall=%.1fs\n', ...
                        expID, SCN.name, h, infoMode, m.fuel_kg, m.Vdc_min, m.P_bat_peak_abs_kW, m.SOC_end, wall);
                    fprintf(lof, '%s OK fuel=%.6f Vdc_min=%.4f SOC_end=%.6f wall=%.1f\n', ...
                        expID, m.fuel_kg, m.Vdc_min, m.SOC_end, wall);
                catch err
                    wall = toc(t0);
                    total = total + 1; failCnt = failCnt + 1; consecFail = consecFail + 1;
                    fprintf('[1A] %-26s FAIL: %s (wall=%.1fs)\n', expID, err.message, wall);
                    fprintf(lof, '%s FAIL: %s\n', expID, err.message);
                    model.write_msff_csv(csvPath, meta, struct(), sprintf('failed:%s', strrep(err.message, ',', ';')));
                    if consecFail >= 3
                        fprintf('[1A] **** 连续 3 次失败，中止 ****\n');
                        error('GATE:1A: 连续 3 次失败 (详见 %s)', logPath);
                    end
                end
            end
        end
    end
end

fclose(lof);

% ---------------- 汇总表 ----------------
if exist(csvPath,'file')
    T = readtable(csvPath, 'TextType','string');
    out = cell(0,23);
    anchorsU = unique(T.anchor); scensU = unique(T.scenario);
    for ai = 1:numel(anchorsU)
        for si = 1:numel(scensU)
            sub = T(strcmp(T.anchor,anchorsU(ai)) & strcmp(T.scenario,scensU(si)), :);
            c0row = sub(strcmp(sub.controller,'C0'), :);
            if isempty(c0row) || isnan(c0row.fuel_kg(1)), continue; end
            hU = unique(sub.horizon_s(sub.controller~='C0'));
            for hi = 1:numel(hU)
                h = hU(hi);
                cpr = sub(strcmp(sub.controller,'CP')  & sub.horizon_s==h, :);
                cmr = sub(strcmp(sub.controller,'CMS') & sub.horizon_s==h, :);
                if isempty(cpr) || isempty(cmr), continue; end
                gv = @(r,f) r.(f)(1);   % 每组合必有行（成功或 NaN 行）
                bf  = gv(c0row,'fuel_kg'); bpk = gv(c0row,'P_bat_peak_abs_kW'); brm = gv(c0row,'P_bat_rms_kW'); bv = gv(c0row,'Vdc_min');
                pct = @(r,f) (gv(r,f) - gv(c0row,f)) / gv(c0row,f) * 100;
                out(end+1,:) = {anchorsU(ai), scensU(si), h, ...
                    bf, gv(cpr,'fuel_kg'), gv(cmr,'fuel_kg'), pct(cpr,'fuel_kg'), pct(cmr,'fuel_kg'), ...
                    bv, gv(cpr,'Vdc_min'), gv(cmr,'Vdc_min'), pct(cpr,'Vdc_min'), pct(cmr,'Vdc_min'), ...
                    bpk, gv(cpr,'P_bat_peak_abs_kW'), gv(cmr,'P_bat_peak_abs_kW'), pct(cpr,'P_bat_peak_abs_kW'), pct(cmr,'P_bat_peak_abs_kW'), ...
                    brm, gv(cpr,'P_bat_rms_kW'), gv(cmr,'P_bat_rms_kW'), pct(cpr,'P_bat_rms_kW'), pct(cmr,'P_bat_rms_kW')}; %#ok<AGROW>
            end
        end
    end
    if ~isempty(out)
        hdr = {'anchor','scenario','horizon_s', ...
               'C0_fuel','CP_fuel','CMS_fuel','dCP_fuel_pct','dCMS_fuel_pct', ...
               'C0_Vdc_min','CP_Vdc_min','CMS_Vdc_min','dCP_Vdcmin_pct','dCMS_Vdcmin_pct', ...
               'C0_Pbat_peak','CP_Pbat_peak','CMS_Pbat_peak','dCP_Ppk_pct','dCMS_Ppk_pct', ...
               'C0_Pbat_rms','CP_Pbat_rms','CMS_Pbat_rms','dCP_Prms_pct','dCMS_Prms_pct'};
        T2 = cell2table(out, 'VariableNames', hdr);
        writetable(T2, fullfile(outDir,'summary_1A.csv'));
        fprintf('[1A] 汇总已写: %d 行\n', height(T2));
    end
end

fprintf('[1A] 完成: 总=%d ok=%d fail=%d 回归=%s 日志=%s\n', total, okCnt, failCnt, string(regPass), logPath);
if ~regPass
    error('GATE:1A: Nominal 回归未通过，实验数据不可信');
end
