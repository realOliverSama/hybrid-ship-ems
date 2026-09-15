function rec = run_formal_voyage(voyage_id, arm, varargin)
% ppe.run_formal_voyage — T-010 single arm-voyage formal run (result
% generation ONLY, TG1). No effect estimation here.
%
% Identity discipline: binds the T-009 freeze package (freeze_sha256) and
% the T-001 T_eval row; C1 consumes the exported stored windows; C2 uses
% the live bridge (frozen keras). P0 = continuous law=5 (contextual).
%
% Name-value:
%   'BridgeToken'  char — C2 bridge token (C2 only)
%   'OutDir'       char — formal evidence dir (default results/.../formal)
%   'CheckpointDir' char — segment checkpoints (resumable)
%
% Returns rec (per-arm-voyage record). Writes <OutDir>/<voyage>/<arm>.json.
p = inputParser;
p.addParameter('BridgeToken', '', @ischar);
p.addParameter('OutDir', '', @ischar);
p.addParameter('CheckpointDir', '', @ischar);
p.addParameter('Prebuilt', [], @(x) isempty(x) || iscell(x));
p.addParameter('ResumeFrom', [], @(x) isempty(x) || isstruct(x));
p.parse(varargin{:});
opt = p.Results;

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if isempty(opt.OutDir)
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', 'formal');
end
if ~exist(opt.OutDir, 'dir'), mkdir(opt.OutDir); end
vdir = fullfile(opt.OutDir, char(voyage_id));
if ~exist(vdir, 'dir'), mkdir(vdir); end

% ---- freeze binding (fail-closed) ----
freeze = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
    'freeze', 'ppe_pre_execution_freeze.json')));
gate = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
    'freeze', 'PRE_EXECUTION_FREEZE_GATE.json')));
if ~gate.overall
    error('PPE:FORMAL:FREEZE', 'freeze gate overall=false');
end
% post-freeze PRE-EXECUTION runner amendment (formal_execution had not
% started: 0 outputs, 0 FORMAL ledger entries). Fail-closed: the runner
% bytes and the composite C* identity MUST equal the amended bindings.
amend_file = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'post_freeze_runner_amendment.json');
if ~exist(amend_file, 'file')
    error('PPE:FORMAL:AMEND', 'runner amendment artifact missing');
end
amend = jsondecode(fileread(amend_file));
if ~amend.pre_execution_confirmed
    error('PPE:FORMAL:AMEND', 'amendment not confirmed pre-execution');
end
run_cstar_path = fullfile(ROOT, 'scripts', '+ppe', 'run_cstar.m');
run_cstar_sha = model.sha256_file(run_cstar_path);
if ~strcmp(run_cstar_sha, amend.run_cstar_file_sha256.amended)
    error('PPE:FORMAL:AMEND', 'run_cstar.m bytes != amended binding');
end
if ~strcmp(ppe.cstar_impl_sha(), amend.new_cstar_impl_sha256)
    error('PPE:FORMAL:AMEND', 'composite C* identity != amended binding');
