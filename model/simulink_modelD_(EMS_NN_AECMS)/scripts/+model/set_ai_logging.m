function set_ai_logging(mdl, mode)
% model.set_ai_logging — 按 EMS 模式启用/停用 AI 变体内部信号记录
% P_pred_max_kW / t_peak_pred_s / AI_valid 只存在于 EMS_AI_Enhanced 变体中；
% EMS_ScheduledFF 变体还有 P_ff_kW / t_fire_latched（T-004）。
% 使用 Simulink.match.allVariants 访问非活动变体；只操作输出端口。
% mode: 0=Baseline(无 AI 记录) 1=AI_Enhanced 2=ScheduledFF
    if mode == 0
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid'}, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid', 'P_ff_kW', 't_fire_latched'}, false);
    elseif mode == 1
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid'}, true);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid', 'P_ff_kW', 't_fire_latched'}, false);
    else
        applyLogging([mdl '/05_EMS_Controller/EMS_AI_Enhanced'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid'}, false);
        applyLogging([mdl '/05_EMS_Controller/EMS_ScheduledFF'], {'P_pred_max_kW', 't_peak_pred_s', 'AI_valid', 'P_ff_kW', 't_fire_latched'}, true);
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
