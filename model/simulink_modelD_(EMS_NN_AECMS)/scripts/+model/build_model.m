%% ========================================================================
%  02_build_model.m — 从零重建 HybridShip_Model_MVP.slx（可重复执行）
%  依赖: config/Model_Parameters.m, config/Model_Scenarios.m
%  输出: model/HybridShip_Model_MVP.slx, reports/parameter_audit.csv
%  说明: 删除旧模型后完全重建；构建过程不使用任何 GUI 操作。
% ========================================================================
function build_model(ROOT)
% model.build_model - 从零重建 HybridShip_Model_MVP.slx（可重复执行）
% 依赖: config/Model_Parameters.m, config/Model_Scenarios.m
% 输出: model/HybridShip_Model_MVP.slx, reports/parameter_audit.csv
if nargin < 1
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
% 配置脚本必须在基础工作区执行：模块参数表达式在 base 中求值
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Parameters.m')));
evalin('base', sprintf('run(''%s'')', fullfile(ROOT, 'config', 'Model_Scenarios.m')));

MDL = 'HybridShip_Model_MVP';
mdlPath = fullfile(ROOT, 'model', [MDL '.slx']);
if ~exist(fullfile(ROOT, 'model'), 'dir'), mkdir(fullfile(ROOT, 'model')); end

% ---- 清理旧模型 ----
if bdIsLoaded(MDL), close_system(MDL, 0); end
if exist(mdlPath, 'file'), delete(mdlPath); end

fprintf('[02] 开始重建 %s\n', mdlPath);
try
    %% ================= 1. 新建模型与求解器配置 =================
    new_system(MDL);
    load_system(MDL);
    set_param(MDL, 'SolverType', 'Fixed-step', 'Solver', 'ode3', ...
        'FixedStep', '0.01', 'StopTime', '7200', ...
        'SignalLogging', 'on', 'SignalLoggingName', 'logsout', ...
        'SaveOutput', 'off', 'SaveTime', 'off', 'ReturnWorkspaceOutputs', 'on', ...
        'AlgebraicLoopMsg', 'error', 'UnconnectedInputMsg', 'none', ...
        'UnconnectedOutputMsg', 'none', 'SolverPrmCheckMsg', 'error');

    % EMS 变体构建期间需要 AI 变体（7 端口）处于激活状态
    assignin('base', 'EMS_MODE', 1);
    % 若存在训练产物，则以最佳窗口与真实权重构建（source=3 环内推理自适应）
    if model.load_mlp_weights(ROOT)
        fprintf('[02] 检测到 mlp_weights.mat：MLP_Window=%d（权重与窗口已载入 base）\n', evalin('base', 'MLP_Window'));
    end

    %% ================= 2. 构建 12 个子系统 =================
    pos01 = [40 60 200 150];  pos02 = [260 60 420 150];  pos03 = [480 60 640 150];
    pos04 = [700 60 860 150]; pos05 = [40 220 200 310];  pos06 = [260 220 420 310];
    pos07 = [480 220 640 310]; pos08 = [700 220 860 310]; pos09 = [40 380 200 470];
    pos10 = [260 380 420 470]; pos11 = [480 380 640 470]; pos12 = [700 380 860 470];

    buildScenarioManager(MDL, pos01);
    buildSpeedController(MDL, pos02);
    buildPropulsionPlant(MDL, pos03);
    buildLoadMeasurement(MDL, pos04);
    buildEMSController(MDL, pos05);
    buildGensetPlant(MDL, pos06);
    buildBatteryPlant(MDL, pos07);
    buildAvailabilityLimiter(MDL, pos08);
    buildBalanceMonitor(MDL, pos09);
    buildFuelConsumption(MDL, pos10);
    buildVoyageStop(MDL, pos11);
    buildDataLoggingDoc(MDL, pos12);
    add_block('simulink/Sources/Clock', [MDL '/t'], 'Position', [920 60 950 90]);

    %% ================= 3. 保存/重载（物化变体父级端口）后接线 =================
    save_system(MDL, mdlPath);
    close_system(MDL, 0);
    load_system(mdlPath);
    assignin('base', 'EMS_MODE', 1);
    % 清理变体父级掩码遗留端口（双保险）
    sys = [MDL '/05_EMS_Controller'];
    cleanupVariantPorts(sys, {'P_load_total_kW','SOC','v_cmd_mps','v_ship_mps','acceleration_mps2','x_ship_m','t'}, {'P_gen_ref_kW'});
    wireTopLevel(MDL);

    %% ================= 4. 信号记录配置 =================
    configureLogging(MDL);

    %% ================= 5. 保存并验证 =================
    save_system(MDL, mdlPath);
    set_param(MDL, 'SimulationCommand', 'update');
    fprintf('[02] 更新图 (EMS_MODE=1) 成功\n');
    assignin('base', 'EMS_MODE', 0);
    set_param(MDL, 'SimulationCommand', 'update');
    fprintf('[02] 更新图 (EMS_MODE=0) 成功\n');
    fprintf('[02] 模型构建完成: %s\n', mdlPath);

catch err
    fprintf('[02] 构建失败: %s\n', err.message);
    for k = 1:numel(err.stack)
        fprintf('    at %s line %d\n', err.stack(k).name, err.stack(k).line);
    end
    if bdIsLoaded(MDL), close_system(MDL, 0); end
    rethrow(err);
end

%% ================= 6. 参数审计 CSV =================
if ~exist(fullfile(ROOT, 'reports'), 'dir'), mkdir(fullfile(ROOT, 'reports')); end
model.write_param_csv(fullfile(ROOT, 'reports', 'parameter_audit.csv'));

%% ========================================================================
%  以下为构建辅助函数
% ========================================================================

end

function buildScenarioManager(mdl, pos)
% 01_Scenario_Manager: 输出 v_cmd/v_current/resistance_multiplier/P_hotel/scenario_id
    sys = [mdl '/01_Scenario_Manager'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/x_ship_m'], 'Position', [20 80 50 110]);
    outs = {'v_cmd_mps','v_current_mps','resistance_multiplier','P_hotel_kW','scenario_id'};
    for k = 1:numel(outs)
        add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [430 30+50*(k-1) 460 60+50*(k-1)]);
    end
    % 目标航速剖面（距离触发 knots）+ 时间域斜坡（禁止瞬时阶跃）
    fnv = [sys '/v_cmd_profile'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fnv, 'Position', [30 30 130 80]);
    setMFCode(fnv, CODE_VPROFILE, {});
    vconsts = {'kx','SCN.v_knots_x'; 'kv','SCN.v_knots_v'; 'vdef','SCN.target_speed_mps'};
    for k = 1:size(vconsts, 1)
        add_block('simulink/Sources/Constant', [sys '/CV_' vconsts{k,1}], ...
            'Position', [20 90+40*(k-1) 60 110+40*(k-1)], 'Value', vconsts{k,2});
        add_line(sys, ['CV_' vconsts{k,1} '/1'], ['v_cmd_profile/' num2str(k+1)]);
    end
    add_line(sys, 'x_ship_m/1', 'v_cmd_profile/1');
    add_block('simulink/Discontinuities/Rate Limiter', [sys '/v_cmd ramp'], 'Position', [150 30 240 60], ...
        'RisingSlewLimit', 'SCN.accel_ramp_mps2', 'FallingSlewLimit', '-SCN.decel_ramp_mps2', ...
        'InitialCondition', '0');
    add_line(sys, 'v_cmd_profile/1', 'v_cmd ramp/1');
    add_line(sys, 'v_cmd ramp/1', 'v_cmd_mps/1');
    % 水流（里程剖面，支持中途反转；单节点=恒定水流，S01~S09 语义不变）
    fnc = [sys '/current_profile'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fnc, 'Position', [200 240 320 300]);
    setMFCode(fnc, CODE_CURRENT, {});
    cconsts = {'ckx','SCN.current_knots_x'; 'ckv','SCN.current_knots_v'; 'cdef','SCN.river_current_mps'};
    for k = 1:size(cconsts, 1)
        add_block('simulink/Sources/Constant', [sys '/CC_' cconsts{k,1}], ...
            'Position', [150 260+40*(k-1) 190 280+40*(k-1)], 'Value', cconsts{k,2});
        add_line(sys, ['CC_' cconsts{k,1} '/1'], ['current_profile/' num2str(k+1)]);
    end
    add_line(sys, 'x_ship_m/1', 'current_profile/1');
    add_line(sys, 'current_profile/1', 'v_current_mps/1');
    % 阻力剖面（MATLAB Function，距离触发）
    fn = [sys '/resist_profile'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [200 90 320 190]);
    setMFCode(fn, CODE_RESIST, {});
    consts = {'profile','SCN.profile_code'; 'pos','SCN.resistance_change_position_m'; ...
        'amp','SCN.resistance_change_amplitude'; 'rampT','SCN.resistance_change_ramp_time_s'; ...
        'bands','SCN.resistance_bands'};
    for k = 1:size(consts, 1)
        add_block('simulink/Sources/Constant', [sys '/C_' consts{k,1}], ...
            'Position', [150 100+45*(k-1) 190 120+45*(k-1)], 'Value', consts{k,2});
        add_line(sys, ['C_' consts{k,1} '/1'], ['resist_profile/' num2str(k+1)]);
    end
    add_block('simulink/Sources/Constant', [sys '/C_vtarget'], 'Position', [150 220 190 240], 'Value', 'SCN.target_speed_mps');
    add_line(sys, 'x_ship_m/1', 'resist_profile/1');
    add_line(sys, 'C_vtarget/1', 'resist_profile/7');
    add_line(sys, 'resist_profile/1', 'resistance_multiplier/1');
    % 日用负荷与工况号
    add_block('simulink/Sources/Constant', [sys '/Hotel'], 'Position', [30 150 100 180], ...
        'Value', 'SCN.hotel_kW');
    add_line(sys, 'Hotel/1', 'P_hotel_kW/1');
    add_block('simulink/Sources/Constant', [sys '/ScenID'], 'Position', [30 200 100 230], ...
        'Value', 'SCN.id');
    add_line(sys, 'ScenID/1', 'scenario_id/1');