end
amend_sha = model.sha256_file(amend_file);
slx_sha = model.sha256_file(fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'));
if ~strcmp(slx_sha, freeze.bindings.modele_slx_sha256)
    error('PPE:FORMAL:SLX', 'slx identity drift');
end

teval = jsondecode(fileread(fullfile(ROOT, 'config', ...
    'phase4f_ppe_teval_table.json')));
if ~strcmp(teval.teval_table_sha256, freeze.bindings.teval_table_sha256)
    error('PPE:FORMAL:TEVAL', 'T_eval table identity drift');
end
row = [];
for i = 1:numel(teval.voyages)
    if strcmp(teval.voyages(i).voyage_id, voyage_id)
        row = teval.voyages(i);
        break;
    end
end
if isempty(row)
    error('PPE:FORMAL:VOYAGE', 'voyage %s not in frozen T_eval table', voyage_id);
end
T_eval = row.T_eval;

pc = ppe.ppe_config();
cfg = pc.t445;
if ~evalin('base', "exist('ECMS_Lambda', 'var')")
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end

% ---- rebuild scenario from pass2-ledger coordinates ----
if isempty(opt.Prebuilt)
    REPO = fileparts(fileparts(ROOT));
    ACT = fullfile(REPO, 'docs', 'phase4', 'd_dataset_v3', ...
        't432_activation_record.json');
    led = jsondecode(fileread(fullfile(ROOT, 'results', 'dataset_v3', ...
        'formal', 'pass2_iid_ledger.json')));
    vrow = [];
    for i = 1:numel(led.voyages)
        if strcmp(led.voyages(i).voyage_id, voyage_id)
            vrow = led.voyages(i);
            break;
        end
    end
    if isempty(vrow)
        error('PPE:FORMAL:LEDGER', 'voyage %s absent from pass2 ledger', voyage_id);
    end
    plan = fleet_v3.plan_load(ACT, 'stage_b_iid');
    ctx = fleet_v3.ctx_build(plan);
    rb = ppe.rebuild_voyage(plan, ctx, vrow);
else
    plan = opt.Prebuilt{1}; ctx = opt.Prebuilt{2}; rb = opt.Prebuilt{3};
end

% ---- identity context (D8) ----
ctxid = struct();
ctxid.study_config_sha256 = pc.study_config_sha256;
ctxid.code_commit_sha = freeze.bindings.code_commit_sha;
ctxid.slx_sha256 = slx_sha;
ctxid.cstar_impl_sha256 = ppe.cstar_impl_sha();
ctxid.hf_config_sha256 = freeze.bindings.hard_feasibility_object_sha256;
ctxid.corpus_identity_sha256 = freeze.bindings.corpus_identity_sha256;
ctxid.teval_identity_sha256 = teval.teval_table_sha256;
ctxid.voyage_id = char(voyage_id);
if strcmp(arm, 'C2')
    pr = freeze.bindings.predictor;
    ctxid.predictor_identity = struct( ...
        'keras_sha256', pr.keras.sha256, ...
        'selected_config_sha256', pr.selected_config.sha256, ...
        'formal_norm_v3_sha256', pr.formal_norm_v3.sha256, ...
        'predictor_norm_ext_v1_sha256', pr.predictor_norm_ext_v1.sha256);
else
    ctxid.predictor_identity = 'N/A';
end

t_run = tic;
if strcmp(arm, 'P0')
    ppe.inject_voyage(ctx.MDL, rb.comp, rb.scn, 5, T_eval);
    out = sim(ctx.MDL);
    ds = gate_ctrl.get_ds(out);
    meta = struct('arm', 'P0', 'runner', 'continuous_law5');
else
    plan_file = fullfile(ROOT, 'artifacts', 'ppe_formal', 'plans', ...
        [char(voyage_id) '.json']);
    win_file = fullfile(ROOT, 'artifacts', 'ppe_formal', ...
        'c1_windows', [char(voyage_id) '.mat']);
    % fail-closed: exported per-voyage assets must match the recorded
    % formal-assets export (plan OBJECT hash embedded in the plan file;
    % C1 windows file bytes).
    fa = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
        'freeze', 'formal_assets.json')));
    % per_voyage was exported in teval-table order; jsondecode drops the
    % object keys, so map positionally (guarded by count equality)
    if fa.n_voyages ~= numel(teval.voyages)
        error('PPE:FORMAL:ASSETS', 'formal_assets voyage count drift');
    end
    favr = find(strcmp({teval.voyages.voyage_id}, voyage_id), 1);
    if isempty(favr)
        error('PPE:FORMAL:ASSETS', 'voyage %s absent from formal_assets', ...
            voyage_id);
    end
    pdoc = jsondecode(fileread(plan_file));
    if ~strcmp(pdoc.plan_sha256, fa.per_voyage(favr).plan_sha256)
        error('PPE:FORMAL:ASSETS', 'plan sha mismatch for %s', voyage_id);
    end
    if ~strcmp(model.sha256_file(win_file), ...
            fa.per_voyage(favr).c1_windows_sha256)
        error('PPE:FORMAL:ASSETS', 'c1 windows sha mismatch for %s', ...
            voyage_id);
    end
    wopts = struct('T_eval', T_eval, ...
        'c1_windows_file', win_file, ...
        'voyage_id', char(voyage_id), ...
        'water_depth_m', double(rb.scn.water_depth_m), ...
        'ctrl_mode', 0);
    br = [];
    if strcmp(arm, 'C2')
        if isempty(opt.BridgeToken)
            error('PPE:FORMAL:BRIDGE', 'C2 requires BridgeToken');
        end
        % stale bridge traffic from earlier runs must not be re-served by a
        % fresh server (transport files only; responses are deterministic)
        bdir = fullfile(ROOT, 'artifacts', 'ppe_bridge', opt.BridgeToken);
        for sub = {'req', 'resp'}
            d = fullfile(bdir, sub{1});
            if exist(d, 'dir')
                fl = dir(fullfile(d, [sub{1} '_*.mat']));
                for j = 1:numel(fl)
                    delete(fullfile(fl(j).folder, fl(j).name));
                end
            end
        end
        br = ppe.c2_bridge(opt.BridgeToken);
        wopts.c2_bridge = br;
        cleanup_br = onCleanup(@() br.stop());   % no server leaks
    end
    winfun = ppe.winfun_plan(plan_file, arm, wopts);
    ppe.inject_voyage(ctx.MDL, rb.comp, rb.scn, 6, T_eval);
    rargs = {'SkipInject', true, 'CheckpointDir', opt.CheckpointDir, ...
        'Seed', 'phase4f-ppe-formal-epoch1'};
    if ~isempty(opt.ResumeFrom)
        rf = opt.ResumeFrom;
        rargs = [rargs, {'StartT0', rf.extra.t_stop, 'InitialOp', rf.op, ...
            'InitialSOC', rf.extra.SOC, 'InitialGen', rf.extra.gen, ...
            'InitialN', rf.extra.N, 'InitialDLam', rf.extra.last_dlam, ...
            'P0Load', rf.extra.p0}];
    end
    [ds, meta] = ppe.run_cstar(rb.scn, winfun, arm, T_eval, ctxid, rargs{:});
