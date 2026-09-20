function pack = run_formal_voyage_pack(voyage_id, varargin)
% ppe.run_formal_voyage_pack — one voyage x 4 arms (P0/C0/C1/C2),
% shared rebuild/ctx, COMMON_WARMUP_PARITY at the t=300 differentiation
% boundary. Arms are ATOMIC (fresh re-run; no mid-arm resume, no checkpoint
% writes).
%
% !!! FormalEpoch 是必填 name-value（1|2）：禁止隐式 epoch 推断（恢复授权 §2）。
%
% FormalEpoch = 1（历史路径，语义与产出 epoch-1 证据的版本一致）：
%   OutDir 默认 results/ppe_practical/formal；跳过条件 = 记录存在且
%   freeze_sha256 与 epoch-1 冻结一致且 COMPLETE*。
%
% FormalEpoch = 2（恢复执行路径）：
%   OutDir 默认 results/ppe_practical/formal_epoch2；绑定
%   epoch2_pre_execution_freeze_v2.json 字节根；跳过条件 =
%   ppe.formal_pack_skip_v2 的完整 epoch-2 身份匹配（formal_epoch/run_id/
%   voyage/arm/冻结根/C* 复合/checker v2 身份/终端状态/轨迹+审计工件存在且
%   哈希一致，§3）——epoch-1 记录对 epoch-2 完成判定不可见（不同命名空间，
%   且从不读取）。陈旧/失配的 epoch-2 记录先隔离到 _quarantine/ 再原子重跑。
%
% Name-value: 'FormalEpoch' (REQUIRED), 'BridgeToken' (default
% 'formal-epoch<N>'), 'OutDir', 'CheckpointRoot'.
p = inputParser;
p.addParameter('FormalEpoch', [], @(x) isnumeric(x) && any(x == [1, 2]));
p.addParameter('BridgeToken', '', @ischar);
p.addParameter('OutDir', '', @ischar);
p.addParameter('CheckpointRoot', '', @ischar);
p.parse(varargin{:});
opt = p.Results;
if isempty(opt.FormalEpoch)
    error('PPE:PACK:EPOCH', ...
        'FormalEpoch 必填（1|2）：禁止隐式 epoch 推断（恢复授权 §2）');
end
if isempty(opt.BridgeToken)
    opt.BridgeToken = sprintf('formal-epoch%d', opt.FormalEpoch);
end

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if isempty(opt.OutDir)
    opt.OutDir = ppe.formal_outdir(ROOT, opt.FormalEpoch);
end
if isempty(opt.CheckpointRoot)
    opt.CheckpointRoot = fullfile(ROOT, 'artifacts', 'ppe_formal_ckpt');
end

if opt.FormalEpoch == 1
    freeze = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
        'freeze', 'ppe_pre_execution_freeze.json')));
    fz2 = [];
else
    fz2 = ppe.load_epoch2_freeze_bindings(ROOT);
    freeze = [];
    if strcmp(fullfile(opt.OutDir), ppe.formal_outdir(ROOT, 1))
        error('PPE:PACK:NS', ...
            'epoch-2 pack 禁止指向 epoch-1 命名空间 formal/（证据不可变）');
    end
end

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
    if opt.FormalEpoch == 1
        skip = false;
        if exist(ev, 'file')
            old = jsondecode(fileread(ev));
            if isfield(old, 'freeze_sha256') ...
                    && strcmp(old.freeze_sha256, freeze.freeze_sha256) ...
                    && strncmp(old.terminal_status, 'COMPLETE', 8)
                skip = true;
            end
        end
        old = [];
    else
        [skip, old, reason] = ppe.formal_pack_skip_v2(ev, fz2, voyage_id, a);
        if ~skip && exist(ev, 'file')
            % 陈旧/失配的 epoch-2 记录：隔离后原子重跑（§3：不得静默计为完成）
            local_quarantine(opt.OutDir, char(voyage_id), a, reason);
        end
        old = [];
    end
    if skip
        if opt.FormalEpoch == 1
            fprintf('[T-010] %s %s already terminal (%s) — skip\n', ...
                voyage_id, a, jsondecode(fileread(ev)).terminal_status);
            old2 = jsondecode(fileread(ev));
            pack.arms.(a) = old2;
            if isfield(old2, 'warmup_state'), wu.(a) = old2.warmup_state; end
        else
            old2 = jsondecode(fileread(ev));
            fprintf('[T-010/e2] %s %s already terminal (%s) — skip\n', ...
                voyage_id, a, old2.terminal_status);
            pack.arms.(a) = old2;
            if isfield(old2, 'warmup_state'), wu.(a) = old2.warmup_state; end
        end
        local_purge_arm_checkpoints(opt.CheckpointRoot, char(voyage_id), a);
        continue;
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
    rec = ppe.run_formal_voyage(voyage_id, a, 'FormalEpoch', ...
        opt.FormalEpoch, 'BridgeToken', bt, 'OutDir', opt.OutDir, ...
        'CheckpointDir', '', 'Prebuilt', {plan, ctx, rb}, 'ResumeFrom', rf);
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

function local_quarantine(outdir, vid, arm, reason)
% 陈旧/失配 epoch-2 证据隔离：从命名空间移除但不销毁（审计可追溯）
vdir = fullfile(outdir, vid);
qd = fullfile(outdir, '_quarantine', sprintf('%s_%s_%s', ...
    char(datetime('now', 'Format', 'yyyyMMdd_HHmmssSSS')), vid, arm));
if ~exist(qd, 'dir'), mkdir(qd); end
pats = {fullfile(vdir, [arm '.json']), ...
    fullfile(vdir, [arm '.traj.v1.mat']), ...
    fullfile(vdir, [arm '.traj.v1.sidecar.json']), ...
    fullfile(vdir, [arm '.audit.v1.json'])};
for i = 1:numel(pats)
    if exist(pats{i}, 'file')
        movefile(pats{i}, fullfile(qd, strrep(pats{i}, vdir, '')), 'f');
    end
end
fid = fopen(fullfile(qd, 'reason.txt'), 'w');
fwrite(fid, reason);
fclose(fid);
fprintf('[T-010/e2] %s %s quarantined stale evidence (%s)\n', vid, arm, reason);
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
