function buildSafetyInterlock(mdl, pos)
% 16_Safety_Interlock（Phase 4-B T-007, FR-444~448）：独立最高优先级 Safety/Interlock 层
%   - 不直写 Plant：override 请求输出送 07_PMS_v2（经顶层 UnitDelay 0.01 s，§5.6 防环）；
%   - S1a/S1b 故障锁存 + S2 realized supply deficit（D-B14）+ S3 min gen surplus（单健康机
%     flag-only）+ S4 prospective firm-capacity（D-B9）+ S5 EMS health fallback（D-B15：
%     显式通道 EMS_HEALTH_TS From Workspace + isfinite，仅 EMS_AGGREGATE 模式报警）；
%   - 纯监视 flags（FR-446）：SOC≤0.20 / Vdc<600 / 电池三 flag 透传；
%   - 与 08_Undervolt_Derate 无任何信号直连（08 保持唯一执行性 propulsion limiter，§5.7）；
%   - 参数经 Parameter 作用域（base workspace 由 ModelE_Control_Parameters.m 提供）。
    sys = [mdl '/16_Safety_Interlock'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);

    ins = {'state_DG1','state_DG2','cmd_onoff_DG1','cmd_onoff_DG2','P_load_gen_kW', ...
        'P_prop_demand_kW','P_hotel_kW','P_DG1_kW','P_DG2_kW','P_bat_dis_avail_g_kW', ...
        'P_bat_chg_avail_g_kW','N_dispatch','SOC','Vdc_V','bat_flag_1','bat_flag_2', ...
        'bat_flag_3','P_gen_ref_kW'};
    for k = 1:numel(ins)
        add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+40*(k-1) 45 55+40*(k-1)]);
    end
    % 输出顺序 = override 总线顺序（07_PMS_v2 输入 12..19 一一对应，UD_safety_* 逐根 UnitDelay）
    outs = {'fault_latch','force_start_req','force_start_reason','force_stop_req', ...
        'force_stop_reason','ems_health_ok','baseline_fallback_active','baseline_fallback_reason', ...
        'min_gen_surplus_unresolved','D_firm_kW','S2_supply_gap_kW', ...
        'flag_soc_low','flag_vdc_low','flag_bat_passthrough'};
    for k = 1:numel(outs)
        add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [640 30+42*(k-1) 670 55+42*(k-1)]);
    end

    fn = [sys '/PMS_Safety_Interlock'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [200 60 360 300]);
    setMFCode(fn, model.code_safety_interlock_e(), ...
        {'PMS_S2_hold_ticks','PMS_S4_hold_ticks','PMS_start_fail_ticks','PMS_firm_margin_kW', ...
         'DG_P_rated_kW','DG_P_min_kW','ASSUMED_eta_rect'});
    % 显式输出尺寸（向量输出 [1×2]/[1×3] 需显式声明，避免基础分析无法定尺寸）
    rt = sfroot;
    chS = rt.find('-isa', 'Stateflow.EMChart', 'Path', fn);
    dOut = chS.find('-isa', 'Stateflow.Data');
    sizeMap = containers.Map({'force_start_req','force_start_reason','force_stop_req','force_stop_reason', ...
        'fault_latch','ems_health_ok','baseline_fallback_active','baseline_fallback_reason', ...
        'min_gen_surplus_unresolved','D_firm_kW','S2_supply_gap_kW','flag_soc_low','flag_vdc_low', ...
        'flag_bat_passthrough'}, {'[1 2]','1','[1 2]','1','[1 2]','1','1','1','1','1','1','1','1','[1 3]'});
    for k = 1:numel(dOut)
        if strcmp(dOut(k).Scope, 'Output') && isKey(sizeMap, dOut(k).Name)
            dOut(k).Props.Array.Size = sizeMap(dOut(k).Name);
        end
    end
    setDiscreteMF(fn, '0.01');   % persistent 状态块（fault 锁存/hold 计数）须离散采样

    % 信号输入（端口序 = 函数签名序，参数已被 Parameter 作用域吸收）
    addL(sys, 'state_DG1', 1, 'PMS_Safety_Interlock', 1);
    addL(sys, 'state_DG2', 1, 'PMS_Safety_Interlock', 2);
    addL(sys, 'cmd_onoff_DG1', 1, 'PMS_Safety_Interlock', 3);
    addL(sys, 'cmd_onoff_DG2', 1, 'PMS_Safety_Interlock', 4);
    addL(sys, 'P_load_gen_kW', 1, 'PMS_Safety_Interlock', 5);
    addL(sys, 'P_prop_demand_kW', 1, 'PMS_Safety_Interlock', 6);
    addL(sys, 'P_hotel_kW', 1, 'PMS_Safety_Interlock', 7);
    addL(sys, 'P_DG1_kW', 1, 'PMS_Safety_Interlock', 8);
    addL(sys, 'P_DG2_kW', 1, 'PMS_Safety_Interlock', 9);
    addL(sys, 'P_bat_dis_avail_g_kW', 1, 'PMS_Safety_Interlock', 10);
    addL(sys, 'P_bat_chg_avail_g_kW', 1, 'PMS_Safety_Interlock', 11);
    addL(sys, 'N_dispatch', 1, 'PMS_Safety_Interlock', 12);
    addL(sys, 'SOC', 1, 'PMS_Safety_Interlock', 13);
    addL(sys, 'Vdc_V', 1, 'PMS_Safety_Interlock', 14);
    addL(sys, 'bat_flag_1', 1, 'PMS_Safety_Interlock', 15);
    addL(sys, 'bat_flag_2', 1, 'PMS_Safety_Interlock', 16);
    addL(sys, 'bat_flag_3', 1, 'PMS_Safety_Interlock', 17);
    addL(sys, 'P_gen_ref_kW', 1, 'PMS_Safety_Interlock', 18);
    % ems_health_raw：From Workspace EMS_HEALTH_TS（FR-448 注入通道，Interpolate off）
    add_block('simulink/Sources/From Workspace', [sys '/ems_health_src'], 'Position', [60 330 140 355], ...
        'VariableName', 'EMS_HEALTH_TS', 'SampleTime', '0.01', 'Interpolate', 'off', ...
        'OutputAfterFinalValue', 'HoldingFinalValue');
    addL(sys, 'ems_health_src', 1, 'PMS_Safety_Interlock', 19);
    % nominal_is_ems：PMS_SOURCE_MODE 编译期常量（0=BASELINE_POLICY 1=EMS_AGGREGATE）
    add_block('simulink/Sources/Constant', [sys '/C_source_mode'], 'Position', [60 370 120 390], ...
        'Value', 'PMS_SOURCE_MODE');
    addL(sys, 'C_source_mode', 1, 'PMS_Safety_Interlock', 20);
    % voyage_active：v1 仿真运行期即航次激活（停止即仿真终止；0DG 港内工况留 C 包，FR-439）
    add_block('simulink/Sources/Constant', [sys '/C_voyage_active'], 'Position', [60 400 120 420], ...
        'Value', '1');
    addL(sys, 'C_voyage_active', 1, 'PMS_Safety_Interlock', 21);
    % 输出（函数签名序 → 总线序显式映射：fn 返回序 1..14 =
    % force_start_req,force_start_reason,force_stop_req,force_stop_reason,fault_latch,
    % ems_health_ok,fallback_active,fallback_reason,min_surplus,D_firm,S2_gap,soc,vdc,bat）
    fnPortForOut = [5 1 2 3 4 6 7 8 9 10 11 12 13 14];
    for k = 1:numel(outs)
        addL(sys, 'PMS_Safety_Interlock', fnPortForOut(k), outs{k}, 1);
    end
    set_param(sys, 'MaskDisplay', ['disp(sprintf(''16_Safety_Interlock\nS1~S5 + monitor (FR-444)\n不直写 Plant''))']);
