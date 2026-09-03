function prof = load_profile(ROOT, scnName)
% ecms.load_profile — 加载 Phase 0 负荷剖面 .mat（T-006/T-008，design §1）
%   results/ecms_phase0/profiles/<scnName>.mat（顶层字段展开）
% 输入: ROOT 工程根目录；scnName 场景名（如 'S01_still_water'）
% 输出: prof 结构体（t/P_demand_kW/P_gen_kW/P_bat_kW/SOC/V_dc_V/v_ship_mps/x_ship_m/
%       fuel_base_tier*_kg/energy_closure/meta 等，字段见 30_export_load_profiles.m）
% 供: DP 求解器（T-008）与不变性验证（T-007）复用。
    f = fullfile(ROOT, 'results', 'ecms_phase0', 'profiles', [scnName '.mat']);
    if ~exist(f, 'file')
        error('ECMS:PROFILE', '剖面文件不存在: %s', f);
    end
    prof = load(f);
end
