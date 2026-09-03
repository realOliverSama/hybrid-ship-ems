function export_structure_fingerprint(mdl, outPath)
% model.export_structure_fingerprint — 结构指纹导出（find_system → JSON，NFR-405/T-010）
%   用于 builder 幂等验证（连建两次指纹逐位一致）与 Gate G1 结构检查。
    load_system(mdl);
    [~, mdlName] = fileparts(mdl);      % 兼容传入 .slx 路径或模型名
    blocks = find_system(mdlName, 'LookUnderMasks', 'all', 'FollowLinks', 'on', ...
        'MatchFilter', @Simulink.match.allVariants, 'Type', 'Block');
    lines = find_system(mdlName, 'FindAll', 'on', 'LookUnderMasks', 'all', 'FollowLinks', 'on', ...
        'MatchFilter', @Simulink.match.allVariants, 'Type', 'line');
    blks = struct(); blks.count = numel(blocks); blks.items = cell(1, numel(blocks));
    for k = 1:numel(blocks)
        bp = blocks{k};
        rt = sfroot;
        ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', bp);
        info = struct('path', bp, 'type', get_param(bp, 'BlockType'));
        if ~isempty(get_param(bp, 'Name'))
            info.name = get_param(bp, 'Name');
        end
        if ~isempty(ch)
            info.mf_signature = ''; %#ok<NASGU> % 采样时间指纹（状态机离散化证据）
            try, info.sample_time = ch.SampleTime; catch, info.sample_time = '<n/a>'; end
            try, info.chart_update = ch.ChartUpdate; catch, info.chart_update = '<n/a>'; end
        end
        blks.items{k} = info;
    end
    fp = struct('schema', 'modelE_structure_fingerprint_v1', ...
        'model', mdlName, ...
        'generated_at', char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ')), ...
        'n_blocks', numel(blocks), 'n_lines', numel(lines), ...
        'blocks', {blks.items});
    fid = fopen(outPath, 'w');
    fwrite(fid, jsonencode(fp, 'PrettyPrint', true));
    fclose(fid);
    fprintf('[fingerprint] %s（%d blocks / %d lines）\n', outPath, numel(blocks), numel(lines));
end