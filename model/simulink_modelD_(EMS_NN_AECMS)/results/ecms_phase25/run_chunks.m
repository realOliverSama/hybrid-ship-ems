% run_chunks.m — T-302 分块驱动 v2（PCT 2 workers / 25 输入块，防爆半径）
% 临时监控脚本（不入库）：断点续跑依赖 61 号跳过已有 voyage_*.mat
% v2 变更：本机并行 worker 出现过两次崩溃（一次致挂死），收缩 workers 4->2、块 50->25
c = parcluster('Processes');
c.NumWorkers = 2;
saveProfile(c);
fprintf('===== parcluster NumWorkers=%d =====\n', c.NumWorkers);
CHUNK = 25;
for s0 = 240:CHUNK:400
    n = min(CHUNK, 401 - s0);
    FLEET_START = s0;
    FLEET_N = n;
    fprintf('===== CHUNK start=%d n=%d =====\n', s0, n);
    eval(fileread(fullfile(pwd, 'scripts', '61_generate_fleet_profiles_v2.m')));
end
fprintf('===== ALL CHUNKS DONE =====\n');