end
function addL(sys, srcBlk, srcPort, dstBlk, dstPort)
% 句柄式 add_line（R2026a：子系统内 MATLAB Function 块字符串端口解析不可靠，A2 T-010 教训）
ps = get_param([sys '/' srcBlk], 'PortHandles');
pd = get_param([sys '/' dstBlk], 'PortHandles');
add_line(sys, ps.Outport(srcPort), pd.Inport(dstPort));
end

function setDiscreteMF(blkPath, st)
% MATLAB Function 块离散化（persistent 状态块必需）：EMChart.ChartUpdate=DISCRETE
rt = sfroot;
ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', blkPath);
if numel(ch) ~= 1, error('MATLAB Function 块未找到: %s', blkPath); end
ch.ChartUpdate = 'DISCRETE';
ch.SampleTime = st;
end

function setMFCode(blkPath, code, paramNames)
% 通过 Stateflow.EMChart 设置块代码并把未声明标识符改写为 Parameter 作用域
% （A2 setMFCode 单遍路径；本批块无 twoPass 需求——参数名单与脚本签名一致）
    if nargin < 3, paramNames = {}; end
    rt = sfroot;
    ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', blkPath);
    if numel(ch) ~= 1, error('MATLAB Function 块未找到: %s', blkPath); end
    ch.Script = code;
    ds = ch.find('-isa', 'Stateflow.Data');
    for i = 1:numel(paramNames)
        d = [];
        for j = 1:numel(ds)
            if strcmp(ds(j).Name, paramNames{i}), d = ds(j); break; end
        end
        if isempty(d)
            d = Stateflow.Data(ch);
            d.Name = paramNames{i};
        end
        d.Scope = 'Parameter';
        d.DataType = 'double';
        d.Props.Array.Size = '1';
    end
end