end
wall = toc(t_run);

% ---- terminal record (result generation only) ----
hf = t445.hard_feasibility(ds, cfg, 'T_frozen_s', T_eval);
fec = t445.metrics_fec(ds, cfg);
if hf.pass
    status = 'COMPLETE_FEASIBLE';
else
    status = 'COMPLETE_HARD_INFEASIBLE';
end
rec = struct();
rec.schema = 'ppe_formal_arm_voyage_v1';
rec.run_id = local_run_id(arm, voyage_id);
rec.voyage_id = char(voyage_id);
rec.split_group_id = char(row.split_group_id);
rec.tier = char(row.tier);
rec.arm = char(arm);
rec.formal_epoch = 1;
rec.freeze_sha256 = freeze.freeze_sha256;
rec.runner_amendment_sha256 = amend_sha;
rec.code_commit_sha = freeze.bindings.code_commit_sha;
rec.cstar_impl_sha256 = ctxid.cstar_impl_sha256;
rec.slx_sha256 = slx_sha;
rec.teval_identity_sha256 = teval.teval_table_sha256;
rec.T_eval = T_eval;
rec.predictor_identity = ctxid.predictor_identity;
rec.fuel_total_kg = fec.fuel_raw_kg;
rec.SOC0 = fec.SOC0;
rec.SOC_end = fec.SOC_end;
rec.J_FEC_kg = fec.J_FEC_kg;
rec.hard_feasibility = hf;
rec.terminal_status = status;
rec.wall_s = wall;
if isstruct(meta) && isfield(meta, 'warmup_state')
    rec.warmup_state = meta.warmup_state;
    rec.oob_query_count = meta.oob_query_count;
    rec.infeasible_fallback_n = meta.infeasible_fallback_n;
    rec.n_segments = meta.n_segments;
    rec.dlambda_n = numel(meta.dlambda_1);
end
rec.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));

fid = fopen(fullfile(vdir, [char(arm) '.json']), 'w');
fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-010] %s %s -> %s (%.0f s wall)\n', voyage_id, arm, status, wall);
end

function rid = local_run_id(arm, vid)
md = java.security.MessageDigest.getInstance('SHA-256');
raw = uint8(sprintf('ppe|%s|%s', char(arm), char(vid)));
d = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
rid = h(1:16);
end
