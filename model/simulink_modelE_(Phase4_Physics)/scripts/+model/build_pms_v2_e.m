function buildPMSv2(mdl, pos)
% 07_PMS_v2（Phase 4-B T-007, FR-449/450/451A）：四层 PMS 替换 Legacy_Commitment_Adapter
%   内部：X_b2g_* 输入边界转换（DD-1，genset-side canonical 域）+ Baseline_Demand_Policy
%   + Source_Selector（PMS_SOURCE_MODE 编译期 + S5 runtime override）+
%   DG_Commitment_Supervisor（Stateflow chart，唯一写 DG cmd）+ DG_Dispatch_Allocator
%   + 输出级（AUTO/force 双路，DD-8 保名 ADAPTER_FORCE_*；D-B16 停机向量）。
%   端口契约 = design §5.1；N_dispatch→Demand 经 UnitDelay 前一拍反馈（§5.3）；
%   16 号 override 经 UnitDelay 0.01 s 进入（拍内防环，§5.6）。
    sys = [mdl '/07_PMS_v2'];
    add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', pos);
    delete_block([sys '/In1']); delete_block([sys '/Out1']);

    % ================= 输入端口（In1..In18） =================
    ins = {'P_gen_ref_kW','P_load_total_kW','SOC','P_bat_dis_avail_kW','P_bat_chg_avail_kW', ...
        'P_prop_demand_kW','P_hotel_kW','state_DG1','state_DG2','P_DG1_kW','P_DG2_kW', ...
        'fault_latch','force_start_req','force_start_reason','force_stop_req','force_stop_reason', ...
        'ems_health_ok','baseline_fallback_active','baseline_fallback_reason'};
    for k = 1:numel(ins)
        add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+42*(k-1) 45 55+42*(k-1)]);
    end
    % ================= 输出端口（Out1..Out34） =================
    outs = {'DG1_enable','P_DG1_ref_kW','DG2_enable','P_DG2_ref_kW','DG_cmd_stop', ...
        'P_load_gen_kW','P_bat_dis_avail_g_kW','P_bat_chg_avail_g_kW', ...
        'P_commit_kW','P_gen_req_eff_kW','N_dispatch','P_gen_unserved_kW','P_gen_surplus_kW', ...
        'dispatch_min_clamp_active','dispatch_n_zero_flag','cmd_onoff_DG1','cmd_onoff_DG2', ...
        'commitment_state','reason_code','u_recovery','P_recovery_actual_kW', ...
        'start_hold_timer','stop_hold_timer','online_timer', ...
        'audit_valid','audit_old_state','audit_new_state','audit_P_commit_kW', ...
        'audit_t_start_hold','audit_t_stop_hold','audit_t_online','audit_safety_override', ...
        'selector_baseline_active','out_baseline_fallback_active','out_baseline_fallback_reason'};
    for k = 1:numel(outs)
        add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [900 30+38*(k-1) 930 55+38*(k-1)]);
    end

    % ================= X_b2g_* 输入边界转换（DD-1，唯一允许的域转换点） =================
    % bus → genset-equivalent：P_gen = P_bus / ASSUMED_eta_rect（不等式同除正数保序）
    add_block('simulink/Math Operations/Gain', [sys '/X_b2g_P_load'], 'Position', [80 90 130 120], ...
        'Gain', '1/ASSUMED_eta_rect');
    addL(sys, 'P_load_total_kW', 1, 'X_b2g_P_load', 1);
    add_block('simulink/Math Operations/Gain', [sys '/X_b2g_bat_dis'], 'Position', [80 140 130 170], ...
        'Gain', '1/ASSUMED_eta_rect');
    addL(sys, 'P_bat_dis_avail_kW', 1, 'X_b2g_bat_dis', 1);
    add_block('simulink/Math Operations/Gain', [sys '/X_b2g_bat_chg'], 'Position', [80 190 130 220], ...
        'Gain', '1/ASSUMED_eta_rect');
    addL(sys, 'P_bat_chg_avail_kW', 1, 'X_b2g_bat_chg', 1);

    % Plant 状态反馈经 UnitDelay 0.01 s（NFR-417 无新代数环：GensetUnitE 输出对 cmd 直接馈通，
    %   与 A2 Wake_vs_HT 的 HT_ratio UD 松弛同一手法；0.01 s 决策延迟对判据无影响）
    add_block('simulink/Discrete/Unit Delay', [sys '/UD_state_DG1'], 'Position', [360 40 395 120], ...
        'SampleTime', '0.01', 'InitialCondition', '0');
    add_block('simulink/Discrete/Unit Delay', [sys '/UD_state_DG2'], 'Position', [360 130 395 150], ...
        'SampleTime', '0.01', 'InitialCondition', '0');
    addL(sys, 'state_DG1', 1, 'UD_state_DG1', 1);
    addL(sys, 'state_DG2', 1, 'UD_state_DG2', 1);

    % ================= Baseline_Demand_Policy（FR-432~435，MATLAB Function 离散） =================
    fnD = [sys '/PMS_Demand_Policy'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fnD, 'Position', [170 120 290 220]);
    setMFCode(fnD, model.code_pms_demand_e(), ...
        {'PMS_LoadLPF_tau_s','PMS_SOC_lo','PMS_SOC_recovered','PMS_P_chg_recovery_kW','DG_P_rated_kW'});
    setDiscreteMF(fnD, '0.01');    % persistent 状态块（LPF/滞回）须离散采样
    addL(sys, 'X_b2g_P_load', 1, 'PMS_Demand_Policy', 1);
    addL(sys, 'SOC', 1, 'PMS_Demand_Policy', 2);
    % N_dispatch 前一拍反馈（§5.3）在 Dispatch 块创建后接线（见下）

    % ================= Source_Selector（FR-450：编译期 + S5 runtime override） =================
    % baseline_active = (PMS_SOURCE_MODE<0.5) ∨ (ems_health_ok<0.5)；Switch u2>=0.5 → 输入1=baseline
    add_block('simulink/Sources/Constant', [sys '/C_source_mode'], 'Position', [80 280 120 300], ...
        'Value', 'PMS_SOURCE_MODE');
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/cmp_mode_baseline'], ...
        'Position', [150 280 200 305], 'const', '0.5', 'relop', '<');
    addL(sys, 'C_source_mode', 1, 'cmp_mode_baseline', 1);
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/cmp_health_bad'], ...
        'Position', [150 315 200 340], 'const', '0.5', 'relop', '<');
    addL(sys, 'ems_health_ok', 1, 'cmp_health_bad', 1);
    add_block('simulink/Logic and Bit Operations/Logical Operator', [sys '/or_baseline'], ...
        'Position', [220 290 250 325], 'Operator', 'OR', 'Inputs', '2');
    addL(sys, 'cmp_mode_baseline', 1, 'or_baseline', 1);
    addL(sys, 'cmp_health_bad', 1, 'or_baseline', 2);
    add_block('simulink/Signal Routing/Switch', [sys '/Source_Selector'], 'Position', [300 150 330 230], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'PMS_Demand_Policy', 1, 'Source_Selector', 1);   % P_gen_req_baseline
    addL(sys, 'or_baseline', 1, 'Source_Selector', 2);
    addL(sys, 'P_gen_ref_kW', 1, 'Source_Selector', 3);        % EMS 聚合（genset-side 零换算）

    % ================= DG_Commitment_Supervisor（Stateflow chart，FR-436~440） =================
    chartBlk = [sys '/PMS_Commitment_Supervisor'];
    model.build_commitment_chart_e(chartBlk);
    addL(sys, 'PMS_Demand_Policy', 2, 'PMS_Commitment_Supervisor', 1);   % P_commit_kW（无恢复偏置，D-B4）
    addL(sys, 'UD_state_DG2', 1, 'PMS_Commitment_Supervisor', 2);        % state_DG2（前一拍，防环）
    % 16 号 override（经 UD 到达）拆分喂 chart
    add_block('simulink/Signal Routing/Demux', [sys '/fault_demux'], 'Position', [60 380 70 420], 'Outputs', '2');
    addL(sys, 'fault_latch', 1, 'fault_demux', 1);
    addL(sys, 'fault_demux', 1, 'PMS_Commitment_Supervisor', 3);
    addL(sys, 'fault_demux', 2, 'PMS_Commitment_Supervisor', 4);
    add_block('simulink/Signal Routing/Demux', [sys '/fs_req_demux'], 'Position', [60 430 70 470], 'Outputs', '2');
    addL(sys, 'force_start_req', 1, 'fs_req_demux', 1);
    addL(sys, 'fs_req_demux', 2, 'PMS_Commitment_Supervisor', 5);
    addL(sys, 'force_start_reason', 1, 'PMS_Commitment_Supervisor', 6);
    add_block('simulink/Signal Routing/Demux', [sys '/fstop_req_demux'], 'Position', [60 480 70 520], 'Outputs', '2');
    addL(sys, 'force_stop_req', 1, 'fstop_req_demux', 1);
    addL(sys, 'fstop_req_demux', 2, 'PMS_Commitment_Supervisor', 7);
    addL(sys, 'force_stop_reason', 1, 'PMS_Commitment_Supervisor', 8);

    % ================= DG_Dispatch_Allocator（FR-441~443，MATLAB Function 组合逻辑） =================
    fnA = [sys '/PMS_Dispatch_Allocator'];
    add_block('simulink/User-Defined Functions/MATLAB Function', fnA, 'Position', [420 120 540 220]);
    setMFCode(fnA, model.code_dispatch_e(), {'DG_P_min_kW','DG_P_rated_kW'});
    addL(sys, 'Source_Selector', 1, 'PMS_Dispatch_Allocator', 1);   % P_gen_req_eff

    addL(sys, 'UD_state_DG1', 1, 'PMS_Dispatch_Allocator', 2);
    addL(sys, 'UD_state_DG2', 1, 'PMS_Dispatch_Allocator', 3);
    % dispatch_eligible_i = NOT fault_latch_i（fault 裁决后语义，FR-441；经 UD 到达）
    add_block('simulink/Logic and Bit Operations/Logical Operator', [sys '/elig_not1'], ...
        'Position', [370 240 390 260], 'Operator', 'NOT');
    addL(sys, 'fault_demux', 1, 'elig_not1', 1);
    add_block('simulink/Logic and Bit Operations/Logical Operator', [sys '/elig_not2'], ...
        'Position', [370 270 390 290], 'Operator', 'NOT');
    addL(sys, 'fault_demux', 2, 'elig_not2', 1);
    addL(sys, 'elig_not1', 1, 'PMS_Dispatch_Allocator', 4);
    addL(sys, 'elig_not2', 1, 'PMS_Dispatch_Allocator', 5);
    % N_dispatch 前一拍反馈（§5.3：经 UnitDelay 切断 demand→commitment→dispatch→demand 拍内环）
    add_block('simulink/Discrete/Unit Delay', [sys '/UD_N_dispatch_fb'], 'Position', [560 90 595 125], ...
        'SampleTime', '0.01', 'InitialCondition', '0');
    addL(sys, 'PMS_Dispatch_Allocator', 3, 'UD_N_dispatch_fb', 1);
    addL(sys, 'UD_N_dispatch_fb', 1, 'PMS_Demand_Policy', 3);

    % ================= 输出级：AUTO（chart+dispatch）vs force（DD-8 保名通道） =================
    % force（bench 测试台直接指令，A2 语义逐位兼容）：enable_i=(force_Pi>0)、ref_i=force_Pi、
    % stop=[0;0]（bench 停机仍走 GEN_CMD_STOP_TS 注入通道）；AUTO：enable=cmd_onoff、
    % ref=dispatch、stop=[1−onoff]（D-B16：enable→cmd_start、stop 向量→06/5 Max 合成）。
    add_block('simulink/Sources/Constant', [sys '/C_force_mode'], 'Position', [370 310 420 330], ...
        'Value', 'ADAPTER_FORCE_MODE');
    add_block('simulink/Sources/Constant', [sys '/C_force_P1'], 'Position', [370 340 420 360], ...
        'Value', 'ADAPTER_FORCE_P1_KW');
    add_block('simulink/Sources/Constant', [sys '/C_force_P2'], 'Position', [370 370 420 390], ...
        'Value', 'ADAPTER_FORCE_P2_KW');
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/cmp_force_on'], ...
        'Position', [450 305 495 330], 'const', '0.5', 'relop', '>=');
    addL(sys, 'C_force_mode', 1, 'cmp_force_on', 1);
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/cmp_fP1_pos'], ...
        'Position', [450 340 495 365], 'const', '0', 'relop', '>');
    addL(sys, 'C_force_P1', 1, 'cmp_fP1_pos', 1);
    add_block('simulink/Logic and Bit Operations/Compare To Constant', [sys '/cmp_fP2_pos'], ...
        'Position', [450 370 495 395], 'const', '0', 'relop', '>');
    addL(sys, 'C_force_P2', 1, 'cmp_fP2_pos', 1);
    % enable 选择（u2>=0.5 → 输入1=force 派生；否则输入3=AUTO chart cmd）
    add_block('simulink/Signal Routing/Switch', [sys '/SW_EN1'], 'Position', [560 60 590 105], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'cmp_fP1_pos', 1, 'SW_EN1', 1);
    addL(sys, 'cmp_force_on', 1, 'SW_EN1', 2);
    addL(sys, 'PMS_Commitment_Supervisor', 1, 'SW_EN1', 3);
    add_block('simulink/Signal Routing/Switch', [sys '/SW_EN2'], 'Position', [560 130 590 175], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'cmp_fP2_pos', 1, 'SW_EN2', 1);
    addL(sys, 'cmp_force_on', 1, 'SW_EN2', 2);
    addL(sys, 'PMS_Commitment_Supervisor', 2, 'SW_EN2', 3);
    % ref 选择（force_Pi vs dispatch P_i_ref）
    add_block('simulink/Signal Routing/Switch', [sys '/SW_REF1'], 'Position', [560 200 590 245], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'C_force_P1', 1, 'SW_REF1', 1);
    addL(sys, 'cmp_force_on', 1, 'SW_REF1', 2);
    addL(sys, 'PMS_Dispatch_Allocator', 1, 'SW_REF1', 3);
    add_block('simulink/Signal Routing/Switch', [sys '/SW_REF2'], 'Position', [560 270 590 315], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'C_force_P2', 1, 'SW_REF2', 1);
    addL(sys, 'cmp_force_on', 1, 'SW_REF2', 2);
    addL(sys, 'PMS_Dispatch_Allocator', 2, 'SW_REF2', 3);
    % stop 向量：AUTO = [1−onoff_1; 1−onoff_2]；force = [0;0]
    add_block('simulink/Math Operations/Sum', [sys '/inv1'], 'Position', [560 340 585 365], 'Inputs', '+-');
    add_block('simulink/Sources/Constant', [sys '/C_one'], 'Position', [520 335 545 355], 'Value', '1');
    addL(sys, 'C_one', 1, 'inv1', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 1, 'inv1', 2);
    add_block('simulink/Math Operations/Sum', [sys '/inv2'], 'Position', [560 375 585 400], 'Inputs', '+-');
    addL(sys, 'C_one', 1, 'inv2', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 2, 'inv2', 2);
    add_block('simulink/Signal Routing/Mux', [sys '/stop_mux'], 'Position', [610 345 620 395], 'Inputs', '2');
    addL(sys, 'inv1', 1, 'stop_mux', 1);
    addL(sys, 'inv2', 1, 'stop_mux', 2);
    add_block('simulink/Sources/Constant', [sys '/C_stop_zero'], 'Position', [560 410 590 430], ...
        'Value', '[0 0]');
    add_block('simulink/Signal Routing/Switch', [sys '/SW_STOP'], 'Position', [640 360 670 405], ...
        'Criteria', 'u2 >= Threshold', 'Threshold', '0.5');
    addL(sys, 'C_stop_zero', 1, 'SW_STOP', 1);
    addL(sys, 'cmp_force_on', 1, 'SW_STOP', 2);
    addL(sys, 'stop_mux', 1, 'SW_STOP', 3);

    % ================= 输出接线 =================
    addL(sys, 'SW_EN1', 1, 'DG1_enable', 1);
    addL(sys, 'SW_REF1', 1, 'P_DG1_ref_kW', 1);
    addL(sys, 'SW_EN2', 1, 'DG2_enable', 1);
    addL(sys, 'SW_REF2', 1, 'P_DG2_ref_kW', 1);
    addL(sys, 'SW_STOP', 1, 'DG_cmd_stop', 1);
    % 域内量与诊断/审计导出
    addL(sys, 'X_b2g_P_load', 1, 'P_load_gen_kW', 1);
    addL(sys, 'X_b2g_bat_dis', 1, 'P_bat_dis_avail_g_kW', 1);
    addL(sys, 'X_b2g_bat_chg', 1, 'P_bat_chg_avail_g_kW', 1);
    addL(sys, 'PMS_Demand_Policy', 2, 'P_commit_kW', 1);
    addL(sys, 'Source_Selector', 1, 'P_gen_req_eff_kW', 1);
    addL(sys, 'PMS_Dispatch_Allocator', 3, 'N_dispatch', 1);
    addL(sys, 'PMS_Dispatch_Allocator', 4, 'P_gen_unserved_kW', 1);
    addL(sys, 'PMS_Dispatch_Allocator', 5, 'P_gen_surplus_kW', 1);
    addL(sys, 'PMS_Dispatch_Allocator', 6, 'dispatch_min_clamp_active', 1);
    addL(sys, 'PMS_Dispatch_Allocator', 7, 'dispatch_n_zero_flag', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 1, 'cmd_onoff_DG1', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 2, 'cmd_onoff_DG2', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 3, 'commitment_state', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 4, 'reason_code', 1);
    addL(sys, 'PMS_Demand_Policy', 3, 'u_recovery', 1);
    addL(sys, 'PMS_Demand_Policy', 4, 'P_recovery_actual_kW', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 5, 'start_hold_timer', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 6, 'stop_hold_timer', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 7, 'online_timer', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 8, 'audit_valid', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 9, 'audit_old_state', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 10, 'audit_new_state', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 11, 'audit_P_commit_kW', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 12, 'audit_t_start_hold', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 13, 'audit_t_stop_hold', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 14, 'audit_t_online', 1);
    addL(sys, 'PMS_Commitment_Supervisor', 15, 'audit_safety_override', 1);
    addL(sys, 'or_baseline', 1, 'selector_baseline_active', 1);
    addL(sys, 'baseline_fallback_active', 1, 'out_baseline_fallback_active', 1);
    addL(sys, 'baseline_fallback_reason', 1, 'out_baseline_fallback_reason', 1);

    set_param(sys, 'MaskDisplay', ['disp(sprintf(''07_PMS_v2\nPMS v2 (FR-449)\ncanonical=genset-side DD-1''))']);
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
