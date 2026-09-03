function files = pick_runs(rawDir, scenIds)
% model.pick_runs — 按工况编号挑选 datasets/raw 下的航次文件
    d = dir(fullfile(rawDir, 'run_*.mat'));
    files = {};
    for i = 1:numel(d)
        S = load(fullfile(rawDir, d(i).name), 'SCN');
        if any(S.SCN.id == scenIds)
            files{end+1} = fullfile(rawDir, d(i).name); %#ok<AGROW>
        end
    end
    if ~isempty(files)
        [~, ord] = sort(files);
        files = files(ord);
    end
end
