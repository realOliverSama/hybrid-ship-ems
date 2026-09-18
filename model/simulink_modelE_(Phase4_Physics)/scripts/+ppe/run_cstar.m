function [ds, meta] = run_cstar(SCN, winfun, arm, T_eval, ctxid, varargin)
% ppe.run_cstar — PPE C* segmented SimState runner (DGN-PPE-07/D8).
%
% One architecture for C0/C1/C2; ONLY the forecast source differs (FR-825).
% The runner never branches on source identity (FR-824): the window provider
% winfun encapsulates source policy (Option-B warm-up, anchor legality).
%
%   [Pbar, winprov, use_previous] = winfun(t0, ds, p0)
%       t0: current update time; ds: this arm's accumulated realized logs
%       ([] before the first segment); p0: causal probe of P_load_total(0)
%       (scalar; [] once any history exists)
%       Pbar: Kx1 bus-side kW bins (K<=30; shrinking horizon; no zero-fill)
%       winprov: struct provenance (source_id, store/model hashes, ...)
%       use_previous: true -> hold last Delta-lambda (ZOH after T_fcst)
%
% ctxid: checkpoint identity context (all D8 fields except
%        segment_t0/segment_index which the runner fills per segment).
%
% Name-value: StartT0 (resume start), EndT0 (early stop, identity still
%   T_eval), InitialOp, InitialSOC, InitialGen, InitialN, InitialDLam,
%   CheckpointDir, Seed, P0Load (resume: the causal probe value from the
%   original run, so the window provider sees identical semantics).
%
% No .slx modification. Deterministic: same identities -> same outputs.
p = inputParser;
p.addParameter('StartT0', 0, @isnumeric);
p.addParameter('EndT0', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('InitialOp', [], @(x) true);
p.addParameter('InitialSOC', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('InitialGen', [], @(x) true);
p.addParameter('InitialN', 1, @isnumeric);
p.addParameter('InitialDLam', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('CheckpointDir', '', @ischar);
p.addParameter('Seed', 'phase4f-ppe-toy-v1', @ischar);
p.addParameter('P0Load', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('SkipInject', false, @islogical);   % caller pre-injected voyage
% EP3 执行路径修复（附加、语义惰性）：给出臂绑定记录时，每段开头复核控制关键绑定，
% 任何"后来者"改写（PMS_SOURCE_MODE / EMS_CTRL_LAW / ...）立即 fail-closed。
% 选项缺省为空 ⇒ epoch-1/2 调用语义逐位不变。
p.addParameter('BindingGuard', [], @(x) isempty(x) || isstruct(x));
p.parse(varargin{:});
opt = p.Results;

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
pc = ppe.ppe_config();
cfg = pc.t445;
if ~evalin('base', "exist('ECMS_Lambda', 'var')")
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end
tbl = t445.policy_table_cached(cfg);

MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
if ~opt.SkipInject
    t445.inject_scenario(MDL, SCN, 6, T_eval);
end
assignin('base', 'ECMS_K_S', cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den);

dt = 10;
nseg = ceil(T_eval / dt - 1e-12);
t0s = (0:dt:(nseg - 1) * dt).';
if ~isempty(opt.EndT0)
    t0s = t0s(t0s < opt.EndT0 - 1e-9);
end
t0s = t0s(t0s >= opt.StartT0 - 1e-9);
assert(~isempty(t0s), 'PPE:RUN:T0', 'empty update grid');

if isempty(opt.InitialSOC)
    SOC = SCN.initial_SOC;
else
    SOC = opt.InitialSOC;
end
op = opt.InitialOp;
gen = opt.InitialGen;
N = opt.InitialN;
last_dlam = opt.InitialDLam;

% causal t=0 load probe (FR-821 t0<10 s rule): run a 1 s probe from the
% ORIGINAL initial operating point with dlambda=0, discard it, and read the
% algebraic t=0 plant load. The probe cannot affect the formal trajectory
% (the loop re-starts from the untouched IC/op). On resume the probe value
% is supplied by the caller (identical semantics).
p0 = opt.P0Load;
if isempty(p0) && isempty(op)
    in0 = Simulink.SimulationInput(MDL);
    in0 = t445.make_simin_zoh(in0);
    in0 = in0.setModelParameter('StopTime', '1');
    assignin('base', 'ECMS_EHat_Data', [0, 0, 0]);
    out0 = sim(in0);
    d0 = model.extract_logsout(out0);
    p0 = d0.P_load_total_kW(1);
end

dlam_seq = zeros(numel(t0s), 1);
win_sha = cell(numel(t0s), 1);
oob = 0;
fallback_n = 0;
ds = [];
warmup_state = [];

for i = 1:numel(t0s)
    t0 = t0s(i);
    if ~isempty(opt.BindingGuard)
        ppe.assert_control_bindings(opt.BindingGuard);   % EP3：段级绑定复核（工作区）
    end
    t_stop = min(t0 + dt, T_eval);
    [Pbar, winprov, use_prev] = winfun(t0, ds, p0);
    if use_prev
        assert(~isempty(last_dlam), 'PPE:RUN:HOLD0', ...
            'use_previous at t0=%g with no prior dlambda', t0);
        d1 = last_dlam;
    else
        Pbar = Pbar(:);
        assert(numel(Pbar) >= 1 && numel(Pbar) <= 30, 'PPE:RUN:K');
        assert(all(isfinite(Pbar)), 'PPE:RUN:FIN');
        sol = t445.b2_window_dp(Pbar, SOC, cfg, tbl, 't0', t0, ...
            'horizon_s', numel(Pbar) * dt, 'SOC_target', 0.60, ...
            'N0', N, 'gen0', gen, 'soc_step', 0.005);
        oob = oob + sol.oob_query_count;
        fallback_n = fallback_n + sol.infeasible_fallback_n;
        d1 = sol.dlambda(1);
        win_sha{i} = t445.sha256_doubles([Pbar; d1]);
    end
    E300_est = 0;
    if ~use_prev
        E300_est = sum(Pbar) * dt / 3600;   % informational channel (col 2)
    end
    assignin('base', 'ECMS_EHat_Data', t445.prep_ehat_zoh(t0, E300_est, d1));

    idctx = ctxid;
    idctx.arm = arm;
    idctx.segment_t0 = t0;
    idctx.segment_index = i - 1 + round(opt.StartT0 / dt);
    idctx.forecast_window_sha256 = win_sha{i};
    if isempty(idctx.forecast_window_sha256)
        idctx.forecast_window_sha256 = 'ZOH_HOLD_PREVIOUS';
    end
    id = ppe.checkpoint_mgr('identity', idctx);

    in = Simulink.SimulationInput(MDL);
    if ~isempty(opt.BindingGuard)
        % EP3：控制关键变量随每段 SimulationInput 显式携带（不依赖 base 工作区
        % 在连续 sim 之间的可见性/编译缓存；本地 runtime 实测确认该风险真实存在）
        bf = fieldnames(opt.BindingGuard.bindings);
        for bi = 1:numel(bf)
            in = in.setVariable(bf{bi}, opt.BindingGuard.bindings.(bf{bi}));
        end
    end
    in = t445.make_simin_zoh(in);
    in = in.setModelParameter('StopTime', sprintf('%.4f', t_stop));
    in = in.setModelParameter('SaveFinalState', 'on');
    in = in.setModelParameter('SaveOperatingPoint', 'on');
    in = in.setModelParameter('FinalStateName', 'xFinal');
    if ~isempty(op)
        in = in.setInitialState(op);
    end
    out = sim(in);
    if ~isprop(out, 'xFinal') && ~isfield(out, 'xFinal')
        error('PPE:RUN:SIMSTATE', 'SaveOperatingPoint did not return xFinal');
    end
    op = out.xFinal;
    dseg = model.extract_logsout(out);
    SOC = dseg.SOC(end);
    [gen, N] = t445.gen0_from_ds(dseg, t_stop, 'last');
    ds = local_append(ds, dseg, t0);
    last_dlam = d1;
    dlam_seq(i) = d1;
    if abs(t_stop - 300) < 1e-9
        % D3 warm-up boundary snapshot for COMMON_WARMUP_PARITY
        warmup_state = struct('SOC', SOC, ...
            'fuel_total_kg', dseg.fuel_total_kg(end), ...
            'P_gen_actual_kW', dseg.P_gen_actual_kW(end), ...
            'P_bat_actual_kW', dseg.P_bat_actual_kW(end), ...
            'P_load_total_kW', dseg.P_load_total_kW(end), ...
            'state_DG1', dseg.state_DG1(end), 'state_DG2', dseg.state_DG2(end), ...
            'N_on', N, 'last_dlam', d1, 't', t_stop);
    end
    if ~isempty(opt.CheckpointDir)
        ck = fullfile(opt.CheckpointDir, sprintf('ckpt_%s_%s_seg%04d.mat', ...
            char(arm), char(ctxid.voyage_id), id.segment_index));
        extra = struct('SOC', SOC, 't_stop', t_stop, 'last_dlam', d1, ...
            'gen', gen, 'N', N, 'p0', p0);
        ppe.checkpoint_mgr('save', ck, op, id, extra);
    end
end

meta = struct();
meta.schema = 'ppe_cstar_run_meta_v1';
meta.arm = char(arm);
meta.voyage_id = char(ctxid.voyage_id);
meta.study_config_sha256 = pc.study_config_sha256;
meta.t445_config_sha256 = cfg.config_sha256;
meta.slx_sha256 = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
meta.T_eval = T_eval;
meta.n_segments = numel(t0s);
meta.EndT0 = opt.EndT0;
meta.dlambda_1 = dlam_seq;
meta.window_sha256 = win_sha;
meta.oob_query_count = oob;
meta.infeasible_fallback_n = fallback_n;
meta.KS = cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den;
meta.soc_grid_step = 0.005;
meta.warmup_state = warmup_state;
meta.hard_feasibility = t445.hard_feasibility(ds, cfg, 'T_frozen_s', T_eval);
meta.fec = t445.metrics_fec(ds, cfg);
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
