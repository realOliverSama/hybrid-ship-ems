function set_ai_logging(mdl, mode)
% model.set_ai_logging — 按 EMS 模式启用/停用 AI 变体内部信号记录
% P_pred_max_kW / t_peak_pred_s / AI_valid 只存在于 EMS_AI_Enhanced 变体中；
% EMS_ScheduledFF 变体还有 P_ff_kW / t_fire_latched（T-004）；
% EMS_MinFF 变体有 P_ff_raw_kW / P_ff_applied_kW / R_max_kW / MSFF_pred_valid（T-004）。
% 使用 Simulink.match.allVariants 访问非活动变体；只操作输出端口。
% mode: 0=Baseline 1=AI_Enhanced 2=ScheduledFF 3=EMS_MinFF
    aiNames = {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid'};
    sffNames = [aiNames, {'P_ff_kW', 't_fire_latched'}];
    mffNames = {'P_ff_raw_kW', 'P_ff_applied_kW', 'R_max_kW', 'MSFF_pred_valid'};
    if mode == 0
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], aiNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], sffNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_MinFF'], mffNames, false);
    elseif mode == 1
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], aiNames, true);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], sffNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_MinFF'], mffNames, false);
    elseif mode == 2
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], aiNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], sffNames, true);
        applyLogging([mdl '/05_EMS_Controller/EMS_MinFF'], mffNames, false);
    else
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], aiNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], sffNames, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_MinFF'], mffNames, true);
    end
end

function applyLogging(base, names, onoff)
    try
        allports = find_system(base, 'MatchFilter', @Simulink.match.allVariants, ...
            'LookUnderMasks', 'all', 'FollowLinks', 'on', 'FindAll', 'on', 'Type', 'port');
    catch
        allports = find_system(base, 'LookUnderMasks', 'all', 'FollowLinks', 'on', ...
            'FindAll', 'on', 'Type', 'port');
    end
    for k = 1:numel(allports)
        p = allports(k);
        try
            pn = get_param(p, 'Name');
            pt = get_param(p, 'PortType');
        catch
            continue;
        end
        if any(strcmp(pn, names)) && strcmp(pt, 'outport')
            if onoff
                set_param(p, 'DataLogging', 'on');
                set_param(p, 'DataLoggingName', pn);
            else
                set_param(p, 'DataLogging', 'off');
            end
        end
    end
end
