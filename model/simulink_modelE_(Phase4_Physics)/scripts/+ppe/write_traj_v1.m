function out = write_traj_v1(ds, out_dir, voyage_id, arm, run_id)
% ppe.write_traj_v1 — epoch-2 1 Hz 正式轨迹工件（amendment FC-5 / design D-7）。
%
% 从 extract_logsout 的 ds 抽取 25 通道（time + 24 信号）1 Hz 右端点采样，
% 原子写 MAT v7.3（tmp → movefile），并写 sidecar JSON（schema/sha256/行数/
% t 范围/通道清单）。返回 out：struct('path',…,'sidecar',…,'sha256',…,'n',…)。
% 任何写入失败都抛错（臂失败 → 原子重跑；不产生半成品轨迹记录）。
%
% 通道清单与已批准设计 FC-5 §5.1 逐字一致；safety_hard_count 由模型记录的
% safety_s1a/s1b/s2 三判定按 (>0.5) 求和派生（与 checker v2 同口径），
% 任一缺失 → FAIL CLOSED。
% 工件布局（design §5.2）：<formal_dir>/<voyage_id>/<arm>.traj.v1.mat。

CH = {'P_load_total_kW','P_prop_demand_kW','P_prop_actual_kW','P_unserved_kW', ...
      'P_gen_unserved_kW','P_gen_actual_kW','P_DG1_kW','P_DG2_kW','state_DG1','state_DG2', ...
      'commitment_state','N_dispatch','P_bat_bus_kW','P_bat_actual_kW','SOC','V_bat_V','I_bat_A', ...
      'P_bat_dis_avail_kW','P_bat_chg_avail_kW','V_dc_V','derate08_active','delta_lambda', ...
      'forecast_source_id'};
CH_DERIVED = 'safety_hard_count';
SAFE = {'safety_s1a','safety_s1b','safety_s2'};

missing = CH(~cellfun(@(c) isfield(ds, c), CH));
if ~isfield(ds, 't')
    error('PPE:TRAJ:FAILCLOSED', '必需信号缺失：t（时间轴）');
end
if ~isempty(missing)
    error('PPE:TRAJ:FAILCLOSED', '轨迹必需通道缺失：%s', strjoin(missing, ','));
end
safem = SAFE(~cellfun(@(c) isfield(ds, c), SAFE));
if ~isempty(safem)
    error('PPE:TRAJ:FAILCLOSED', '轨迹必需通道缺失：%s（safety_hard_count 派生源）', ...
        strjoin(safem, ','));
end

% 1 Hz 右端点采样：t 网格 = ceil(ds.t)（去重保末值）
t_all = ds.t(:);
edges = unique(ceil(t_all - 1e-9));
n = numel(edges);
idx = zeros(n, 1);
cur = 1;
for k = 1:n
    while cur < numel(t_all) && t_all(cur) < edges(k) - 1e-9
        cur = cur + 1;
    end
    idx(k) = min(cur, numel(t_all));
end
traj = struct();
traj.time = edges(:);
traj = orderfields(traj, {'time'});
sig_names = CH(2:end);
for c = 1:numel(sig_names)
    sig = ds.(sig_names{c});
    if numel(sig) == n
        traj.(CH{c}) = sig(:);
    elseif numel(sig) == 1
        traj.(CH{c}) = repmat(sig, n, 1);
    else
        traj.(CH{c}) = sig(idx);
    end
end
% safety_hard_count（派生通道，设计 FC-5 §5.1）
shc = (ds.safety_s1a(:) > 0.5) + (ds.safety_s1b(:) > 0.5) + (ds.safety_s2(:) > 0.5);
if numel(shc) == n
    traj.(CH_DERIVED) = double(shc);
elseif numel(shc) == 1
    traj.(CH_DERIVED) = repmat(double(shc), n, 1);
else
    traj.(CH_DERIVED) = double(shc(idx));
end

% 原子写：tmp → movefile；命名 <voyage>/<arm>.traj.v1.mat（design §5.2）
if ~exist(out_dir, 'dir'), mkdir(out_dir); end
mat = fullfile(out_dir, sprintf('%s.traj.v1.mat', char(arm)));
tmp = [mat '.writing'];   % char 拼接用方括号（+ 对 char 是数值加法）
save(tmp, '-v7.3', 'traj');
movefile(tmp, mat, 'f');

% sidecar
sha = model.sha256_file(mat);
all_ch = [{'time'}, CH, {CH_DERIVED}];
side = struct('schema', 'ppe_traj_v1', 'voyage_id', char(voyage_id), 'arm', char(arm), ...
    'run_id', char(run_id), 'channels', {all_ch}, 'n_samples', n, ...
    't_end', traj.time(end), 'sha256', sha, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z')));
fid = fopen(fullfile(out_dir, sprintf('%s.traj.v1.sidecar.json', char(arm))), 'w');
fwrite(fid, jsonencode(side, 'PrettyPrint', true));
fclose(fid);

out = struct('path', mat, 'sha256', sha, 'n_samples', n, 'sidecar', side);
end
