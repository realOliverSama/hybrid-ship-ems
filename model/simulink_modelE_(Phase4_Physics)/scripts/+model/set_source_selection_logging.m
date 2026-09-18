function info = set_source_selection_logging(mdl, onoff)
% model.set_source_selection_logging — 运行时源选择遥测信号记录开关（内存内，不保存模型）。
%
% 依据（EP3 §H）：2026-09-18 缺陷之所以长期不可见，是因为"PMS 实际消费了哪一路
% 参考值"没有任何落盘证据，只能事后从 plant 响应反推。本函数把源选择链上的**既有
% 模型输出端口**纳入 logsout 记录（仅设置 DataLogging 属性，不修改、不保存 .slx），
% 使每次运行都自带"哪一路参考值被选中"的权威证据。
%
% 记录端口（全部为冻结模型中的**真实 outport 名**，逐条可在 slx 中核对）：
%   P_gen_ref_kW                05_EMS_Controller 输出（= EMS 律参考值）
%   P_gen_req_baseline_kW       PMS_Demand_Policy 基线需求律输出
%   P_gen_req_eff_kW            Source_Selector 输出（= 实际选中并送 dispatch 的请求）
%   selector_baseline_active    源选择器分支（1=baseline 分支被选中）
%   P_gen_unserved_kW           generation-command mismatch（PMS 诊断）
%   P_gen_surplus_kW            surplus 诊断
%   out_baseline_fallback_active Safety S5 fallback 锁存
%   ems_health_ok               Safety 健康通道判定
%
% 输入：mdl 模型名；onoff logical（缺省 true）。
% 输出：info.logged = 实际开启的端口名 cell；info.missing = 未找到的端口名。
%
% 纪律：只操作输出端口；使用 Simulink.match.allVariants 以覆盖非活动变体；
% 不调用 save_system（.slx 字节不变，冻结身份不受影响）。

if nargin < 2 || isempty(onoff), onoff = true; end
names = {'P_gen_ref_kW', 'P_gen_req_baseline_kW', 'P_gen_req_eff_kW', ...
    'selector_baseline_active', 'P_gen_unserved_kW', 'P_gen_surplus_kW', ...
    'out_baseline_fallback_active', 'ems_health_ok'};

% 扫描范围：EMS 控制器 + PMS + 安全联锁（源选择链的三个持有者）
roots = {[mdl '/05_EMS_Controller'], [mdl '/07_PMS_v2'], [mdl '/16_Safety_Interlock']};
allports = [];
for r = 1:numel(roots)
    try
        pp = find_system(roots{r}, 'MatchFilter', @Simulink.match.allVariants, ...
            'LookUnderMasks', 'all', 'FollowLinks', 'on', 'FindAll', 'on', ...
            'Type', 'port');
    catch
        pp = find_system(roots{r}, 'LookUnderMasks', 'all', 'FollowLinks', 'on', ...
            'FindAll', 'on', 'Type', 'port');
    end
    allports = [allports; pp(:)]; %#ok<AGROW>
end

hit = {};
for k = 1:numel(allports)
    p = allports(k);
    try
        pn = get_param(p, 'Name');
        pt = get_param(p, 'PortType');
    catch
        continue;
    end
    if ~strcmp(pt, 'outport') || ~any(strcmp(pn, names))
        continue;
    end
    if onoff
        set_param(p, 'DataLogging', 'on');
        set_param(p, 'DataLoggingName', pn);
        set_param(p, 'DataLoggingNameMode', 'Custom');
    else
        set_param(p, 'DataLogging', 'off');
    end
    hit{end + 1} = pn; %#ok<AGROW>
end

info = struct();
info.logged = unique(hit);
info.missing = setdiff(names, info.logged);
info.modified_on_disk = false;   % set_param 仅内存生效；调用方负责断言未 save_system
end