end

function buildSpeedController(mdl, pos)
% 02_Ship_Speed_Controller: PI 船速环 -> P_prop_demand_kW（限幅+抗饱和）
    sys = [mdl '/02_Ship_Speed_Controller'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/v_cmd_mps'], 'Position', [20 60 50 90]);
    add_block('simulink/Sources/In1', [sys '/v_ship_mps'], 'Position', [20 110 50 140]);
    add_block('simulink/Sinks/Out1', [sys '/P_prop_demand_kW'], 'Position', [330 80 360 110]);
    add_block('simulink/Math Operations/Sum', [sys '/err'], 'Position', [90 80 120 120], 'Inputs', '+-');
    add_line(sys, 'v_cmd_mps/1', 'err/1');
    add_line(sys, 'v_ship_mps/1', 'err/2');
    add_block('simulink/Math Operations/Gain', [sys '/Kp'], 'Position', [150 60 190 90], 'Gain', 'ASSUMED_SpeedPI_Kp');
    add_block('simulink/Continuous/Integrator', [sys '/Ki_int'], 'Position', [150 120 190 160], ...
        'InitialCondition', '0', 'LimitOutput', 'on', ...
        'UpperSaturationLimit', 'Mot_P_nom/ASSUMED_SpeedPI_Ki', ...
        'LowerSaturationLimit', '0');
    add_line(sys, 'err/1', 'Kp/1');
    add_line(sys, 'err/1', 'Ki_int/1');
    add_block('simulink/Math Operations/Gain', [sys '/Ki'], 'Position', [230 130 270 160], 'Gain', 'ASSUMED_SpeedPI_Ki');
    add_line(sys, 'Ki_int/1', 'Ki/1');
    add_block('simulink/Math Operations/Sum', [sys '/PI out'], 'Position', [250 70 280 110], 'Inputs', '++');
    add_line(sys, 'Kp/1', 'PI out/1');
    add_line(sys, 'Ki/1', 'PI out/2');
    add_block('simulink/Discontinuities/Saturation', [sys '/Limit'], 'Position', [300 80 320 110], ...
        'UpperLimit', 'Mot_P_nom', 'LowerLimit', '0');
    add_line(sys, 'PI out/1', 'Limit/1');
    add_line(sys, 'Limit/1', 'P_prop_demand_kW/1');
end

function buildPropulsionPlant(mdl, pos)
% 03_Propulsion_Plant: 功率限幅->驱动滞后->转矩->齿轮箱->轴系->螺旋桨->船体
    sys = [mdl '/03_Propulsion_Plant'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    ins = {'P_prop_demand_kW','P_prop_limit_kW','v_current_mps','resistance_multiplier'};
    for k = 1:numel(ins)
        add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+45*(k-1) 45 60+45*(k-1)]);
    end
    outs = {'P_prop_actual_kW','v_ship_mps','x_ship_m','acceleration_mps2','R_hull_N','F_prop_eff_N','n_prop_rpm'};
    for k = 1:numel(outs)
        add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [720 20+45*(k-1) 750 50+45*(k-1)]);
    end
    % 可用功率限幅: P_prop_ref = min(demand, limit)
    add_block('simulink/Math Operations/MinMax', [sys '/Avail'], 'Position', [70 30 110 60], 'Function', 'min', 'Inputs', '2');
    add_line(sys, 'P_prop_demand_kW/1', 'Avail/1');
    add_line(sys, 'P_prop_limit_kW/1', 'Avail/2');
    % 驱动滞后 -> P_prop_actual
    add_block('simulink/Continuous/Transfer Fcn', [sys '/DriveLag'], 'Position', [130 30 210 60], ...
        'Numerator', '[1]', 'Denominator', '[ASSUMED_Drive_TimeConstant 1]');
    add_block('simulink/Discontinuities/Saturation', [sys '/P_prop_clamp'], 'Position', [230 30 260 60], ...
        'UpperLimit', 'Mot_P_nom', 'LowerLimit', '0');
    add_line(sys, 'Avail/1', 'DriveLag/1');
    add_line(sys, 'DriveLag/1', 'P_prop_clamp/1');
    add_line(sys, 'P_prop_clamp/1', 'P_prop_actual_kW/1');
    % 电功率 -> 机械功率
    add_block('simulink/Math Operations/Gain', [sys '/ElecEff'], 'Position', [280 30 320 60], 'Gain', 'Elec_Eff');
    add_line(sys, 'P_prop_clamp/1', 'ElecEff/1');
    % 船速与航程（先建，供螺旋桨/船体引用；hull→v_ship 连线在船体创建后补）
    add_block('simulink/Continuous/Integrator', [sys '/v_ship'], 'Position', [600 30 640 60], ...
        'InitialCondition', 'Ship_V_init');
    add_line(sys, 'v_ship/1', 'v_ship_mps/1');
    add_block('simulink/Continuous/Integrator', [sys '/x_ship'], 'Position', [660 30 700 60], ...
        'InitialCondition', 'Ship_X_init');
    add_line(sys, 'v_ship/1', 'x_ship/1');
    add_line(sys, 'x_ship/1', 'x_ship_m/1');
    % 轴系动力学: dω/dt = (T_drive - Q_prop - B*ω)/Prop_J
    add_block('simulink/Math Operations/Sum', [sys '/shaft sum'], 'Position', [300 220 330 260], 'Inputs', '+--');
    add_block('simulink/Math Operations/Gain', [sys '/1J'], 'Position', [350 230 380 260], 'Gain', '1/Prop_J');
    add_block('simulink/Continuous/Integrator', [sys '/omega_prop'], 'Position', [400 230 440 260], 'InitialCondition', '0');
    add_line(sys, 'shaft sum/1', '1J/1');
    add_line(sys, '1J/1', 'omega_prop/1');
    % 螺旋桨转速 rpm
    add_block('simulink/Math Operations/Gain', [sys '/radps2rpm'], 'Position', [460 230 500 260], 'Gain', '30/pi');
    add_line(sys, 'omega_prop/1', 'radps2rpm/1');
    add_line(sys, 'radps2rpm/1', 'n_prop_rpm/1');
    % 螺旋桨水动力（复用原模型 MATLAB Function）
    fn = [sys '/Propeller_Hydrodynamics'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [540 120 660 230]);
    setMFCode(fn, CODE_PROPELLER, {'wake_fraction','K_Fw','K_Fv','K_Qw','K_Qv'});
    add_line(sys, 'radps2rpm/1', 'Propeller_Hydrodynamics/1');
    add_line(sys, 'v_ship/1', 'Propeller_Hydrodynamics/2');
    add_line(sys, 'v_current_mps/1', 'Propeller_Hydrodynamics/3');
    add_line(sys, 'Propeller_Hydrodynamics/2', 'shaft sum/3');   % Q_prop
    add_line(sys, 'Propeller_Hydrodynamics/1', 'F_prop_eff_N/1');
    % 船体（复用+阻力倍率）
    fn2 = [sys '/Ship_Hull_Dynamics'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn2, 'Position', [540 260 660 370]);
    setMFCode(fn2, CODE_HULL, {'Hull_K_R','Ship_Mass'});
    add_block('simulink/Sources/Constant', [sys '/F_env'], 'Position', [480 330 530 360], 'Value', 'F_env_const');
    add_line(sys, 'v_ship/1', 'Ship_Hull_Dynamics/1');
    add_line(sys, 'Propeller_Hydrodynamics/1', 'Ship_Hull_Dynamics/2');
    add_line(sys, 'v_current_mps/1', 'Ship_Hull_Dynamics/3');
    add_line(sys, 'F_env/1', 'Ship_Hull_Dynamics/4');
    add_line(sys, 'resistance_multiplier/1', 'Ship_Hull_Dynamics/5');
    add_line(sys, 'Ship_Hull_Dynamics/1', 'acceleration_mps2/1');
    add_line(sys, 'Ship_Hull_Dynamics/2', 'R_hull_N/1');
    add_line(sys, 'Ship_Hull_Dynamics/1', 'v_ship/1');
    % 转矩链: T_req = P_mech*1000/max(|ω_motor|, ω_min) -> ±Mot_T_max -> T_drive
    add_block('simulink/Math Operations/Gain', [sys '/kW2W'], 'Position', [340 30 380 60], 'Gain', '1000');
    add_line(sys, 'ElecEff/1', 'kW2W/1');
    add_block('simulink/Math Operations/Gain', [sys '/GearRatio_omega'], 'Position', [340 160 380 190], 'Gain', 'Gear_Ratio');
    add_line(sys, 'omega_prop/1', 'GearRatio_omega/1');
    add_block('simulink/Math Operations/Abs', [sys '/abs_omega'], 'Position', [400 160 430 190]);
    add_line(sys, 'GearRatio_omega/1', 'abs_omega/1');
    add_block('simulink/Math Operations/MinMax', [sys '/omega_min'], 'Position', [450 160 490 190], 'Function', 'max', 'Inputs', '2');
    add_block('simulink/Sources/Constant', [sys '/omega_floor'], 'Position', [400 130 440 150], 'Value', 'omega_min_radps');
    add_line(sys, 'omega_floor/1', 'omega_min/2');
    add_line(sys, 'abs_omega/1', 'omega_min/1');
    add_block('simulink/Math Operations/Product', [sys '/T_req'], 'Position', [520 30 560 60], 'Inputs', '*/');
    add_line(sys, 'kW2W/1', 'T_req/1');
    add_line(sys, 'omega_min/1', 'T_req/2');
    add_block('simulink/Discontinuities/Saturation', [sys '/T_motor_clamp'], 'Position', [580 30 620 60], ...
        'UpperLimit', 'Mot_T_max', 'LowerLimit', '-Mot_T_max');
    add_line(sys, 'T_req/1', 'T_motor_clamp/1');
    add_block('simulink/Math Operations/Gain', [sys '/Gearbox'], 'Position', [640 100 690 130], ...
        'Gain', 'Gear_Ratio*Gear_Eff');
    add_line(sys, 'T_motor_clamp/1', 'Gearbox/1');
    add_line(sys, 'Gearbox/1', 'shaft sum/1');
    % 轴系阻尼
    add_block('simulink/Math Operations/Gain', [sys '/ShaftB'], 'Position', [330 280 370 310], 'Gain', 'Shaft_B');
    add_line(sys, 'omega_prop/1', 'ShaftB/1');
    add_line(sys, 'ShaftB/1', 'shaft sum/2');
