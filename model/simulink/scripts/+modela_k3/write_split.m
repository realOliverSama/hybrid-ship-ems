function write_split(path, files)
% modela_k3.write_split — 将一组航次合并写出 CSV，并附加未来 60s 负荷峰值列
    nRun = numel(files);
    if nRun == 0, error('无匹配航次数据'); end
    first = load(files{1}, 'ds1');
    names = fieldnames(first.ds1);
    cols = [names; {'P_load_future_max_60s'}];
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s\n', strjoin(cols, ','));
    fmtRow = [repmat('%.6g,', 1, numel(cols) - 1) '%.6g\n'];
    for i = 1:nRun
        S = load(files{i}, 'ds1');
        ds1 = S.ds1;
        n = numel(ds1.t);
        fwin = 60;
        fut = nan(n, 1);
        for k = 1:n
            j2 = k + fwin;
            if j2 <= n
                fut(k) = max(ds1.P_load_total_kW(k:j2));
            end
        end
        for r = 1:n
            row = zeros(1, numel(cols));
            for c = 1:numel(names)
                v = ds1.(names{c});
                if numel(v) >= r, row(c) = v(r); end
            end
            row(end) = fut(r);
            fprintf(fid, fmtRow, row);
        end
    end
    fclose(fid);
    fprintf('[05] %s: %d 行\n', path, modela_k3.count_lines(path));
end
