%% ========================================================================
%  09_run_modelB_comparison.m — Model B 上 Baseline vs AI 四工况对比（T-009）
%  工况: S01 静水 / S04 阻力渐增 / S07 桥区机动 / S10 水流反转
%  输出: reports/ModelB_comparison_report.md + results/modelB_compare/<scen>/*.png
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));

% 加载 MLP 权重（source=3 环内推理）
if ~model.load_mlp_weights(ROOT)
    error('未找到 mlp_weights.mat，请先运行 python/train_load_predictor.py');
end

MDL = 'HybridShip_ModelB_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
assignin('base', 'EMS_WORK_MODE', 2);   % 全自动模式对比

outRoot = fullfile(ROOT, 'results', 'modelB_compare');
if ~exist(outRoot, 'dir'), mkdir(outRoot); end

testScen = [1, 4, 7, 10];
RES = {};
for s = testScen
    SCN = SCENARIOS(s);
    for mode = 0:1
        assignin('base', 'AI_Pred_Source', double(mode == 1) * 3);
        out = model.run_scenario_sim(MDL, SCN, mode, 5000 / 5.66 + 1800);
        ds = model.extract_logsout(out);
        mm = model.dcbus_metrics(ds);
        fprintf('[09] %s EMS_MODE=%d: fuel=%.2f kg, FEC=%.2f kg, dU_max=%.1f V, t_rec=%.2f s, SOC_end=%.3f\n', ...
            SCN.name, mode, mm.fuel_kg, mm.FEC_kg, mm.dU_max, mm.t_recovery_s, mm.SOC_end);
        RES{end+1} = struct('scen', SCN.name, 'sid', s, 'mode', mode, 'ds', ds, 'm', mm, 'SCN', SCN); %#ok<AGROW>
    end
end
assignin('base', 'AI_Pred_Source', 0);

% ---- 出图 ----
for s = testScen
    iB = find(cellfun(@(r) r.sid == s && r.mode == 0, RES));
    iA = find(cellfun(@(r) r.sid == s && r.mode == 1, RES));
    sdir = fullfile(outRoot, sprintf('S%02d', s));
    if ~exist(sdir, 'dir'), mkdir(sdir); end
    model.modelb_plot(RES{iB}, RES{iA}, sdir);
end

% ---- 报告 ----
fid = fopen(fullfile(ROOT, 'reports', 'ModelB_comparison_report.md'), 'w', 'n', 'UTF-8');
W = @(s) fprintf(fid, '%s\n', s);
W('# Model B（直流母线电气层）Baseline vs AI-Enhanced EMS 对比报告');
W('');
W('## 1. 实验设置');
W('- 模型: `model/HybridShip_ModelB_DCBus.slx`（Model A 功率流 + 聚合单母线 750 V 电气层，');
W('  电池 DC/DC 下垂稳压 K_droop=10 kW/V，CCS 2026 电能质量判据：稳态 675~795 V / 瞬态 600~802.5 V / t_rec≤1.5 s）');
W('- EMS_WORK_MODE=2（全自动）；AI 支路为 MLP 环内推理（窗口 22 s，AI_Pred_Source=3）');
W('- 下垂系数选择依据：K_droop=5 时起步段 V_min=660 V 越出稳态带且 t_rec=7.25 s 超 1.5 s；');
W('  K_droop=10/20 均满足 CCS；取 10 以兼顾电池瞬态应力（扫描数据：results/droop_sweep.mat）');
W('- FEC 等效燃油 = fuel + (0.60−SOC_end)×1200×k_eq，k_eq≈0.499 kg/kWh（ECMS 折算，ASSUMED）');
W('');
W('## 2. 指标卡（完整航次 5000 m）');
W('');
W('| 工况 | 模式 | ΔU_max [V] | t_rec [s] | 稳态越限 [s] | P_margin_min [kW] | SOC_min | SOC_end | P_bat_max [kW] | fuel [kg] | FEC [kg] | v_rmse [m/s] |');
W('|---|---|---|---|---|---|---|---|---|---|---|---|');
for i = 1:numel(RES)
    r = RES{i}; mm = r.m;
    if r.mode == 0, md = 'Baseline'; else, md = 'AI'; end
    fprintf(fid, '| %s | %s | %.1f | %.2f | %.1f | %.0f | %.3f | %.3f | %.0f | %.2f | %.2f | %.4f |\n', ...
        r.scen, md, mm.dU_max, mm.t_recovery_s, mm.t_steady_viol_s, mm.P_margin_min_kW, ...
        mm.SOC_min, mm.SOC_end, mm.P_bat_max_kW, mm.fuel_kg, mm.FEC_kg, mm.v_rmse);
end
W('');
W('## 3. 结果解读');
for s = testScen
    iB = find(cellfun(@(r) r.sid == s && r.mode == 0, RES));
    iA = find(cellfun(@(r) r.sid == s && r.mode == 1, RES));
    b = RES{iB}.m; a = RES{iA}.m;
    W(sprintf('- **%s**：电池峰值放电 %.0f→%.0f kW，SOC 末值 %.3f→%.3f，ΔU_max %.1f→%.1f V，FEC %.2f→%.2f kg。', ...
        RES{iB}.scen, b.P_bat_max_kW, a.P_bat_max_kW, b.SOC_end, a.SOC_end, b.dU_max, a.dU_max, b.FEC_kg, a.FEC_kg));
end
W('');
W('## 4. 与实船/CCS 架构的差异声明');
W('- 本模型为聚合单母线等效（对称双支路合并）；CCS 2026 要求母排≥2 分段，分段故障隔离/母联重构不在本期范围；');
W('- 电压动态为平均值模型（无 PWM 纹波）；断路器/保护动作不建模（只监视不切除）；');
W('- 所有电气参数（C_bus、K_droop、效率）为 ASSUMED 预置值，待设计方核定。');
W('');
W('---');
W('*由 scripts/09_run_modelB_comparison.m 生成*');
fclose(fid);
fprintf('[09] 报告: %s\n', fullfile(ROOT, 'reports', 'ModelB_comparison_report.md'));