end

function buildLoadMeasurement(mdl, pos)
% 04_Load_Measurement: P_load_total = max(P_prop_actual,0) + P_hotel
    sys = [mdl '/04_Load_Measurement'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/P_prop_actual_kW'], 'Position', [20 50 50 80]);
    add_block('simulink/Sources/In1', [sys '/P_hotel_kW'], 'Position', [20 110 50 140]);
    add_block('simulink/Sinks/Out1', [sys '/P_load_total_kW'], 'Position', [250 80 280 110]);
    add_block('simulink/Discontinuities/Saturation', [sys '/clamp0'], 'Position', [80 50 110 80], ...
        'UpperLimit', 'inf', 'LowerLimit', '0');
    add_line(sys, 'P_prop_actual_kW/1', 'clamp0/1');
    add_block('simulink/Math Operations/Sum', [sys '/total'], 'Position', [150 70 180 110], 'Inputs', '++');
    add_line(sys, 'clamp0/1', 'total/1');
    add_line(sys, 'P_hotel_kW/1', 'total/2');
    add_line(sys, 'total/1', 'P_load_total_kW/1');
end

function buildEMSController(mdl, pos)
% 05_EMS_Controller: Variant Subsystem（EMS_MODE 选择 Baseline/AI Enhanced）
    sys = [mdl '/05_EMS_Controller'];
    add_block('simulink/Ports & Subsystems/Variant Subsystem', sys, 'Position', pos);
    % 变体选择（默认子模块路径由掩码提供）
    set_param([sys '/Subsystem'],  'VariantControl', 'EMS_MODE == 1');
    set_param([sys '/Subsystem1'], 'VariantControl', 'EMS_MODE == 0');
    set_param([sys '/Subsystem'],  'Name', 'EMS_AI_Enhanced');
    set_param([sys '/Subsystem1'], 'Name', 'EMS_Baseline');
    inames = {'P_load_total_kW','SOC','v_cmd_mps','v_ship_mps','acceleration_mps2','x_ship_m','t'};
    for k = 1:2
        if k == 1, ch = [sys '/EMS_Baseline']; else, ch = [sys '/EMS_AI_Enhanced']; end
        % 清空默认端口
        d = find_system(ch, 'SearchDepth', 1, 'BlockType', 'Inport');
        for i = 1:numel(d), delete_block(d{i}); end
        d = find_system(ch, 'SearchDepth', 1, 'BlockType', 'Outport');
        for i = 1:numel(d), delete_block(d{i}); end
        for j = 1:numel(inames)
            add_block('simulink/Sources/In1', [ch '/' inames{j}], 'Position', [10 10+40*(j-1) 40 30+40*(j-1)]);
        end
        add_block('simulink/Sinks/Out1', [ch '/P_gen_ref_kW'], 'Position', [330 10 360 30]);
        if k == 1
            buildBaselineVariant(ch);
        else
            buildAIVariant(ch);
        end
    end
    % 同步父级接口端口，并清理掩码遗留端口
    slprivate('variantfixes', 'MissingPorts', sys, 'addPorts', 'IOConnPort');
    for k = 1:2
        if k == 1, ch = [sys '/EMS_Baseline']; else, ch = [sys '/EMS_AI_Enhanced']; end
        for nm = {'In1', 'Out1'}
            try, delete_block([ch '/' nm{1}]); catch, end
        end
    end
    cleanupVariantPorts(sys, inames, {'P_gen_ref_kW'});
end

function buildBaselineVariant(ch)
% EMS_Baseline: 负荷跟随 + SOC 修正 -> 限幅
    fn = [ch '/ems_target'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [90 10 200 80]);
    setMFCode(fn, CODE_EMS_TARGET, {'ASSUMED_EMS_SOC_Gain','ASSUMED_EMS_SOC_Ref'});
    add_block('simulink/Discontinuities/Saturation', [ch '/clamp'], 'Position', [240 10 280 50], ...
        'UpperLimit', 'Gen_P_max', 'LowerLimit', '0');
    add_line(ch, 'P_load_total_kW/1', 'ems_target/1');
    add_line(ch, 'SOC/1', 'ems_target/2');
    add_line(ch, 'ems_target/1', 'clamp/1');
    add_line(ch, 'clamp/1', 'P_gen_ref_kW/1');
    % 未使用输入接终结块
    for nm = {'v_cmd_mps', 'v_ship_mps', 'acceleration_mps2', 'x_ship_m', 't'}
        add_block('simulink/Sinks/Terminator', [ch '/T_' nm{1}], 'Position', [150 200 170 220]);
        add_line(ch, [nm{1} '/1'], ['T_' nm{1} '/1']);
    end
end

function buildAIVariant(ch)
% EMS_AI_Enhanced: Baseline + 预测前馈（AI_valid 失效时自动退回）
    win = 60;
    try
        w0 = evalin('base', 'MLP_Window');
        if isscalar(w0) && w0 >= 5, win = round(w0); end
    catch
    end
    fn = [ch '/ai_predict'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [60 30 180 120]);
    setMFCode(fn, CODE_AI_PREDICT(win), {'AI_Pred_Source','AI_Pred_Data','STM32_Pred_Data','ASSUMED_AI_DataValid','ASSUMED_Pred_Bound_kW','MLP_WeightsLoaded','MLP_W1','MLP_b1','MLP_W2','MLP_b2','MLP_W3','MLP_b3','MLP_FeatMean','MLP_FeatStd','MLP_YMean','MLP_YStd'}, {'','[1 2]','[1 2]','','','',sprintf('[%d 64]', win*5),sprintf('[1 64]'),'[64 32]','[1 32]','[32 1]','[1 1]','[1 5]','[1 5]','[1 1]','[1 1]'});
    % 含 persistent 历史窗口 -> 必须离散采样（取模型最快速率 0.01 s，与 FixedStep 一致；
    % 内部仍以 1 Hz 更新窗口，source=0 时逐步 Ppred=P_load，与连续语义一致）
    chAI = sfroot().find('-isa', 'Stateflow.EMChart', 'Path', fn);
    chAI.ChartUpdate = 'DISCRETE';
    chAI.SampleTime = '0.01';
    fn2 = [ch '/ems_target'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn2, 'Position', [60 160 180 230]);
    setMFCode(fn2, CODE_EMS_TARGET, {'ASSUMED_EMS_SOC_Gain','ASSUMED_EMS_SOC_Ref'});
    add_line(ch, 't/1', 'ai_predict/1');
    add_line(ch, 'P_load_total_kW/1', 'ai_predict/2');
    add_line(ch, 'v_cmd_mps/1', 'ai_predict/3');
    add_line(ch, 'v_ship_mps/1', 'ai_predict/4');
    add_line(ch, 'acceleration_mps2/1', 'ai_predict/5');
    add_line(ch, 'x_ship_m/1', 'ai_predict/6');
    add_line(ch, 'P_load_total_kW/1', 'ems_target/1');
    add_line(ch, 'SOC/1', 'ems_target/2');
    % Delta = max(P_pred - P_load, 0)
    add_block('simulink/Math Operations/Sum', [ch '/delta'], 'Position', [230 40 260 70], 'Inputs', '+-');
    add_line(ch, 'ai_predict/1', 'delta/1');
    add_line(ch, 'P_load_total_kW/1', 'delta/2');
    add_block('simulink/Discontinuities/Saturation', [ch '/delta_clamp'], 'Position', [280 40 310 70], ...
        'UpperLimit', 'inf', 'LowerLimit', '0');
    add_line(ch, 'delta/1', 'delta_clamp/1');
    add_block('simulink/Math Operations/Gain', [ch '/K_ff'], 'Position', [330 40 370 70], 'Gain', 'ASSUMED_EMS_FF_Gain');
    add_line(ch, 'delta_clamp/1', 'K_ff/1');
    add_block('simulink/Discontinuities/Saturation', [ch '/ff_clamp'], 'Position', [390 40 430 70], ...
        'UpperLimit', 'ASSUMED_EMS_FF_Max_kW', 'LowerLimit', '0');
    add_line(ch, 'K_ff/1', 'ff_clamp/1');
    add_block('simulink/Math Operations/Product', [ch '/gate'], 'Position', [450 40 490 70], 'Inputs', '**');
    add_line(ch, 'ff_clamp/1', 'gate/1');
    add_line(ch, 'ai_predict/2', 'gate/2');
    % P_load + P_ff
    add_block('simulink/Math Operations/Sum', [ch '/load_ff'], 'Position', [510 40 540 70], 'Inputs', '++');
    add_line(ch, 'P_load_total_kW/1', 'load_ff/1');
    add_line(ch, 'gate/1', 'load_ff/2');
    % max(base, load+ff)
    add_block('simulink/Math Operations/MinMax', [ch '/pick'], 'Position', [560 40 600 70], 'Function', 'max', 'Inputs', '2');
    add_line(ch, 'ems_target/1', 'pick/1');
    add_line(ch, 'load_ff/1', 'pick/2');
    add_block('simulink/Discontinuities/Saturation', [ch '/clamp'], 'Position', [620 40 660 70], ...
        'UpperLimit', 'Gen_P_max', 'LowerLimit', '0');
    add_line(ch, 'pick/1', 'clamp/1');
    add_line(ch, 'clamp/1', 'P_gen_ref_kW/1');
end

function buildGensetPlant(mdl, pos)
% 06_Genset_Average_Plant: 限幅->爬坡->一阶惯性->限幅（发电机常开）
    sys = [mdl '/06_Genset_Average_Plant'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/P_gen_ref_kW'], 'Position', [20 80 50 110]);
    add_block('simulink/Sinks/Out1', [sys '/P_gen_actual_kW'], 'Position', [330 80 360 110]);
    add_block('simulink/Discontinuities/Saturation', [sys '/limit_in'], 'Position', [70 80 100 110], ...
        'UpperLimit', 'Gen_P_max', 'LowerLimit', 'ASSUMED_Gen_MinStablePower');
    add_line(sys, 'P_gen_ref_kW/1', 'limit_in/1');
    add_block('simulink/Discontinuities/Rate Limiter', [sys '/ramp'], 'Position', [130 80 210 110], ...
        'RisingSlewLimit', 'Gen_Ramp_Up', 'FallingSlewLimit', '-Gen_Ramp_Down', ...
        'InitialCondition', 'Pgen_init');
    add_line(sys, 'limit_in/1', 'ramp/1');
    add_block('simulink/Continuous/Transfer Fcn', [sys '/inertia'], 'Position', [240 80 300 110], ...
        'Numerator', '[1]', 'Denominator', '[ASSUMED_Gen_TimeConstant 1]');
    add_line(sys, 'ramp/1', 'inertia/1');
    add_block('simulink/Discontinuities/Saturation', [sys '/limit_out'], 'Position', [310 80 330 110], ...
        'UpperLimit', 'Gen_P_max', 'LowerLimit', 'ASSUMED_Gen_MinStablePower');
    add_line(sys, 'inertia/1', 'limit_out/1');
    add_line(sys, 'limit_out/1', 'P_gen_actual_kW/1');
end

function buildBatteryPlant(mdl, pos)
% 07_Battery_Energy_Plant: 残差功率 + 充放电限制/SOC 效率积分
    sys = [mdl '/07_Battery_Energy_Plant'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/P_load_total_kW'], 'Position', [20 60 50 90]);
    add_block('simulink/Sources/In1', [sys '/P_gen_actual_kW'], 'Position', [20 120 50 150]);
    outs = {'P_bat_actual_kW','SOC','P_bat_dis_avail_kW','P_bat_chg_avail_kW'};
    for k = 1:numel(outs)
        add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [380 30+60*(k-1) 410 60+60*(k-1)]);
    end
    add_block('simulink/Math Operations/Sum', [sys '/P_bat_req'], 'Position', [80 90 110 120], 'Inputs', '+-');
    add_line(sys, 'P_load_total_kW/1', 'P_bat_req/1');
    add_line(sys, 'P_gen_actual_kW/1', 'P_bat_req/2');
    % 可用功率（SOC 降额）
    fn = [sys '/bat_limits'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [220 140 340 210]);
    setMFCode(fn, CODE_BAT_LIMITS, {'ASSUMED_SOC_min','ASSUMED_SOC_max','ASSUMED_SOC_DisDerate_Start','ASSUMED_SOC_ChgDerate_Start','ASSUMED_Bat_DischargeLimit','ASSUMED_Bat_ChargeLimit'});
    add_block('simulink/Continuous/Integrator', [sys '/SOC_int'], 'Position', [150 280 190 310], ...
        'InitialCondition', 'Bat_SOC_init', 'LimitOutput', 'on', ...
        'UpperSaturationLimit', 'ASSUMED_SOC_max', 'LowerSaturationLimit', 'ASSUMED_SOC_min');
    add_line(sys, 'SOC_int/1', 'bat_limits/1');
    add_line(sys, 'bat_limits/1', 'P_bat_dis_avail_kW/1');
    add_line(sys, 'bat_limits/2', 'P_bat_chg_avail_kW/1');
    % 限幅: P_bat = min(max(req, -chg), dis)
    add_block('simulink/Math Operations/Gain', [sys '/neg_chg'], 'Position', [150 210 190 240], 'Gain', '-1');
    add_line(sys, 'bat_limits/2', 'neg_chg/1');
    add_block('simulink/Math Operations/MinMax', [sys '/lo_clamp'], 'Position', [220 90 260 120], 'Function', 'max', 'Inputs', '2');
    add_line(sys, 'P_bat_req/1', 'lo_clamp/1');
    add_line(sys, 'neg_chg/1', 'lo_clamp/2');
    add_block('simulink/Math Operations/MinMax', [sys '/hi_clamp'], 'Position', [280 90 320 120], 'Function', 'min', 'Inputs', '2');
    add_line(sys, 'lo_clamp/1', 'hi_clamp/1');
    add_line(sys, 'bat_limits/1', 'hi_clamp/2');
    add_line(sys, 'hi_clamp/1', 'P_bat_actual_kW/1');
    % SOC 变化率（充放电效率）
    fn2 = [sys '/soc_rate'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn2, 'Position', [150 320 270 380]);
    setMFCode(fn2, CODE_SOC_RATE, {'Bat_Cap_kWh','ASSUMED_Bat_Eff_Discharge','ASSUMED_Bat_Eff_Charge'});
    add_line(sys, 'hi_clamp/1', 'soc_rate/1');
    add_line(sys, 'soc_rate/1', 'SOC_int/1');
    add_line(sys, 'SOC_int/1', 'SOC/1');
end

function buildAvailabilityLimiter(mdl, pos)
% 08_Power_Availability_Limiter
    sys = [mdl '/08_Power_Availability_Limiter'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    ins = {'P_load_total_kW','P_gen_actual_kW','P_bat_dis_avail_kW','P_hotel_kW'};
    for k = 1:numel(ins)
        add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+45*(k-1) 45 60+45*(k-1)]);
    end
    add_block('simulink/Sinks/Out1', [sys '/P_prop_limit_kW'], 'Position', [330 40 360 70]);
    add_block('simulink/Sinks/Out1', [sys '/P_unserved_kW'], 'Position', [330 140 360 170]);
    % P_available = max(gen + bat - hotel, 0)
    add_block('simulink/Math Operations/Sum', [sys '/avail sum'], 'Position', [80 30 110 70], 'Inputs', '++-');
    add_line(sys, 'P_gen_actual_kW/1', 'avail sum/1');
    add_line(sys, 'P_bat_dis_avail_kW/1', 'avail sum/2');
    add_line(sys, 'P_hotel_kW/1', 'avail sum/3');
    add_block('simulink/Discontinuities/Saturation', [sys '/avail clamp'], 'Position', [140 40 170 70], ...
        'UpperLimit', 'inf', 'LowerLimit', '0');
    add_line(sys, 'avail sum/1', 'avail clamp/1');
    add_block('simulink/Math Operations/MinMax', [sys '/prop limit'], 'Position', [220 40 260 70], 'Function', 'min', 'Inputs', '2');
    add_block('simulink/Sources/Constant', [sys '/Mot_P_nom'], 'Position', [170 90 220 110], 'Value', 'Mot_P_nom');
    add_line(sys, 'Mot_P_nom/1', 'prop limit/1');
    add_line(sys, 'avail clamp/1', 'prop limit/2');
    add_line(sys, 'prop limit/1', 'P_prop_limit_kW/1');
    % P_unserved = max(load - gen - bat_dis, 0)
    add_block('simulink/Math Operations/Sum', [sys '/unserved sum'], 'Position', [80 120 110 160], 'Inputs', '+--');
    add_line(sys, 'P_load_total_kW/1', 'unserved sum/1');
    add_line(sys, 'P_gen_actual_kW/1', 'unserved sum/2');
    add_line(sys, 'P_bat_dis_avail_kW/1', 'unserved sum/3');
    add_block('simulink/Discontinuities/Saturation', [sys '/unserved clamp'], 'Position', [160 130 190 160], ...
        'UpperLimit', 'inf', 'LowerLimit', '0');
    add_line(sys, 'unserved sum/1', 'unserved clamp/1');
    add_line(sys, 'unserved clamp/1', 'P_unserved_kW/1');
end

function buildBalanceMonitor(mdl, pos)
% 09_Power_Balance_Monitor: e_balance = P_gen + P_bat - P_load + P_unserved
    sys = [mdl '/09_Power_Balance_Monitor'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    ins = {'P_gen_actual_kW','P_bat_actual_kW','P_load_total_kW','P_unserved_kW'};
    for k = 1:numel(ins)
        add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+45*(k-1) 45 60+45*(k-1)]);
    end
    add_block('simulink/Sinks/Out1', [sys '/e_balance_kW'], 'Position', [240 80 270 110]);
    add_block('simulink/Math Operations/Sum', [sys '/bal'], 'Position', [110 70 140 110], 'Inputs', '++-+');
    add_line(sys, 'P_gen_actual_kW/1', 'bal/1');
    add_line(sys, 'P_bat_actual_kW/1', 'bal/2');
    add_line(sys, 'P_load_total_kW/1', 'bal/3');
    add_line(sys, 'P_unserved_kW/1', 'bal/4');
    add_line(sys, 'bal/1', 'e_balance_kW/1');
