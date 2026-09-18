function info = set_source_selection_logging(mdl, onoff)
% model.set_source_selection_logging — 运行时源选择遥测信号记录开关（内存内，不保存模型）。
%
% 依据（EP3 §H）：2026-09-18 缺陷之所以长期不可见，是因为"PMS 实际消费了哪一路
% 参考值"没有落盘证据，只能事后从 plant 响应反推。本函数把源选择链上的既有信号
% 纳入 logsout 记录（只设置 DataLogging 属性，不修改、不保存 .slx）。
%
% 实现要点（2026-09-18 本地 runtime 五轮实测收敛，全部为 harness plumbing）：
%   ① 产生侧端口若属 **Simulink Function**（PMS_Demand_Policy 编译为 Stateflow 函数）
%      则 sim 时被拒（"请记录 Simulink 函数内的信号"）；函数**内部**端口同样被拒；
%   ② Simulink **信号线不支持** DataLogging 参数；
%   ③ **输入端口**不接受 DataLogging；
%   ④ 因此本函数只在**普通块的输出端口**开启记录（下文映射逐条实测通过）。
%   ⑤ 基线参考 `P_gen_req_baseline_kW` 因 ① **无法直接记录**——其证据改为等价形式：
%      基线模式下 `P_gen_req_eff_kW`（选中参考）与独立计算的基线需求律
%      `LPF_{τ=60 s}(P_load_gen)` 一致，且与 EMS 参考显著不同（见 qual_ep3 Q-EP3-1）。
%      `P_load_gen_kW`（PMS 边界 bus→genset 域换算增益输出）可直接记录，作为该
%      独立计算的输入。
%
% 映射（信号 → 普通块输出端口）：
%   P_gen_ref_kW              → 05_EMS_Controller out:1（EMS 律参考值）
%   P_gen_req_eff_kW          → 07_PMS_v2/Source_Selector out:1（选中参考值）
%   selector_baseline_active  → 07_PMS_v2/or_baseline out:1（分支控制量）
%   P_load_gen_kW             → 07_PMS_v2/X_b2g_P_load out:1（genset 域负荷）
%   P_gen_unserved_kW         → PMS_Dispatch_Allocator out:4
%   P_gen_surplus_kW          → PMS_Dispatch_Allocator out:5
%   dispatch_min_clamp_active → PMS_Dispatch_Allocator out:6
%   baseline_fallback_active  → UD_safety_baseline_fallback_active out:1（S5 锁存）
%   ems_health_ok             → UD_safety_ems_health_ok out:1（健康通道）
%
% 输入：mdl；onoff logical（缺省 true）。
% 输出：info.logged / info.missing / info.restricted（函数边界受限名单）。

if nargin < 2 || isempty(onoff), onoff = true; end
M = { ...
    'P_gen_ref_kW',              '05_EMS_Controller',                        'out', 1; ...
    'P_gen_req_eff_kW',          '07_PMS_v2/Source_Selector',                'out', 1; ...
    'selector_baseline_active',  '07_PMS_v2/or_baseline',                    'out', 1; ...
    'P_load_gen_kW',             '07_PMS_v2/X_b2g_P_load',                   'out', 1; ...
    'P_gen_unserved_kW',         '07_PMS_v2/PMS_Dispatch_Allocator',         'out', 4; ...
    'P_gen_surplus_kW',          '07_PMS_v2/PMS_Dispatch_Allocator',         'out', 5; ...
    'dispatch_min_clamp_active', '07_PMS_v2/PMS_Dispatch_Allocator',         'out', 6; ...
    'baseline_fallback_active',  'UD_safety_baseline_fallback_active',       'out', 1; ...
    'ems_health_ok',             'UD_safety_ems_health_ok',                  'out', 1};

hit = {}; miss = {};
for k = 1:size(M, 1)
    nm = M{k, 1};
    blk = [mdl '/' M{k, 2}];
    idx = M{k, 4};
    ok = false;
    try
        h = get_param(blk, 'Handle');
        ph = get_param(h, 'PortHandles');
        p = ph.Outport(idx);
        set_param(p, 'DataLogging', onoff);
        if onoff
            set_param(p, 'DataLoggingNameMode', 'Custom');
            set_param(p, 'DataLoggingName', nm);
        end
        ok = true;
    catch
        ok = false;
    end
    if ok
        hit{end + 1} = nm; %#ok<AGROW>
    else
        miss{end + 1} = nm; %#ok<AGROW>
    end
end

info = struct();
info.logged = unique(hit);
info.missing = unique(miss);
info.restricted = {'P_gen_req_baseline_kW'};   % Simulink Function 输出，见文件头 ⑤
info.modified_on_disk = false;
end
