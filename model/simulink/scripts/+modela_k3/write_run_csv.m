function write_run_csv(path, ds1)
% modela_k3.write_run_csv — 将 1s 重采样数据集写为 CSV（首行为列名）
    names = fieldnames(ds1);
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fprintf(fid, '%s\n', strjoin(names, ','));
    n = numel(ds1.t);
    fmt = repmat('%.6g,', 1, numel(names) - 1);
    fmt = [fmt '%.6g\n'];
    for i = 1:n
        row = zeros(1, numel(names));
        for k = 1:numel(names)
            v = ds1.(names{k});
            if numel(v) >= i, row(k) = v(i); end
        end
        fprintf(fid, fmt, row);
    end
    fclose(fid);
end