end

function buildFuelConsumption(mdl, pos)
% 10_Fuel_Consumption: ∫ P_gen * BSFC(P_gen)/3600/1000
    sys = [mdl '/10_Fuel_Consumption'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/P_gen_actual_kW'], 'Position', [20 80 50 110]);
    add_block('simulink/Sinks/Out1', [sys '/fuel_total_kg'], 'Position', [330 80 360 110]);
    add_block('simulink/Lookup Tables/1-D Lookup Table', [sys '/BSFC'], 'Position', [80 60 140 110], ...
        'BreakpointsForDimension1', 'ASSUMED_BSFC_Breakpoints', 'Table', 'ASSUMED_BSFC_Table');
    add_line(sys, 'P_gen_actual_kW/1', 'BSFC/1');
    add_block('simulink/Math Operations/Product', [sys '/flow'], 'Position', [170 80 210 110], 'Inputs', '**');
    add_line(sys, 'P_gen_actual_kW/1', 'flow/1');
    add_line(sys, 'BSFC/1', 'flow/2');
    add_block('simulink/Math Operations/Gain', [sys '/hr2sec'], 'Position', [230 80 260 110], 'Gain', '1/3600');
    add_line(sys, 'flow/1', 'hr2sec/1');
    add_block('simulink/Continuous/Integrator', [sys '/fuel_int'], 'Position', [280 80 310 110], 'InitialCondition', '0');
    add_line(sys, 'hr2sec/1', 'fuel_int/1');
    add_block('simulink/Math Operations/Gain', [sys '/g2kg'], 'Position', [320 80 330 110], 'Gain', '1/1000');
    add_line(sys, 'fuel_int/1', 'g2kg/1');
    add_line(sys, 'g2kg/1', 'fuel_total_kg/1');
end

function buildVoyageStop(mdl, pos)
% 11_Voyage_Stop_Logic: x_ship >= 航程 -> 停止仿真
    sys = [mdl '/11_Voyage_Stop_Logic'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    add_block('simulink/Sources/In1', [sys '/x_ship_m'], 'Position', [20 80 50 110]);
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/reach'], 'Position', [90 80 160 110], ...
        'const', 'SCN.voyage_distance_m', 'relop', '>=');
    add_line(sys, 'x_ship_m/1', 'reach/1');
    add_block('simulink/Sinks/Stop Simulation', [sys '/stop'], 'Position', [200 80 280 110]);
    add_line(sys, 'reach/1', 'stop/1');
end

function buildDataLoggingDoc(mdl, pos)
% 12_Data_Logging: 顶层数据记录配置说明（无端口，日志经 signal logging 实现）
    sys = [mdl '/12_Data_Logging'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);
    set_param(sys, 'MaskDisplay', ['disp(sprintf(''12 Data Logging\n' ...
        'signal logging -> logsout\n' ...
        'configured in +model/build_model.m''))']);
end

function wireTopLevel(mdl)
% 顶层接线（使用 EMS 变体的动态端口号）
    emsPort = emsPortMap(mdl);
    L = @(src, dst) add_line(mdl, src, dst, 'autorouting', 'on');
    L('01_Scenario_Manager/1', '02_Ship_Speed_Controller/1');          % v_cmd
    L('01_Scenario_Manager/2', '03_Propulsion_Plant/3');               % v_current
    L('01_Scenario_Manager/3', '03_Propulsion_Plant/4');               % resistance_multiplier
    L('01_Scenario_Manager/4', '04_Load_Measurement/2');               % P_hotel
    L('01_Scenario_Manager/4', '08_Power_Availability_Limiter/4');     % P_hotel -> limiter
    L('02_Ship_Speed_Controller/1', '03_Propulsion_Plant/1');          % P_prop_demand
    L('03_Propulsion_Plant/1', '04_Load_Measurement/1');               % P_prop_actual
    L('03_Propulsion_Plant/2', '02_Ship_Speed_Controller/2');          % v_ship
    L('03_Propulsion_Plant/2', sprintf('05_EMS_Controller/%d', emsPort('v_ship_mps')));
    L('01_Scenario_Manager/1', sprintf('05_EMS_Controller/%d', emsPort('v_cmd_mps')));  % v_cmd -> EMS
    L('03_Propulsion_Plant/3', '01_Scenario_Manager/1');              % x_ship
    L('03_Propulsion_Plant/3', '11_Voyage_Stop_Logic/1');             % x_ship -> stop
    L('03_Propulsion_Plant/3', sprintf('05_EMS_Controller/%d', emsPort('x_ship_m'))); % x_ship -> EMS(ML特征)
    L('03_Propulsion_Plant/4', sprintf('05_EMS_Controller/%d', emsPort('acceleration_mps2')));
    L('04_Load_Measurement/1', sprintf('05_EMS_Controller/%d', emsPort('P_load_total_kW')));
    L('04_Load_Measurement/1', '07_Battery_Energy_Plant/1');
    L('04_Load_Measurement/1', '08_Power_Availability_Limiter/1');
    L(sprintf('05_EMS_Controller/%d', emsPort('P_gen_ref_kW')), '06_Genset_Average_Plant/1');
    L('06_Genset_Average_Plant/1', '07_Battery_Energy_Plant/2');
    L('06_Genset_Average_Plant/1', '08_Power_Availability_Limiter/2');
    L('06_Genset_Average_Plant/1', '09_Power_Balance_Monitor/1');
    L('06_Genset_Average_Plant/1', '10_Fuel_Consumption/1');
    L('07_Battery_Energy_Plant/1', '09_Power_Balance_Monitor/2');
    L('04_Load_Measurement/1', '09_Power_Balance_Monitor/3');          % P_load_total -> balance
    L('07_Battery_Energy_Plant/2', sprintf('05_EMS_Controller/%d', emsPort('SOC')));
    L('07_Battery_Energy_Plant/3', '08_Power_Availability_Limiter/3');
    L('08_Power_Availability_Limiter/1', '03_Propulsion_Plant/2');     % P_prop_limit
    L('08_Power_Availability_Limiter/2', '09_Power_Balance_Monitor/4');
    L('t/1', sprintf('05_EMS_Controller/%d', emsPort('t')));
    % 仅记录信号的输出接 Terminator（保证信号线存在、可命名）
    terms = {'01_Scenario_Manager/5'; '03_Propulsion_Plant/5'; '03_Propulsion_Plant/6'; ...
        '03_Propulsion_Plant/7'; '07_Battery_Energy_Plant/4'; '09_Power_Balance_Monitor/1'; ...
        '10_Fuel_Consumption/1'};
    for k = 1:numel(terms)
        tb = sprintf('T_log_%d', k);
        add_block('simulink/Sinks/Terminator', [mdl '/' tb], 'Position', [960 60+40*(k-1) 980 80+40*(k-1)]);
        L(terms{k}, [tb '/1']);
    end
end

function m = emsPortMap(mdl)
% 读取 EMS 变体父级同步端口的 PortNumber 映射
    sys = [mdl '/05_EMS_Controller'];
    names = {'P_load_total_kW','SOC','v_cmd_mps','v_ship_mps','acceleration_mps2','x_ship_m','t','P_gen_ref_kW'};
    m = containers.Map();
    for i = 1:numel(names)
        p = [sys '/' names{i}];
        try
            m(names{i}) = str2double(get_param(p, 'Port'));
        catch
            error('EMS 变体端口缺失: %s', names{i});
        end
    end
    if m.Count < 8
        error('EMS 变体端口同步不完整: %d/8', m.Count);
    end
end

function configureLogging(mdl)
% 在信号源端口上启用 DataLogging（logsout）
    L = @(blk, portIdx, nm) logPort(blk, portIdx, nm);
    L([mdl '/01_Scenario_Manager/v_cmd_mps'], 1, 'v_cmd_mps');
    L([mdl '/01_Scenario_Manager/v_current_mps'], 1, 'v_current_mps');
    L([mdl '/01_Scenario_Manager/resistance_multiplier'], 1, 'resistance_multiplier');
    L([mdl '/01_Scenario_Manager/P_hotel_kW'], 1, 'P_hotel_kW');
    L([mdl '/01_Scenario_Manager/scenario_id'], 1, 'scenario_id');
    L([mdl '/02_Ship_Speed_Controller/P_prop_demand_kW'], 1, 'P_prop_demand_kW');
    L([mdl '/03_Propulsion_Plant/P_prop_actual_kW'], 1, 'P_prop_actual_kW');
    L([mdl '/03_Propulsion_Plant/v_ship_mps'], 1, 'v_ship_mps');
    L([mdl '/03_Propulsion_Plant/x_ship_m'], 1, 'x_ship_m');
    L([mdl '/03_Propulsion_Plant/acceleration_mps2'], 1, 'acceleration_mps2');
    L([mdl '/03_Propulsion_Plant/R_hull_N'], 1, 'R_hull_N');
    L([mdl '/03_Propulsion_Plant/F_prop_eff_N'], 1, 'F_prop_eff_N');
    L([mdl '/03_Propulsion_Plant/n_prop_rpm'], 1, 'n_prop_rpm');
    L([mdl '/04_Load_Measurement/P_load_total_kW'], 1, 'P_load_total_kW');
    % P_gen_ref_kW：在两个变体内部的 clamp 输出端口记录（活动变体生效）
    for v = {'EMS_Baseline', 'EMS_AI_Enhanced'}
        logPort([mdl '/05_EMS_Controller/' v{1} '/clamp'], 1, 'P_gen_ref_kW');
    end
    L([mdl '/06_Genset_Average_Plant/P_gen_actual_kW'], 1, 'P_gen_actual_kW');
    L([mdl '/07_Battery_Energy_Plant/P_bat_actual_kW'], 1, 'P_bat_actual_kW');
    L([mdl '/07_Battery_Energy_Plant/SOC'], 1, 'SOC');
    L([mdl '/07_Battery_Energy_Plant/P_bat_dis_avail_kW'], 1, 'P_bat_dis_avail_kW');
    L([mdl '/08_Power_Availability_Limiter/P_prop_limit_kW'], 1, 'P_prop_limit_kW');
    L([mdl '/08_Power_Availability_Limiter/P_unserved_kW'], 1, 'P_unserved_kW');
    L([mdl '/09_Power_Balance_Monitor/e_balance_kW'], 1, 'e_balance_kW');
    L([mdl '/10_Fuel_Consumption/fuel_total_kg'], 1, 'fuel_total_kg');
    % AI 变体内部信号
    ai = [mdl '/05_EMS_Controller/EMS_AI_Enhanced/ai_predict'];
    logPort(ai, 1, 'P_pred_max_kW');
    logPort(ai, 2, 'AI_valid');
end

function logPortIn(blk, portIdx, nm)
% 在子系统输入端口启用信号记录（用于变体输出等接口场景）
    try
        [parent, ~] = fileparts(blk);
        k = str2double(get_param(blk, 'Port'));
        ph = get_param(parent, 'PortHandles');
        h = ph.Inport(k);
        set_param(h, 'DataLogging', 'on');
        ln = get_param(h, 'Line');
        if ln > 0
            set_param(ln, 'Name', nm);
        end
    catch err
        fprintf('[02] 记录配置警告 %s: %s\n', nm, err.message);
    end
end

function logPort(blk, portIdx, nm)
% 在指定端口启用信号记录并命名信号线
% blk: 子系统路径（Outport 块路径亦可）或 MATLAB Function 块路径
    try
        [parent, ~] = fileparts(blk);
        bt = get_param(blk, 'BlockType');
        if strcmp(bt, 'Outport')
            % 子系统输出：在父级端口句柄上记录
            k = str2double(get_param(blk, 'Port'));
            ph = get_param(parent, 'PortHandles');
            h = ph.Outport(k);
        else
            ph = get_param(blk, 'PortHandles');
            h = ph.Outport(portIdx);
        end
        set_param(h, 'DataLogging', 'on');
        ln = get_param(h, 'Line');
        if ln > 0
            set_param(ln, 'Name', nm);
        end
    catch err
        fprintf('[02] 记录配置警告 %s: %s\n', nm, err.message);
    end
end

function cleanupVariantPorts(sys, keepIn, keepOut)
% 删除变体子系统父级/子级中不属于保留名单的端口块
    allb = find_system(sys, 'MatchFilter', @Simulink.match.allVariants, ...
        'LookUnderMasks', 'all', 'FollowLinks', 'on', 'SearchDepth', 1, 'Type', 'block');
    for i = 1:numel(allb)
        p = allb{i};
        bt = get_param(p, 'BlockType');
        [~, nm] = fileparts(p);
        if strcmp(bt, 'Inport') && ~ismember(nm, keepIn)
            try, delete_block(p); catch, end
        elseif strcmp(bt, 'Outport') && ~ismember(nm, keepOut)
            try, delete_block(p); catch, end
        end
    end
end

function setMFCode(blkPath, code, paramNames, paramSizes)
% 通过 Stateflow.EMChart 设置 MATLAB Function 块代码（R2026a 新式块）
% 并声明工作区参数（Stateflow.Data, Scope=Parameter）
    if nargin < 3, paramNames = {}; end
    if nargin < 4, paramSizes = {}; end
    rt = sfroot;
    ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', blkPath);
    if numel(ch) ~= 1
        error('MATLAB Function 块未找到: %s', blkPath);
    end
    ch.Script = code;
    % 参数声明：脚本解析会自动把未声明标识符生成为 Input 数据，
    % 这里将其改写为 Parameter 作用域（不存在则新建）
    ds = ch.find('-isa', 'Stateflow.Data');
    for i = 1:numel(paramNames)
        d = [];
        for j = 1:numel(ds)
            if strcmp(ds(j).Name, paramNames{i})
                d = ds(j);
                break;
            end
        end
        if isempty(d)
            d = Stateflow.Data(ch);
            d.Name = paramNames{i};
        end
        d.Scope = 'Parameter';
        if numel(paramSizes) >= i && ~isempty(paramSizes{i})
            d.Props.Array.Size = paramSizes{i};
        end
    end
end

%% ================= MATLAB Function 代码模板 =================
function c = CODE_VPROFILE()
    c = sprintf([ ...
        'function v = v_cmd_profile(x, kx, kv, v_def)\n' ...
        '%% 目标航速剖面：按里程节点分段线性（下游 Rate Limiter 再作时间域斜坡）\n' ...
        '%% kx: 1x6 节点里程 [m]（-1=结束），kv: 1x6 节点航速 [m/s]，v_def: 默认航速\n' ...
        'v = v_def;\n' ...
        'if kx(1) < 0\n' ...
        '    return;\n' ...
        'end\n' ...
        'if x <= kx(1)\n' ...
        '    v = kv(1);\n' ...
        '    return;\n' ...
        'end\n' ...
        'for k = 1:5\n' ...
        '    if kx(k+1) < 0\n' ...
        '        v = kv(k);\n' ...
        '        return;\n' ...
        '    end\n' ...
        '    if x <= kx(k+1)\n' ...
        '        w = (x - kx(k)) / max(kx(k+1) - kx(k), 1e-6);\n' ...
        '        v = kv(k) + w * (kv(k+1) - kv(k));\n' ...
        '        return;\n' ...
        '    end\n' ...
        'end\n' ...
        'v = kv(6);\n' ...
        'end\n']);
end

function c = CODE_CURRENT()
    c = sprintf([ ...
        'function vc = current_profile(x, kx, kv, v_def)\n' ...
        '%% 水流速度剖面：按里程节点分段线性（负=逆水，正=顺水；节点 -1 结束）\n' ...
        'vc = v_def;\n' ...
        'if kx(1) < 0\n' ...
        '    return;\n' ...
        'end\n' ...
        'if x <= kx(1)\n' ...
        '    vc = kv(1);\n' ...
        '    return;\n' ...
        'end\n' ...
        'for k = 1:3\n' ...
        '    if kx(k+1) < 0\n' ...
        '        vc = kv(k);\n' ...
        '        return;\n' ...
        '    end\n' ...
        '    if x <= kx(k+1)\n' ...
        '        w = (x - kx(k)) / max(kx(k+1) - kx(k), 1e-6);\n' ...
        '        vc = kv(k) + w * (kv(k+1) - kv(k));\n' ...
        '        return;\n' ...
        '    end\n' ...
        'end\n' ...
        'vc = kv(4);\n' ...
        'end\n']);
end

function c = CODE_RESIST()
    c = sprintf([ ...
        'function m = resist_profile(x, profile, pos, amp, rampT, bands, v_target)\n' ...
        '%% 阻力倍率剖面（距离触发，x 平滑过渡；无状态，适合连续采样）\n' ...
        '%% 过渡带宽度 = v_target * rampT（约 10~20 s 对应距离）；bands: 4x3 [起点 终点 幅值]，起点<0 结束\n' ...
        'dx = v_target * max(rampT, 1e-6);\n' ...
        'r = min(max((x - pos) / max(dx, 1e-6), 0), 1);\n' ...
        'switch profile\n' ...
        '    case 0\n' ...
        '        m = 1;\n' ...
        '    case 1\n' ...
        '        m = 1 + amp * r;\n' ...
        '    case 2\n' ...
        '        m = 1 + amp * r;\n' ...
        '    case 3\n' ...
        '        if x >= pos, m = 1 + amp; else, m = 1; end\n' ...
        '    case 4\n' ...
        '        m = 1;\n' ...
        '        for k = 1:4\n' ...
        '            xs = bands(k, 1);\n' ...
        '            if xs < 0\n' ...
        '                break;\n' ...
        '            end\n' ...
        '            xe = bands(k, 2);\n' ...
        '            a = bands(k, 3);\n' ...
        '            r_up = min(max((x - xs) / max(dx, 1e-6), 0), 1);\n' ...
        '            r_dn = min(max((x - xe) / max(dx, 1e-6), 0), 1);\n' ...
        '            m = m + a * (r_up - r_dn);\n' ...
        '        end\n' ...
        '    otherwise\n' ...
        '        m = 1;\n' ...
        'end\n' ...
        'm = max(m, 0.05);\n' ...
        'end\n']);
end

function c = CODE_EMS_TARGET()
    c = sprintf([ ...
        'function P = ems_target(P_load, SOC)\n' ...
        '%% 传统规则型 EMS：负荷跟随 + SOC 修正（Baseline 与 AI 共用同一函数）\n' ...
        'P = P_load + ASSUMED_EMS_SOC_Gain * (ASSUMED_EMS_SOC_Ref - SOC);\n' ...
        'end\n']);
end

function c = CODE_AI_PREDICT(win)
    if nargin < 1, win = 60; end
    c = sprintf([ ...
        'function [Ppred, valid] = ai_predict(t, P_load, v_cmd, v_ship, accel, x_ship)\n' ...
        '%% AI 预测接口：输出预测峰值功率 P_pred_max_kW 与有效性 AI_valid\n' ...
        '%% AI_Pred_Source: 0=Disabled/Persistence, 1=FromWorkspace, 2=External(STM32N6 预留), 3=EmbeddedML(环内 MLP 推理)\n' ...
        '%% 注意：EMChart 局部函数不可见图级参数，MLP 推理全部内联于主函数（2026-08 调试结论）\n' ...
        'persistent hist lastT\n' ...
        'if isempty(hist)\n' ...
        '    hist = repmat([P_load, v_cmd, v_ship, accel, x_ship], %d, 1);\n' ...
        '    lastT = t - 1;\n' ...
        'end\n' ...
        'if t - lastT >= 1 - 1e-9\n' ...
        '    hist = [hist(2:end, :); P_load, v_cmd, v_ship, accel, x_ship];\n' ...
        '    lastT = t;\n' ...
        'end\n' ...
        'if AI_Pred_Source == 0\n' ...
        '    Ppred = P_load; valid = 1;\n' ...
        'elseif AI_Pred_Source == 3\n' ...
        '    if MLP_WeightsLoaded < 0.5\n' ...
        '        Ppred = P_load; valid = 0;\n' ...
        '        return;\n' ...
        '    end\n' ...
        '    x = reshape(hist.'', 1, []);\n' ...
        '    mu = repmat(MLP_FeatMean, 1, %d);\n' ...
        '    sd = repmat(MLP_FeatStd, 1, %d);\n' ...
        '    xn = (x - mu) ./ sd;\n' ...
        '    h1 = max(xn * MLP_W1 + MLP_b1, 0);\n' ...
        '    h2 = max(h1 * MLP_W2 + MLP_b2, 0);\n' ...
        '    yn = h2 * MLP_W3 + MLP_b3;\n' ...
        '    delta = max(yn * MLP_YStd + MLP_YMean, 0);\n' ...
        '    Ppred = P_load + delta;\n' ...
        '    ok = all(isfinite([Ppred, P_load, v_cmd, v_ship, accel, x_ship]));\n' ...
        '    ok = ok && Ppred >= 0 && Ppred <= ASSUMED_Pred_Bound_kW;\n' ...
        '    valid = double(ok);\n' ...
        '    Ppred = min(max(Ppred, 0), ASSUMED_Pred_Bound_kW);\n' ...
        'else\n' ...
        '    if AI_Pred_Source == 1\n' ...
        '        D = AI_Pred_Data;\n' ...
        '    else\n' ...
        '        D = STM32_Pred_Data;\n' ...
        '    end\n' ...
        '    if size(D, 1) >= 2 && ASSUMED_AI_DataValid\n' ...
        '        Ppred = lininterp1(D, t);\n' ...
        '    else\n' ...
        '        Ppred = P_load; valid = 0;\n' ...
        '        return;\n' ...
        '    end\n' ...
        '    ok = all(isfinite([Ppred, P_load, v_cmd, v_ship, accel]));\n' ...
        '    ok = ok && Ppred >= 0 && Ppred <= ASSUMED_Pred_Bound_kW;\n' ...
        '    valid = double(ok);\n' ...
        '    Ppred = min(max(Ppred, 0), ASSUMED_Pred_Bound_kW);\n' ...
        'end\n' ...
        'end\n' ...
        '\n' ...
        'function y = lininterp1(D, t)\n' ...
        '%% 标量线性插值（避免 interp1 在代码生成中的变尺寸问题）\n' ...
        'if t <= D(1, 1)\n' ...
        '    y = D(1, 2);\n' ...
        'elseif t >= D(end, 1)\n' ...
        '    y = D(end, 2);\n' ...
        'else\n' ...
        '    k = 1;\n' ...
        '    while k < size(D, 1) && D(k + 1, 1) <= t\n' ...
        '        k = k + 1;\n' ...
        '    end\n' ...
        '    w = (t - D(k, 1)) / (D(k + 1, 1) - D(k, 1));\n' ...
        '    y = D(k, 2) + w * (D(k + 1, 2) - D(k, 2));\n' ...
        'end\n' ...
        'end\n'], win, win, win);
end

function c = CODE_PROPELLER()
    c = sprintf([ ...
        'function [F_prop_eff, Q_prop, P_hydro_kW, V_A] = PropellerHydroSimple(...\n' ...
        '    n_prop_rpm, V_ship_mps, V_current_mps, wake_fraction, K_Fw, K_Fv, K_Qw, K_Qv)\n' ...
        '%% 简化螺旋桨水动力模型（复用原模型，未修改）\n' ...
        'omega = n_prop_rpm * pi / 30;\n' ...
        'V_rel = V_ship_mps - V_current_mps;\n' ...
        'V_A = (1 - wake_fraction) * V_rel;\n' ...
        'F_prop_eff = K_Fw * omega * abs(omega) - K_Fv * V_A * abs(V_A);\n' ...
        'Q_prop = K_Qw * omega * abs(omega) - K_Qv * V_A * abs(V_A);\n' ...
        'P_hydro_kW = Q_prop * omega / 1000;\n' ...
        'end\n']);
end

function c = CODE_HULL()
    c = sprintf([ ...
        'function [dVdt, R_hull] = HullLongitudinalDynamics(...\n' ...
        '    V_ship, F_prop_eff, V_current, F_env, Hull_K_R, Ship_Mass, resistance_multiplier)\n' ...
        '%% 船体纵向动力学（原模型基础上增加阻力倍率输入）\n' ...
        'V_rel = V_ship - V_current;\n' ...
        'R_hull = resistance_multiplier * Hull_K_R * V_rel * abs(V_rel);\n' ...
        'dVdt = (F_prop_eff - R_hull + F_env) / Ship_Mass;\n' ...
        'end\n']);
end

function c = CODE_BAT_LIMITS()
    c = sprintf([ ...
        'function [dis, chg] = bat_limits(SOC)\n' ...
        '%% 电池充放电可用功率（SOC 降额）\n' ...
        'if SOC <= ASSUMED_SOC_min\n' ...
        '    dis = 0;\n' ...
        'elseif SOC >= ASSUMED_SOC_DisDerate_Start\n' ...
        '    dis = ASSUMED_Bat_DischargeLimit;\n' ...
        'else\n' ...
        '    dis = ASSUMED_Bat_DischargeLimit * (SOC - ASSUMED_SOC_min) / ...\n' ...
        '        (ASSUMED_SOC_DisDerate_Start - ASSUMED_SOC_min);\n' ...
        'end\n' ...
        'if SOC >= ASSUMED_SOC_max\n' ...
        '    chg = 0;\n' ...
        'elseif SOC <= ASSUMED_SOC_ChgDerate_Start\n' ...
        '    chg = ASSUMED_Bat_ChargeLimit;\n' ...
        'else\n' ...
        '    chg = ASSUMED_Bat_ChargeLimit * (ASSUMED_SOC_max - SOC) / ...\n' ...
        '        (ASSUMED_SOC_max - ASSUMED_SOC_ChgDerate_Start);\n' ...
        'end\n' ...
        'end\n']);
end

function c = CODE_SOC_RATE()
    c = sprintf([ ...
        'function r = soc_rate(P)\n' ...
        '%% SOC 变化率（kW/kWh/s 一致换算，充放电效率分别处理）\n' ...
        'E = Bat_Cap_kWh * 3600;\n' ...
        'if P > 0\n' ...
        '    r = -(P / ASSUMED_Bat_Eff_Discharge) / E;\n' ...
        'else\n' ...
        '    r = -P * ASSUMED_Bat_Eff_Charge / E;\n' ...
        'end\n' ...
        'end\n']);
end
