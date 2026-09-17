function out = write_traj_v1(ds, out_dir, voyage_id, arm, run_id)
% ppe.write_traj_v1 — epoch-2 1 Hz 正式轨迹工件（amendment FC-5 / design D-7）。
%
% 从 extract_logsout 的 ds 抽取 25 通道 1 Hz 右端点采样，原子写 MAT v7.3
% （tmp → movefile），并写 sidecar JSON（schema/sha256/行数/t 范围/通道清单）。
% 返回 out：struct('path',…,'sidecar',…,'sha256',…,'n',…)。
% 任何写入失败都抛错（臂失败 → 原子重跑；不产生半成品轨迹记录）。

CH = {'P_load_total_kW','P_prop_demand_kW','P_prop_actual_kW','P_unserved_kW', ...
      'P_gen_unserved_kW','P_gen_actual_kW','P_DG1_kW','P_DG2_kW','state_DG1','state_DG2', ...
      'commitment_state','N_dispatch','P_bat_bus_kW','P_bat_actual_kW','SOC','V_bat_V','I_bat_A', ...
      'P_bat_dis_avail_kW','P_bat_chg_avail_kW','V_dc_V','derate08_active','delta_lambda', ...
      'forecast_source_id','P_prop_limit_kW'};

missing = CH(~cellfun(@(c) isfield(ds, c), CH));
if ~isfield(ds, 't')
    error('PPE:TRAJ:FAILCLOSED', '必需信号缺失：t（时间轴）');
end
if ~isempty(missing)
    error('PPE:TRAJ:FAILCLOSED', '轨迹必需通道缺失：%s', strjoin(missing, ','));
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

% 原子写：tmp → movefile
if ~exist(out_dir, 'dir'), mkdir(out_dir); end
mat = fullfile(out_dir, sprintf('%s_%s.traj.v1.mat', voyage_id, arm));
tmp = [mat '.writing'];   % char 拼接用方括号（+ 对 char 是数值加法）
save(tmp, '-v7.3', 'traj');
movefile(tmp, mat, 'f');

% sidecar
sha = model.sha256_file(mat);
side = struct('schema', 'ppe_traj_v1', 'voyage_id', char(voyage_id), 'arm', char(arm), ...
    'run_id', char(run_id), 'channels', {CH}, 'n_samples', n, ...
    't_end', traj.time(end), 'sha256', sha, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z')));
fid = fopen([mat(1:end-4) '.sidecar.json'], 'w');
fwrite(fid, jsonencode(side, 'PrettyPrint', true));
fclose(fid);

out = struct('path', mat, 'sha256', sha, 'n_samples', n, 'sidecar', side);
end
