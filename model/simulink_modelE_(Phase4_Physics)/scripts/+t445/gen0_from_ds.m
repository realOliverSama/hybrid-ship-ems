function [gen, N0] = gen0_from_ds(ds, t0, mode)
% t445.gen0_from_ds — 从植物当前拍构造 receding-horizon 机组初态（E5）
if nargin < 3 || isempty(mode)
    mode = 'first';
end
t = ds.t(:);
if strcmp(mode, 'last')
    i0 = find(t <= t0 + 1e-9, 1, 'last');
else
    i0 = find(t >= t0 - 1e-9, 1, 'first');
end
if isempty(i0), i0 = 1; end
p1 = local_sig(ds, {'P_DG1_kW','P_gen1_kW'}, i0);
p2 = local_sig(ds, {'P_DG2_kW','P_gen2_kW'}, i0);
s1 = round(local_sig(ds, {'state_DG1'}, i0));
s2 = round(local_sig(ds, {'state_DG2'}, i0));
gen = struct('st', [s1, s2], 'P_ramp', [p1, p2], 'P_lag', [p1, p2], ...
    't_dwell', [0, 0]);
N0 = double(s1 == 3) + double(s2 == 3);
if N0 < 1
    N0 = 1;
end
end

function v = local_sig(ds, names, idx)
v = 0;
for i = 1:numel(names)
    if isfield(ds, names{i})
        x = ds.(names{i})(:);
        v = x(min(idx, numel(x)));
        return;
    end
end
end
