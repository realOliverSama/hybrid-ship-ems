function pack = run_formal_voyage_pack(voyage_id, varargin)
% ppe.run_formal_voyage_pack — T-010 one voyage x 4 arms (P0/C0/C1/C2),
% shared rebuild/ctx, COMMON_WARMUP_PARITY at the t=300 differentiation
% boundary, checkpoint-resume for interrupted C* arms.
%
% Name-value: 'BridgeToken' (default 'formal-epoch1'), 'OutDir',
% 'CheckpointRoot'. Skips arms whose terminal evidence JSON already exists
% with the current freeze identity (idempotent resume).
p = inputParser;
p.addParameter('BridgeToken', 'formal-epoch1', @ischar);
p.addParameter('OutDir', '', @ischar);
p.addParameter('CheckpointRoot', '', @ischar);
p.parse(varargin{:});
opt = p.Results;

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if isempty(opt.OutDir)
    opt.OutDir = fullfile(ROOT, 'results', 'ppe_practical', 'formal');
end
if isempty(opt.CheckpointRoot)
    opt.CheckpointRoot = fullfile(ROOT, 'artifacts', 'ppe_formal_ckpt');
end
freeze = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
    'freeze', 'ppe_pre_execution_freeze.json')));

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
    error('PPE:PACK:LEDGER', 'voyage %s absent', voyage_id);
end
plan = fleet_v3.plan_load(ACT, 'stage_b_iid');
ctx = fleet_v3.ctx_build(plan);
rb = ppe.rebuild_voyage(plan, ctx, vrow);

pack = struct('voyage_id', char(voyage_id), 'arms', struct(), ...
    'warmup_parity', 'N/A', 'resumes', 0);
wu = struct();

for arm = {'P0', 'C0', 'C1', 'C2'}
    a = char(arm);
    ev = fullfile(opt.OutDir, char(voyage_id), [a '.json']);
    if exist(ev, 'file')
        old = jsondecode(fileread(ev));
        if isfield(old, 'freeze_sha256') ...
                && strcmp(old.freeze_sha256, freeze.freeze_sha256) ...
                && strncmp(old.terminal_status, 'COMPLETE', 8)
            fprintf('[T-010] %s %s already terminal (%s) — skip\n', ...
                voyage_id, a, old.terminal_status);
            pack.arms.(a) = old;
            if isfield(old, 'warmup_state'), wu.(a) = old.warmup_state; end
            local_purge_arm_checkpoints(opt.CheckpointRoot, ...
                char(voyage_id), a);
            continue;
        end
    end
    ckdir = fullfile(opt.CheckpointRoot, char(voyage_id));
    % Arms are atomic: history-dependent window providers (C0 persistence
    % bins, C2 300 s H1 assembly) cannot be rehydrated from a checkpoint
    % payload, and the runner is bitwise deterministic, so a fresh re-run is
    % equivalent to a resumed continuation. No mid-arm resume; no checkpoint
    % writes (transport-only files, superseded by terminal records).
    local_purge_arm_checkpoints(opt.CheckpointRoot, char(voyage_id), a);
    rf = [];
    bt = '';
    if strcmp(a, 'C2'), bt = opt.BridgeToken; end
    rec = ppe.run_formal_voyage(voyage_id, a, 'BridgeToken', bt, ...
        'OutDir', opt.OutDir, 'CheckpointDir', '', ...
        'Prebuilt', {plan, ctx, rb}, 'ResumeFrom', rf);
    if ~isempty(rf), pack.resumes = pack.resumes + 1; end
    pack.arms.(a) = rec;
    if isfield(rec, 'warmup_state'), wu.(a) = rec.warmup_state; end
end

