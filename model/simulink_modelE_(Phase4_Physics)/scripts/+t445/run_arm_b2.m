function [ds, meta] = run_arm_b2(SCN, stream, varargin)
% t445.run_arm_b2 — B2 分段 SimState 交替（DGN-F-10）。不落盘 .slx。
% stream: t (1 Hz), P_load_kW, voyage_id
p = inputParser;
p.addParameter('StopTime', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('cfg', [], @(x) isempty(x) || isstruct(x));
p.addParameter('tbl', [], @(x) isempty(x) || isstruct(x));
p.addParameter('StartT0', 0, @isnumeric);
p.addParameter('InitialOp', [], @(x) true);
p.addParameter('InitialSOC', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('CheckpointDir', '', @ischar);
p.addParameter('Seed', 'phase4f-t445-toy-smoke-v1', @ischar);
p.parse(varargin{:});
opt = p.Results;
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if isempty(opt.cfg)
    cfg = phase4f_t445_config();
else
    cfg = opt.cfg;
end
if ~evalin('base', "exist('ECMS_Lambda', 'var')")
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end
if isempty(opt.tbl)
    tbl = t445.policy_table_cached(cfg);
else
    tbl = opt.tbl;
    tbl.oob_query_count = 0;
end

MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
stopT = opt.StopTime;
if isempty(stopT)
    stopT = 60;
end
t445.inject_scenario(MDL, SCN, 6, stopT);
KS = cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den;
assignin('base', 'ECMS_K_S', KS);

t_s = stream.t(:);
P = stream.P_load_kW(:);
W = t445.assemble_oracle_window(t_s, P, 'T_end', stopT, ...
    'E_ref_kWh', cfg.b1_norm.E_ref_kWh, 'E_scale_kWh', cfg.b1_norm.E_scale_kWh);
stream_sha = t445.sha256_doubles(P);
vid = 'TOY';
if isfield(stream, 'voyage_id'), vid = stream.voyage_id; end

dt = cfg.oracle_window.refresh_s;
t0s = (opt.StartT0:dt:(stopT - dt)).';
assert(~isempty(t0s), 'T445:B2:T0');

if isempty(opt.InitialSOC)
    SOC = SCN.initial_SOC;
else
    SOC = opt.InitialSOC;
end
op = opt.InitialOp;
oob = 0;
fallback_n = 0;
dlam1 = zeros(size(t0s));
info_windows = zeros(numel(t0s), 2);
ds = [];
n_retry = 0;
N = 1;
gen = [];

for i = 1:numel(t0s)
    t0 = t0s(i);
    t_stop = t0 + dt;
    iw = find(abs(W.t0 - t0) < 1e-9, 1, 'first');
    assert(~isempty(iw), 'T445:B2:WIN', 'no window at t0=%g', t0);
    Pbar = W.Pbar{iw};
    sol = t445.b2_window_dp(Pbar, SOC, cfg, tbl, 't0', t0, ...
        'horizon_s', numel(Pbar) * dt, 'SOC_target', SCN.initial_SOC, ...
        'N0', N, 'gen0', gen);
    oob = oob + sol.oob_query_count;
    fallback_n = fallback_n + sol.infeasible_fallback_n;
    d1 = sol.dlambda(1);
    dlam1(i) = d1;
    info_windows(i, :) = sol.info_window;
    ehat = t445.prep_ehat_zoh(t0, W.E300_true_kWh(iw), d1);
    assignin('base', 'ECMS_EHat_Data', ehat);
    id = t445.simstate_mgr('identity', vid, 'B2', cfg, stream_sha, t0, opt.Seed);

    in = Simulink.SimulationInput(MDL);
    in = t445.make_simin_zoh(in);
    in = in.setModelParameter('StopTime', sprintf('%.4f', t_stop));
    in = in.setModelParameter('SaveFinalState', 'on');
    in = in.setModelParameter('SaveOperatingPoint', 'on');
    in = in.setModelParameter('FinalStateName', 'xFinal');
    if ~isempty(op)
        in = in.setInitialState(op);
    end
    try
        out = sim(in);
    catch ME
        t445.incident_log(struct('kind','MECHANICAL','task','T-009', ...
            'voyage_id', vid, 'segment_t0', t0, 'message', ME.message, ...
            'identity', id));
        n_retry = n_retry + 1;
        out = sim(in);
    end
    if ~isprop(out, 'xFinal') && ~isfield(out, 'xFinal')
        t445.incident_log(struct('kind','SIMSTATE','task','T-009', ...
            'message', 'xFinal missing', 'segment_t0', t0));
        error('T445:B2:SIMSTATE', 'SaveOperatingPoint did not return xFinal');
    end
    op = out.xFinal;
    dseg = model.extract_logsout(out);
    SOC = dseg.SOC(end);
    [gen, N] = t445.gen0_from_ds(dseg, t_stop, 'last');
    ds = local_append(ds, dseg, t0);
    if ~isempty(opt.CheckpointDir)
        ck = fullfile(opt.CheckpointDir, sprintf('ckpt_%s_t%.0f.mat', vid, t_stop));
        t445.simstate_mgr('save', ck, op, id, struct('SOC', SOC, 't_stop', t_stop));
    end
end

meta = struct();
meta.schema = 't445_b2_run_meta_v1';
meta.arm = 'B2';
meta.voyage_id = vid;
meta.config_sha256 = cfg.config_sha256;
meta.stream_sha256 = stream_sha;
meta.model_checksum = t445.simstate_mgr('checksum').combined;
meta.oob_query_count = oob;
meta.infeasible_fallback_n = fallback_n;
meta.dlambda_1 = dlam1;
meta.info_windows = info_windows;
meta.n_segments = numel(t0s);
meta.n_retry = n_retry;
meta.KS = KS;
meta.slx_saved = false;
end

function ds = local_append(ds, dseg, t0)
if isempty(ds)
    ds = dseg;
    return;
end
fn = fieldnames(dseg);
mask = dseg.t > t0 + 1e-12;
for i = 1:numel(fn)
    nm = fn{i};
    v = dseg.(nm);
    if isnumeric(v) && numel(v) == numel(dseg.t)
        ds.(nm) = [ds.(nm); v(mask)];
    end
end
end
