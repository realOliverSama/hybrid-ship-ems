function write_result(path, R)
% gate_ctrl.write_result — gate_control_result.json 序列化（jsonencode PrettyPrint）
    fid = fopen(path, 'w', 'n', 'UTF-8');
    fwrite(fid, jsonencode(R, 'PrettyPrint', true));
    fclose(fid);
    fprintf('[43] JSON 已写 %s\n', path);
end