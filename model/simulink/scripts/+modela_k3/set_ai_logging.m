function set_ai_logging(mdl, mode)
% modela_k3.set_ai_logging — 按 EMS 模式启用/停用 AI 变体内部信号记录
% P_pred_max_kW / AI_valid 只存在于 EMS_AI_Enhanced 变体中。
% 使用 Simulink.match.allVariants 访问非活动变体；只操作输出端口。
    base = [mdl '/05_EMS_Controller/EMS_AI_Enhanced'];
    names = {'P_pred_max_kW', 'AI_valid'};
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
            if mode == 1
                set_param(p, 'DataLogging', 'on');
                set_param(p, 'DataLoggingName', pn);
            else
                set_param(p, 'DataLogging', 'off');
            end
        end
    end
end
