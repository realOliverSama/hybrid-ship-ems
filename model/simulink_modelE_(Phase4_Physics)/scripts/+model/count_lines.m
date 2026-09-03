function n = count_lines(path)
% model.count_lines — 统计 CSV 数据行数（不含列名行）
    fid = fopen(path, 'r');
    n = 0;
    while ~feof(fid)
        fgetl(fid);
        n = n + 1;
    end
    fclose(fid);
    n = max(n - 1, 0);
end
