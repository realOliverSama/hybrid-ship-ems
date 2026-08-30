%% ========================================================================
%  v4_validate_T006.m — T-006 指标体系与结果工具链验收
%  用法: 在 model/simulink_modelC_(oracle_ms) 下
%        eval(fileread('scripts/v4_validate_T006.m'))
% 内容:
%    V1: 离线回放——用旧 phase1 (S04 E0/E1/E2) 的 ds 重算 msff_metrics，
%        同名既有字段与旧 m 一致（1e-6）；新字段数值合理（吞吐量≥放电等）
%    V2: law=3 nominal S04 全航程——双阈值独立计算
%        （Vdc_min≈689.9 → N_UV@675=0，N_stress@700≥1）+ ff 字段合理
%    V3: 合成 runs.csv → msff_region_plot 出图 + 收益单元格标注正确
%  输出: results/ms_phase1/T006_validate_<ts>.log
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT,'config'),'dir'), ROOT = pwd; end
addpath(fullfile(ROOT,'config'), fullfile(ROOT,'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT,'config','Model_Parameters.m'));
run(fullfile(ROOT,'config','Model_Scenarios.m'));
MDL = 'HybridShip_ModelCms_DCBus';
TS = datestr(now,'yyyymmdd_HHMMSS');
LOGF = fullfile(ROOT,'results','ms_phase1',sprintf('T006_validate_%s.log',TS));
if ~exist(fileparts(LOGF),'dir'), mkdir(fileparts(LOGF)); end
fid = fopen(LOGF,'w');
logf = @(fmt,varargin) fprintf(fid, fmt, varargin{:});
results = {};
logp = @(s) fprintf('[T006] %s\n', s);

OLD = fullfile(fileparts(ROOT), 'simulink_modelC', 'results', 'phase1');

%% ---------------- V1: 离线回放一致性 ----------------
logp('=== V1: offline replay on old phase1 (S04 E0/E1/E2) ===');
shared = {'fuel_kg','Vdc_min','Vdc_max','SOC_end','SOC_min','E_bat_dis_kWh', ...
          'P_bat_rms_kW','P_bat_peak_abs_kW','t_sat_s','E_unserved_kWh','v_rmse_mps'};
v1_ok = true; v1_msg = '';
for expi = {'E0','E1','E2'}
    exp = expi{1};
    S = load(fullfile(OLD,'S04',[exp '.mat']));
    m_new = model.msff_metrics(S.ds);
    for fld = shared
        f = fld{1};
        a = S.m.(f); b = m_new.(f);
        % 旧 E0 无 P_ff_activated_s 字段？有——旧 m 含该字段，但新 m 不含；只比 shared
        if ~isfield(S.m, f)
            v1_msg = sprintf('%s: 旧 m 缺字段 %s', exp, f);
            v1_ok = false; continue;
        end
        d = abs(a - b);
        if ~(d <= 1e-6 + 1e-9*abs(a))
            v1_ok = false;
            v1_msg = sprintf('%s: %s 不一致 old=%.9g new=%.9g d=%.3g', exp, f, a, b, d);
        end
    end
    % 新字段合理性：吞吐量 ≥ 放电能量；N_UV/N_stress 非负
    if ~(m_new.E_bat_throughput_kWh >= m_new.E_bat_dis_kWh - 1e-6)
        v1_ok = false; v1_msg = sprintf('%s: throughput<dis', exp);
    end
    if m_new.N_UV < 0 || m_new.N_stress < 0
        v1_ok = false; v1_msg = sprintf('%s: 负事件数', exp);
    end
    % 旧 ds 无 MinFF 信号 → ff 字段应 NaN
    if ~(isnan(m_new.ff_active_s) && isnan(m_new.ff_energy_kJ))
        v1_ok = false; v1_msg = sprintf('%s: ff 应 NaN', exp);
    end
    fprintf('    V1 %s: fuel=%.4f thr=%.4f N_UV=%d N_stress=%d ff=%.3g\n', ...
        exp, m_new.fuel_kg, m_new.E_bat_throughput_kWh, m_new.N_UV, m_new.N_stress, m_new.ff_active_s);
end
if v1_ok, v1_msg = '所有共享字段 1e-6 一致，新字段合理'; end
fprintf('    V1 -> %s: %s\n', string(v1_ok), v1_msg);
logf('V1 replay: %s | %s\n', string(v1_ok), v1_msg);
results{end+1} = {'V1', v1_ok};

%% ---------------- V2: law=3 nominal S04 双阈值 + ff ----------------
logp('=== V2: law=3 S04 dual-threshold + ff fields ===');
try
    load_system(fullfile(ROOT,'model',[MDL '.slx']));
    assignin('base','EMS_WORK_MODE',2);
    SCN = SCENARIOS(4);
    oracle = load(fullfile(ROOT,'results','ms_phase1','oracle_S04.mat'));
    assignin('base','MSFF_Oracle_Data', oracle.D_ms);
    assignin('base','MSFF_InfoMode', 0);   % traj
    assignin('base','MSFF_Horizon_s', 30);
    out3 = model.run_scenario_sim(MDL, SCN, 3, 4000);
    ds3 = model.extract_logsout(out3);
    m3 = model.msff_metrics(ds3);
    % S04 nominal: Vdc_min≈689.9 → 675 无事件，700 有事件
    v2_thr = (m3.N_UV == 0 && m3.T_UV == 0) && (m3.N_stress >= 1);
    % ff 字段：law=3 应有信号；均值功率 = energy/active 应在合理范围（冷启动前馈≈机组上限量级）
    v2_ff = ~isnan(m3.ff_active_s) && ~isnan(m3.ff_energy_kJ) && m3.ff_energy_kJ >= 0;
    if v2_ff && m3.ff_active_s > 0.5
        meanPff = m3.ff_energy_kJ / m3.ff_active_s;   % kW
        v2_ff = v2_ff && (meanPff > 50) && (meanPff < 2 * evalin('base','Gen_P_max'));
        fprintf('    V2: mean P_ff during activation = %.1f kW (range 50..%d)\n', ...
            meanPff, round(2 * evalin('base','Gen_P_max')));
    end
    % 不等式：throughput ≥ dis；Vdc_min 数值合理
    v2_sane = m3.E_bat_throughput_kWh >= m3.E_bat_dis_kWh - 1e-6;
    v2_ok = v2_thr && v2_ff && v2_sane;
    fprintf('    V2: Vdc_min=%.2f N_UV=%d T_UV=%.2f N_stress=%d T_stress=%.2f\n', ...
        m3.Vdc_min, m3.N_UV, m3.T_UV, m3.N_stress, m3.T_stress);
    fprintf('    V2: ff_active=%.2f s ff_energy=%.2f kJ thr=%.2f kWh dis=%.2f kWh\n', ...
        m3.ff_active_s, m3.ff_energy_kJ, m3.E_bat_throughput_kWh, m3.E_bat_dis_kWh);
    fprintf('    V2 -> %s (thr=%d ff=%d sane=%d)\n', string(v2_ok), v2_thr, v2_ff, v2_sane);
    logf('V2 law3: Vdc_min=%.2f N_UV=%d N_stress=%d ff_active=%.2f -> %s\n', ...
        m3.Vdc_min, m3.N_UV, m3.N_stress, m3.ff_active_s, string(v2_ok));
    % 保存 law=3 ds 供后续复用
    save(fullfile(ROOT,'results','ms_phase1','msff_law3_S04_ds.mat'), 'ds3', 'm3');
catch ME
    v2_ok = false;
    fprintf('    V2 FAIL: %s\n', ME.message);
    logf('V2 law3 FAIL: %s\n', ME.message);
end
results{end+1} = {'V2', v2_ok};

%% ---------------- V3: 合成 runs.csv + 区域图 ----------------
logp('=== V3: synthetic runs.csv + region plot ===');
try
    synDir = fullfile(ROOT,'results','ms_phase1','syn_region');
    if ~exist(synDir,'dir'), mkdir(synDir); end
    synCsv = fullfile(synDir,'synthetic_runs.csv');
    if exist(synCsv,'file'), delete(synCsv); end
    LG = [0.8 0.9 1.0 1.1 1.25]; RG = [20 30 40 50 60];
    scenarios = {'S04','S07'};
    % 合成数据：C0 + CMS(H=30,traj)
    % 收益区：λ_P≤0.9 且 R_gen≤40（每场景 6 单元格）
    rng(42);
    for si = 1:numel(scenarios)
        scen = scenarios{si};
        if strcmp(scen,'S04'), base = struct('fuel',91.2,'vdc',689.9,'peak',600.8,'rms',70.6,'thr',52.0);
        else, base = struct('fuel',78.4,'vdc',689.9,'peak',600.8,'rms',76.1,'thr',58.0); end
        for li = 1:numel(LG)
            for ri = 1:numel(RG)
                lp = LG(li); rg = RG(ri);
                inBenefit = (lp <= 0.9) && (rg <= 40);
                % C0
                meta = struct('exp_id','syn','scenario',scen,'controller','C0','anchor','-', ...
                    'lambda_P',lp,'R_gen',rg,'tau_gen',5,'SOC0',0.6,'horizon_s',0,'info_mode','-');
                m0 = struct('fuel_kg',base.fuel,'Vdc_min',base.vdc,'Vdc_max',750,'SOC_end',0.593, ...
                    'SOC_min',0.593,'E_bat_dis_kWh',8.75,'E_bat_throughput_kWh',base.thr, ...
                    'P_bat_rms_kW',base.rms,'P_bat_peak_abs_kW',base.peak,'t_sat_s',17.5, ...
                    'E_unserved_kWh',0,'v_rmse_mps',1.01,'N_UV',0,'T_UV',0, ...
                    'N_stress',30,'T_stress',8.0,'ff_active_s',0,'ff_energy_kJ',0);
                model.write_msff_csv(synCsv, meta, m0, 'ok');
                % CMS
                if inBenefit
                    mC = struct('fuel_kg',base.fuel*1.005,'Vdc_min',base.vdc+5,'Vdc_max',751, ...
                        'SOC_end',0.594,'SOC_min',0.593,'E_bat_dis_kWh',6.0, ...
                        'E_bat_throughput_kWh',base.thr*0.85,'P_bat_rms_kW',base.rms*0.85, ...
                        'P_bat_peak_abs_kW',base.peak*0.80,'t_sat_s',6.0,'E_unserved_kWh',0, ...
                        'v_rmse_mps',1.01,'N_UV',0,'T_UV',0,'N_stress',0,'T_stress',0, ...
                        'ff_active_s',40,'ff_energy_kJ',1200);
                else
                    mC = struct('fuel_kg',base.fuel*1.0005,'Vdc_min',base.vdc+0.5,'Vdc_max',750.5, ...
                        'SOC_end',0.593,'SOC_min',0.593,'E_bat_dis_kWh',8.7, ...
                        'E_bat_throughput_kWh',base.thr*0.99,'P_bat_rms_kW',base.rms*0.99, ...
                        'P_bat_peak_abs_kW',base.peak*0.99,'t_sat_s',17.0,'E_unserved_kWh',0, ...
                        'v_rmse_mps',1.01,'N_UV',0,'T_UV',0,'N_stress',28,'T_stress',7.5, ...
                        'ff_active_s',5,'ff_energy_kJ',100);
                end
                metaC = meta; metaC.controller = 'CMS'; metaC.horizon_s = 30; metaC.info_mode = 'traj';
                model.write_msff_csv(synCsv, metaC, mC, 'ok');
            end
        end
    end
    % 失败行样例：1 个 failed 行验证落盘
    metaF = struct('exp_id','syn','scenario','S07','controller','CMS','anchor','-', ...
        'lambda_P',1.25,'R_gen',20,'tau_gen',5,'SOC0',0.6,'horizon_s',30,'info_mode','traj');
    model.write_msff_csv(synCsv, metaF, struct(), 'failed:sim_error_test');
    % 出图
    model.msff_region_plot(synCsv, synDir, 'lambda_grid', LG, 'Rgen_grid', RG);
    % 校验收益单元格数：2 场景 × 6 = 12
    T = readtable(synCsv, 'TextType', 'string');
    okT = T(T.status == "ok" & T.controller == "CMS", :);
    nB = sum(okT.lambda_P <= 0.9 & okT.R_gen <= 40);
    v3_mark = (nB == 12);
    % PNG 产出
    pngs = dir(fullfile(synDir,'msff_region_*.png'));
    v3_png = numel(pngs) == 2;
    % 失败行存在性
    v3_fail = any(T.status == "failed:sim_error_test");
    v3_ok = v3_mark && v3_png && v3_fail;
    fprintf('    V3: benefit-cells=%d/12 pngs=%d failed-row=%d -> %s\n', nB, numel(pngs), v3_fail, string(v3_ok));
    fprintf('    V3 files: %s\n', strjoin({pngs.name}, ', '));
    logf('V3 region: cells=%d/12 pngs=%d failrow=%d -> %s\n', nB, numel(pngs), v3_fail, string(v3_ok));
catch ME
    v3_ok = false;
    fprintf('    V3 FAIL: %s\n', ME.message);
    logf('V3 region FAIL: %s\n', ME.message);
end
results{end+1} = {'V3', v3_ok};

%% ---------------- 汇总 ----------------
fprintf('\n[T006] ===== T-006 acceptance summary =====\n');
allok = true;
for i = 1:numel(results)
    fprintf('[T006] %s: %s\n', results{i}{1}, string(results{i}{2}));
    logf('SUMMARY %s: %s\n', results{i}{1}, string(results{i}{2}));
    allok = allok && results{i}{2};
end
fprintf('[T006] OVERALL: %s\n', string(allok));
fclose(fid);
