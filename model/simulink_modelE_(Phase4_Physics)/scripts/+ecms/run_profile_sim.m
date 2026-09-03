function prof = run_profile_sim(ROOT, SCN, law, stopTimeLimit)
% ecms.run_profile_sim — 单场景单控制律仿真并提取 1 s 均匀网格负荷剖面（T-006/T-007，FR-006）
%
% P_demand 因果隔离提取点（FR-006 / design §5，位于欠压降额与 EMS 作用的上游）：
%   P_demand(t) = P_prop_demand_kW(t) + P_hotel_kW(t)
%   信号路径与结构性证据：
%     02_Ship_Speed_Controller/P_prop_demand_kW   <- 速度 PI 输出（顶层 02/1 端口）
%        ├─> 03_Propulsion_Plant/1   内部 Avail = min(demand, P_prop_limit_kW)
%        │                            才施加欠压降额限幅（降额作用的下游起点）
%        └─> 08_Undervolt_Derate/2   降额块仅以该需求指令为输入计算 P_prop_limit
%     P_hotel_kW <- 01_Scenario_Manager 日用负荷指令（外生，不依赖 EMS/母线状态）
%   对照（下游，不可用作需求剖面）：04_Load_Measurement 的 P_load_total_kW
%     = max(P_prop_actual_kW,0) + P_hotel_kW，其中 P_prop_actual 已含降额限幅
%     与驱动滞后；EMS（05_EMS_Controller）仅见 P_load_total_kW。
%   因此本提取点不受 EMS 控制律与欠压降额影响（控制不变的外生需求）。
%   两信号均由模型 configureLogging 写入 logsout，无需修改 .slx。
%
% 输入:
%   ROOT          工程根目录（model/simulink_modelD）
%   SCN           工况结构体（config/Model_Scenarios.m）
%   law           EMS_CTRL_LAW：0=Baseline(无预测) 1=AI_Enhanced（本任务固定
%                 AI_Pred_Source=0 persistence，行为等同 Baseline）
%   stopTimeLimit 仿真 StopTime 安全上限 [s]（11_Voyage_Stop_Logic 到达航程提前终止）
% 输出:
%   prof 结构体（全部信号为 1 s 均匀网格列向量），字段：
%     scenario_name / scenario_id / law / t / P_demand_kW / P_prop_demand_kW /
%     P_hotel_kW / P_gen_kW / P_bat_kW / SOC / V_dc_V / v_ship_mps / x_ship_m /
%     P_load_total_kW / P_bus_residual_kW / fuel_total_kg（模型内默认 BSFC 表积分，
%     仅供参考列）/ duration_s / Vdc_min_V / derate_event 与降额事件明细字段
% 前置: 调用方已在 base 工作区运行 config/Model_Parameters.m 与 config/Model_Scenarios.m，
%       且已 load_system('HybridShip_ModelD_DCBus')（+model/+ecms 包在 path）。
    MDL = 'HybridShip_ModelD_DCBus';
    if ~bdIsLoaded(MDL)
        load_system(fullfile(ROOT, 'model', [MDL '.slx']));
    end

    % ---- 预测通道全关（Fixed Decision：两种控制律均无预测）----
    assignin('base', 'AI_Pred_Source', 0);
    assignin('base', 'AI_TPeak_Source', 0);
    if law >= 1
        % law=1 激活 AI 变体：ai_predict 图表的 MLP_* 参数须满足编译期尺寸检查
        % （AI_Pred_Source=0 时 MLP 路径不执行，仅需尺寸匹配；不加载任何权重）
        ensureMlpParamSizes(MDL);
    end

    % ---- 运行仿真（复用既有 run 入口：写入 SCN/EMS_CTRL_LAW/Bat_SOC_init）----
    out = model.run_scenario_sim(MDL, SCN, law, stopTimeLimit);
    ds0 = model.extract_logsout(out);

    % ---- 1 s 重采样（分段常数：取每 1 s 窗口末值，与既有数据集导出一致）----
    ds = model.resample_1s(ds0);
    t = ds.t;

    % ---- P_demand 上游提取 ----
    P_prop_demand_kW = ds.P_prop_demand_kW;
    P_hotel_kW       = ds.P_hotel_kW;
    P_demand_kW      = P_prop_demand_kW + P_hotel_kW;

    % ---- 欠压降额事件检查 ----
    Mot_P_nom = evalin('base', 'Mot_P_nom');
    Vdc = ds.V_dc_V;
    n_undervoltage = sum(Vdc < 600);                          % 瞬态带下限 600 V（design §5 / CCS）
    n_derate       = sum(ds.P_prop_limit_kW < Mot_P_nom - 1e-6); % 降额系数 f<1 激活
    n_alarm_transient = sum(ds.alarm_transient > 0.5);        % CCS 瞬态越限报警
    E_unserved_kWh = sum(ds.P_unserved_kW) / 3600;            % dt=1 s
    derate_event = (n_undervoltage > 0) || (n_derate > 0) || (max(ds.P_unserved_kW) > 1e-6);

    % ---- 能量闭合（在原始 0.01 s 网格计算；design §4.1 效率链）----
    % 母线物理: η_rect·P_gen + P_bat(母线侧) = P_load_total + P_bus_residual(=C·V·dV/dt)
    % 需求侧:   P_demand = P_prop_demand + P_hotel = P_load_total + (P_prop_demand − P_prop_actual)
    % 因此 Σ(η_rect·P_gen + P_bat − P_demand) = ΔE_bus_cap − τ_lag·P_prop_actual(T)（驱动滞后瞬态）
    % 注意: 1 s 重采样会给滞后项引入系统偏差（实测可达 2.5×），故闭合校验必须在原始
    % 网格上进行；1 s 残差仅作参考列（不是能量不一致，而是重采样伪差）。
    eta_rect = evalin('base', 'ASSUMED_eta_rect');
    tr = ds0.t; dtr = [diff(tr); tr(2) - tr(1)];
    Ein_raw  = sum((eta_rect .* ds0.P_gen_actual_kW + ds0.P_bat_actual_kW) .* dtr) / 3600;
    Edem_raw = sum((ds0.P_prop_demand_kW + ds0.P_hotel_kW) .* dtr) / 3600;
    Ebus_raw = sum(ds0.P_bus_residual_kW .* dtr) / 3600;
    Eserved_raw = sum(ds0.P_load_total_kW .* dtr) / 3600;
    residual_raw = (Ein_raw - Edem_raw) / Edem_raw;
    Ein_1s  = sum(eta_rect .* ds.P_gen_actual_kW + ds.P_bat_actual_kW) / 3600;
    Edem_1s = sum(P_demand_kW) / 3600;
    residual_1s = (Ein_1s - Edem_1s) / Edem_1s;
    closure_pass = abs(residual_raw) < 1e-3;   % <0.1%（AC：能量闭合）

    % ---- 组装输出 ----
    prof = struct();
    prof.scenario_name = SCN.name;
    prof.scenario_id   = double(SCN.id);
    prof.law           = double(law);
    prof.t             = t;
    prof.P_demand_kW   = P_demand_kW;          % 上游外生需求（推进需求指令 + 日用负荷）
    prof.P_prop_demand_kW = P_prop_demand_kW;  % 推进需求指令（速度 PI 输出，降额前）
    prof.P_hotel_kW    = P_hotel_kW;           % 日用负荷指令
    prof.P_gen_kW      = ds.P_gen_actual_kW;   % 发电机实际功率（聚合）
    prof.P_bat_kW      = ds.P_bat_actual_kW;   % 电池母线侧功率（正=放电；已含 DC/DC 效率折算）
    prof.SOC           = ds.SOC;
    prof.V_dc_V        = Vdc;
    prof.v_ship_mps    = ds.v_ship_mps;        % 船速（剖面完整性核对列）
    prof.x_ship_m      = ds.x_ship_m;          % 航程（剖面完整性核对列）
    prof.P_load_total_kW = ds.P_load_total_kW; % 降额后实际服务负荷（对照列，非需求）
    prof.P_bus_residual_kW = ds.P_bus_residual_kW; % 母线功率残差（= C·V·dV/dt）
    prof.fuel_total_kg = ds.fuel_total_kg(end);% 模型内默认 BSFC 表积分值（仅供参考列）
    prof.duration_s    = t(end);
    prof.Vdc_min_V     = min(Vdc);
    prof.derate_event  = derate_event;         % true => 剖面作废（FR-006）
    prof.n_undervoltage_samples = n_undervoltage;
    prof.n_derate_samples       = n_derate;
    prof.n_alarm_transient_samples = n_alarm_transient;
    prof.E_unserved_kWh = E_unserved_kWh;
    prof.energy_closure = struct( ...
        'E_in_kWh', Ein_raw, 'E_demand_kWh', Edem_raw, ...
        'E_bus_residual_kWh', Ebus_raw, ...          % = ΔE_bus_cap
        'E_lag_kWh', Eserved_raw - Edem_raw, ...     % = −τ·y(T) 驱动滞后瞬态
        'residual_frac', residual_raw, ...
        'residual_1s_frac', residual_1s, ...         % 参考列（重采样伪差，不作判据）
        'pass', closure_pass, 'grid', 'raw 0.01 s');
