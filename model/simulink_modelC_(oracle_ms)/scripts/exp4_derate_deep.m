%% exp4_derate_deep.m — 更大缺额：电池降额限 300 kW → 强制 Vdc<675 触发降额
ROOT = fileparts(fileparts(mfilename('fullpath')));
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'Model_Parameters.m'));
run(fullfile(ROOT, 'config', 'Model_Scenarios.m'));
md = fullfile(ROOT, 'results', 'ms_phase1'); if ~exist(md,'dir'), mkdir(md); end

MDL = 'HybridShip_ModelCms_DCBus';
load_system(fullfile(ROOT, 'model', [MDL '.slx']));
SCN = SCENARIOS(9);
stop = SCN.voyage_distance_m / SCN.target_speed_mps + 1200;

combos = { ...
  560, 5,  300; ...
  1000, 40, 300; ...
  560, 20, 300; ...
};
fprintf('%-28s %8s %8s %8s %6s\n', 'cfg', 'Vdc_min', 'lim_min', 'unserved_s', 'n_der');
for c = 1:size(combos,1)
    GP = combos{c,1}; RG = combos{c,2}; BL = combos{c,3};
    assignin('base', 'Gen_P_max', GP);
    assignin('base', 'Gen_Ramp_Up', RG);
    assignin('base', 'Gen_Ramp_Down', RG);
    assignin('base', 'ASSUMED_Bat_DischargeLimit', BL);
    try
        out = model.run_scenario_sim(MDL, SCN, 0, stop);
        ds = model.extract_logsout(out);
        t = ds.t; k200 = find(t<=200,1,'last');
        Vdc = ds.V_dc_V(1:k200); lim = ds.P_prop_limit_kW(1:k200);
        us = ds.P_unserved_kW(1:k200);
        fprintf('GP=%3d RG=%2d BL=%3d %10.1f %9.1f %9.0f %7d\n', ...
            GP, RG, BL, min(Vdc), min(lim), sum(us>0.01), sum(lim<699.5));
        if min(Vdc) < 675 && ~exist(fullfile(md,'_exp4_derate_active.mat'),'file')
            save(fullfile(md,'_exp4_derate_active.mat'), 'ds');
            fprintf('  saved _exp4_derate_active.mat\n');
        end
    catch err
        fprintf('GP=%3d RG=%2d BL=%3d FAILED: %s\n', GP, RG, BL, err.message);
    end
end
close_system(MDL, 0);