% ---- COMMON_WARMUP_PARITY (methodological integrity; not a result) ----
if all(isfield(wu, {'C0', 'C1', 'C2'}))
    fns = {'SOC', 'fuel_total_kg', 'P_gen_actual_kW', 'P_bat_actual_kW', ...
        'P_load_total_kW', 'state_DG1', 'state_DG2', 'N_on', 'last_dlam', 't'};
    par_ok = true;
    worst = 0;
    for i = 1:numel(fns)
        f = fns{i};
        v0 = wu.C0.(f); v1 = wu.C1.(f); v2 = wu.C2.(f);
        if ~(isequal(v0, v1) && isequal(v0, v2))
            par_ok = false;
            worst = max([worst, abs(v0 - v1), abs(v0 - v2)]);
        end
    end
    pack.warmup_parity = ternary(par_ok, 'PASS', 'FAIL');
    pack.warmup_parity_max_abs_diff = worst;
    if ~par_ok
        error('PPE:PACK:WARMUP_PARITY', ...
            ['COMMON_WARMUP_PARITY FAIL on %s (maxAbsDiff=%g) — integrity ', ...
             'incident; STOP formal execution (no repair from performance)'], ...
            voyage_id, worst);
    end
    % disk hygiene: all 4 arms terminal + parity PASS -> checkpoints are
    % superseded (evidence = terminal records + window hashes)
    local_purge_checkpoints(opt.CheckpointRoot, char(voyage_id));
end
pack.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z'));
vdir = fullfile(opt.OutDir, char(voyage_id));
if ~exist(vdir, 'dir'), mkdir(vdir); end
fid = fopen(fullfile(vdir, 'warmup_parity.json'), 'w');
fwrite(fid, jsonencode(pack.warmup_parity, 'PrettyPrint', true));
fclose(fid);
end

function rf = local_latest_checkpoint(ckdir, arm)
rf = [];
if ~exist(ckdir, 'dir'), return; end
d = dir(fullfile(ckdir, sprintf('ckpt_%s_*_seg*.mat', arm)));
if isempty(d), return; end
segs = zeros(numel(d), 1);
for i = 1:numel(d)
    tok = regexp(d(i).name, 'seg(\d+)\.mat$', 'tokens');
    segs(i) = str2double(tok{1}{1});
end
[~, ix] = max(segs);
S = load(fullfile(ckdir, d(ix).name));
rf = S.payload;
fprintf('[T-010] resume %s from segment t_stop=%g\n', arm, rf.extra.t_stop);
end

function s = ternary(c, a, b)
if c, s = a; else, s = b; end
end

function local_purge_arm_checkpoints(ckpt_root, voyage_id, arm)
% terminal arm: all of its checkpoints are superseded
d = dir(fullfile(ckpt_root, voyage_id, sprintf('ckpt_%s_*.mat', arm)));
for i = 1:numel(d)
    delete(fullfile(d(i).folder, d(i).name));
end
end

function local_purge_checkpoints(ckpt_root, voyage_id)
% checkpoints are resume transport only, not evidence; purge after the
% pack is terminal (idempotent no-op when nothing is left)
d = dir(fullfile(ckpt_root, voyage_id, 'ckpt_*.mat'));
for i = 1:numel(d)
    delete(fullfile(d(i).folder, d(i).name));
end
if numel(d) > 0
    fprintf('[T-010] %s: purged %d superseded checkpoints (~%.1f GB)\n', ...
        voyage_id, numel(d), numel(d) * 2.9e-3);
end
end

function local_purge_keep_latest(ckpt_root, voyage_id, arm)
% mid-run hygiene: keep only the latest checkpoint of a non-terminal arm
% (resume reads the latest segment only)
ckdir = fullfile(ckpt_root, voyage_id);
d = dir(fullfile(ckdir, sprintf('ckpt_%s_*_seg*.mat', arm)));
if numel(d) <= 1, return; end
segs = zeros(numel(d), 1);
for i = 1:numel(d)
    tok = regexp(d(i).name, 'seg(\d+)\.mat$', 'tokens');
    segs(i) = str2double(tok{1}{1});
end
[~, ix] = max(segs);
for i = 1:numel(d)
    if i ~= ix
        delete(fullfile(d(i).folder, d(i).name));
    end
end
end