end

function ensureMlpParamSizes(mdl)
% 确保 base 中 MLP_* 参数的尺寸与 AI 变体 ai_predict 图表声明一致（编译期检查）。
% AI_Pred_Source=0 时 MLP 推理路径不执行（ai_predict 直接返回 Ppred=P_load），
% 因此只需尺寸匹配的占位零值，且保持 MLP_WeightsLoaded=0（不加载任何权重）。
    rt = sfroot;
    ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', ...
        [mdl '/05_EMS_Controller/EMS_AI_Enhanced/ai_predict']);
    if numel(ch) ~= 1
        return;
    end
    ds = ch.find('-isa', 'Stateflow.Data');
    for i = 1:numel(ds)
        nm = ds(i).Name;
        if ~strcmp(ds(i).Scope, 'Parameter') || ~startsWith(nm, 'MLP_')
            continue;
        end
        szStr = ds(i).Props.Array.Size;
        if contains(szStr, '-1')
            continue;   % 标量/动态尺寸参数（如 MLP_WeightsLoaded）无需处理
        end
        sz = str2double(regexp(szStr, '\d+', 'match'));
        if isempty(sz) || any(isnan(sz))
            continue;
        end
        sz = sz(:).';
        okSize = false;
        if evalin('base', sprintf('exist(''%s'', ''var'')', nm))
            v = evalin('base', nm);
            okSize = isnumeric(v) && isequal(size(v), sz);
        end
        if ~okSize
            assignin('base', nm, zeros(sz));
            fprintf('[ecms] %s 已按图表声明尺寸 %s 置占位零值（AI_Pred_Source=0，不加载权重）\n', ...
                nm, sprintf('%dx%d', sz));
        end
    end
end
