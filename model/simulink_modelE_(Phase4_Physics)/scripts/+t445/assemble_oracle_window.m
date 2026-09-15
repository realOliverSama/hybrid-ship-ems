function W = assemble_oracle_window(t_s, P_load_kW, varargin)
% t445.assemble_oracle_window — 10 s bin / shrinking-horizon / E300_true（DGN-F-02）
%
% t_s, P_load_kW: 1 Hz 列向量。窗 [t0, min(t0+300, T_end)]，禁零填充。
% 完整 bin：Pbar = 样本均值，E = (10/3600)*Pbar。
% partial 末 bin：按时长加权。E300_true = sum E_k，与 1 s 积分在 1 Hz 上等价。
%
% Name-value: T_end, refresh_s (10), horizon_s (300), E_ref_kWh, E_scale_kWh

p = inputParser;
p.addParameter('T_end', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('refresh_s', 10, @isnumeric);
p.addParameter('horizon_s', 300, @isnumeric);
p.addParameter('E_ref_kWh', 27.781415, @isnumeric);
p.addParameter('E_scale_kWh', 13.671476, @isnumeric);
p.parse(varargin{:});
opt = p.Results;

t_s = t_s(:);
P_load_kW = P_load_kW(:);
assert(numel(t_s) == numel(P_load_kW), 'T445:WINDOW:LEN', 't and P length mismatch');
assert(numel(t_s) >= 2, 'T445:WINDOW:SHORT', 'need at least 2 samples');
dt = median(diff(t_s));
assert(abs(dt - 1) < 1e-9, 'T445:WINDOW:DT', '1 Hz grid required, dt=%g', dt);

if isempty(opt.T_end)
    T_end = t_s(end) + dt;   % last sample covers [t_end, t_end+dt)
else
    T_end = opt.T_end;
end
refresh = opt.refresh_s;
horizon = opt.horizon_s;

t0 = (0:refresh:(T_end - refresh)).';
assert(~isempty(t0), 'T445:WINDOW:T0', 'T_end=%g too short for a refresh', T_end);

n = numel(t0);
E300 = zeros(n, 1);
ehat = zeros(n, 1);
K_rem = zeros(n, 1);
Pbar = cell(n, 1);
Ebin = cell(n, 1);
dtbin = cell(n, 1);
E1s = zeros(n, 1);
maxRel = 0;

for i = 1:n
    t_lo_w = t0(i);
    t_hi_w = min(t_lo_w + horizon, T_end);
    remaining = t_hi_w - t_lo_w;
    assert(remaining >= refresh - 1e-12, 'T445:WINDOW:REM', 'empty window at t0=%g', t_lo_w);

    n_full = floor((remaining + 1e-12) / refresh);
    rem = remaining - n_full * refresh;
    if rem < 1e-12
        rem = 0;
        n_bin = n_full;
    else
        n_bin = n_full + 1;
    end
    pb = zeros(n_bin, 1);
    eb = zeros(n_bin, 1);
    db = zeros(n_bin, 1);
    e1 = 0;
    for k = 1:n_bin
        t_lo = t_lo_w + refresh * (k - 1);
        if k <= n_full
            t_hi = t_lo_w + refresh * k;
            dur = refresh;
        else
            t_hi = t_hi_w;
            dur = rem;
        end
        mask = t_s >= t_lo - 1e-12 & t_s < t_hi - 1e-12;
        Pk = P_load_kW(mask);
        assert(~isempty(Pk), 'T445:WINDOW:EMPTYBIN', 'no samples in [%g,%g)', t_lo, t_hi);
        pb(k) = mean(Pk);
        db(k) = dur;
        eb(k) = (dur / 3600) * pb(k);
        e1 = e1 + sum(Pk) * dt / 3600;
    end
    Pbar{i} = pb;
    Ebin{i} = eb;
    dtbin{i} = db;
    K_rem(i) = n_bin;
    E300(i) = sum(eb);
    E1s(i) = e1;
    denom = max(abs(E1s(i)), 1e-12);
    rel = abs(E300(i) - E1s(i)) / denom;
    if rel > maxRel, maxRel = rel; end
    ehat(i) = (E300(i) - opt.E_ref_kWh) / opt.E_scale_kWh;
end

assert(maxRel < 1e-12, 'T445:WINDOW:EQUIV', ...
    'bin-mean vs 1s integral rel=%g exceeds 1e-12', maxRel);

W = struct();
W.t0 = t0;
W.T_end = T_end;
W.refresh_s = refresh;
W.horizon_s = horizon;
W.K_rem = K_rem;
W.Pbar = Pbar;
W.E_bin_kWh = Ebin;
W.dt_bin_s = dtbin;
W.E300_true_kWh = E300;
W.E300_1s_kWh = E1s;
W.ehat_B1 = ehat;
W.E_ref_kWh = opt.E_ref_kWh;
W.E_scale_kWh = opt.E_scale_kWh;
W.max_rel_1s_vs_bin = maxRel;
W.shrinking_horizon = true;
W.zero_fill = false;
end
