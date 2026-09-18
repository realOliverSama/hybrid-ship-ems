function out = write_sourcesel_v1(ds, out_dir, voyage_id, arm, run_id, binding)
% ppe.write_sourcesel_v1 — 运行时源选择遥测工件（EP3 §H，schema ppe_sourcesel_v1）。
%
% 目的：让"PMS 实际消费了哪一路参考值"成为**运行自带的一手证据**，而不是事后从
% plant 响应反推（2026-09-18 缺陷的可见性根因）。
%
% 通道（来自 model.set_source_selection_logging 开启的既有输出端口）：
%   P_gen_ref_kW / P_gen_req_baseline_kW / P_gen_req_eff_kW /
%   selector_baseline_active / P_gen_unserved_kW / P_gen_surplus_kW /
%   out_baseline_fallback_active / ems_health_ok
% 采样：1 Hz 右端点值 + 该秒内 min/max（离散分支信号 min≠max 即秒内发生过切换）。
%
% 与植物轨迹工件分离（独立 schema / 独立文件 <arm>.sourcesel.v1.mat），
% 因此不改变 ppe_plant_traj_v1 的通道契约（FC-3 checker 消费面不变）。
%
% FAIL CLOSED：任一遥测通道缺失即报错（缺失意味着 set_source_selection_logging
% 未生效或模型被改动 —— 属必须暴露的配置错误）。
%
% 输入：ds 日志结构（model.extract_logsout 输出）；out_dir/voyage_id/arm/run_id；
%       binding = ppe.arm_runtime_bind 记录（写入 sidecar，证明绑定值）。
% 输出：out.path / out.sidecar / out.n_samples / out.sha256

CH = {'P_gen_ref_kW', 'P_gen_req_eff_kW', 'selector_baseline_active', ...
    'P_load_gen_kW', 'P_gen_unserved_kW', 'P_gen_surplus_kW', ...
    'dispatch_min_clamp_active', 'baseline_fallback_active', 'ems_health_ok'};
% 注：基线参考 P_gen_req_baseline_kW 处于 Simulink Function 边界，R2026a 禁止直接
% 记录（见 model.set_source_selection_logging 文件头 ⑤）。其证据形式为：
% 基线模式下 P_gen_req_eff_kW 与独立计算的 LPF_{tau=60s}(P_load_gen_kW) 一致。

if ~isfield(ds, 't')
    error('PPE:SOURCESEL:FAILCLOSED', '缺少时间轴 t');
end
missing = CH(~cellfun(@(c) isfield(ds, c), CH));
if ~isempty(missing)
    error('PPE:SOURCESEL:FAILCLOSED', ...
        ['源选择遥测通道缺失：%s（set_source_selection_logging 未生效或' ...
         '被后续改动覆盖）'], strjoin(missing, ', '));
end

t_all = ds.t(:);
edges = unique(ceil(t_all - 1e-9));     % 1 Hz 右端点网格
n = numel(edges);
tel = struct();
tel.time_s = edges;
for k = 1:numel(CH)
    c = CH{k};
    v = local_col(ds.(c), numel(t_all));
    vend = zeros(n, 1); vmin = zeros(n, 1); vmax = zeros(n, 1);
    lo = 1;
    for i = 1:n
        hi = find(t_all <= edges(i) + 1e-9, 1, 'last');
        if isempty(hi), hi = lo; end
        seg = v(lo:hi);
        if isempty(seg), seg = v(min(lo, numel(v))); end
        vend(i) = seg(end); vmin(i) = min(seg); vmax(i) = max(seg);
        lo = hi + 1;
        if lo > numel(v), lo = numel(v); end
    end
    tel.([c '_end']) = vend;
    tel.([c '_min']) = vmin;
    tel.([c '_max']) = vmax;
end

if ~exist(out_dir, 'dir'), mkdir(out_dir); end
vdir = fullfile(out_dir, char(voyage_id));
if ~exist(vdir, 'dir'), mkdir(vdir); end
mpath = fullfile(vdir, sprintf('%s.sourcesel.v1.mat', char(arm)));
tmp = [mpath '.tmp'];
save(tmp, 'tel', '-v7.3');
movefile(tmp, mpath, 'f');

side = struct();
side.schema = 'ppe_sourcesel_sidecar_v1';
side.voyage_id = char(voyage_id);
side.arm = char(arm);
side.run_id = char(run_id);
side.n_samples = n;
side.channels = CH;
side.arm_binding = binding;
side.source_gate = ['selector_baseline_active == 0 断言 EMS 分支；' ...
    'P_gen_req_eff_kW 必须等于所选分支的参考值'];
side.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
spath = fullfile(vdir, sprintf('%s.sourcesel.v1.sidecar.json', char(arm)));
fid = fopen(spath, 'w');
fwrite(fid, jsonencode(side, 'PrettyPrint', true));
fclose(fid);

out = struct('path', mpath, 'sidecar', spath, 'n_samples', n, ...
    'sha256', model.sha256_file(mpath));
end

function v = local_col(x, n)
x = double(x(:));
if numel(x) == 1
    v = repmat(x, n, 1);
elseif numel(x) >= n
    v = x(1:n);
else
    v = [x; repmat(x(end), n - numel(x), 1)];
end
end
