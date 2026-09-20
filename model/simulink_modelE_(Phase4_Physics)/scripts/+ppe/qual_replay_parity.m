function r = qual_replay_parity(voyage_ids)
% ppe.qual_replay_parity — NONFORMAL replay-parity qualification.
%
% For each listed formal voyage: rebuild the scenario from pass2-ledger
% coordinates (ppe.rebuild_voyage), run the SAME generation law
% (EMS_CTRL_LAW=0 Baseline, continuous, scn.stop_time_s), extract the 1 Hz
% stream with the fleet_v3 grid rule, and compare against the frozen
% Dataset-v3 .mat stream channel-by-channel (bitwise target).
%
% This proves the scenario-rebuild path reproduces the frozen voyage
% contract exactly. It consumes no controller outcomes of this study.
%
% Writes results/ppe_practical/qualification/replay_parity.json.
if nargin < 1 || isempty(voyage_ids)
    voyage_ids = {};   % caller must pass explicit ids (freeze-bound set)
end
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
REPO = fileparts(fileparts(ROOT));
ACT = fullfile(REPO, 'docs', 'phase4', 'd_dataset_v3', ...
    't432_activation_record.json');
plan = fleet_v3.plan_load(ACT, 'stage_b_iid');
ctx = fleet_v3.ctx_build(plan);

led = jsondecode(fileread(fullfile(ROOT, 'results', 'dataset_v3', ...
    'formal', 'pass2_iid_ledger.json')));
vmap = containers.Map();
for i = 1:numel(led.voyages)
    vmap(led.voyages(i).voyage_id) = led.voyages(i);
end

per = struct();
allok = true;
for vi = 1:numel(voyage_ids)
    vid = voyage_ids{vi};
    vrow = vmap(vid);
    rb = ppe.rebuild_voyage(plan, ctx, vrow);
    % member_exec order: injection_list FIRST, then scenario assignments,
    % then sim. ppe.inject_voyage reproduces both halves (no sim).
    ppe.inject_voyage(ctx.MDL, rb.comp, rb.scn, 0, rb.scn.stop_time_s);
    simout = sim(ctx.MDL);
    ds = gate_ctrl.get_ds(simout);   % same extractor as generation
    % fleet_v3 1 Hz grid rule (verbatim semantics)
    tt = double(ds.t(:)) * 100;
    sel = (abs(tt - round(tt)) <= 1e-6) & (mod(round(tt), 100) == 0);
    F = load(fullfile(ROOT, 'datasets_v3', 'formal', 'iid', ...
        [vid '.mat']), '-mat');
    diffs = struct();
    worst = 0;
    fn = fieldnames(F.stream);
    sm = ctx.stream_map;
    for j = 1:numel(fn)
        nm = fn{j};
        if strcmp(nm, 'scenario_id') || strcmp(nm, 'time_s'), continue; end
        % same sourcing as generation (member_exec local_fill_stream):
        % logsout: -> ds; scenario: -> base workspace constant
        if isfield(sm, nm)
            src = char(sm.(nm).source);
        else
            src = ['logsout:' nm];
        end
        if strncmp(src, 'logsout:', 8)
            vn = matlab.lang.makeValidName(src(9:end));
            if ~isfield(ds, vn)
                diffs.(matlab.lang.makeValidName(nm)) = inf;
                worst = inf;
                continue;
            end
            a = double(ds.(vn)(sel));
        else
            a = repmat(double(evalin('base', src(10:end))), nnz(sel), 1);
        end
        b = double(F.stream.(nm)(:));
        n = min(numel(a), numel(b));
        if numel(a) ~= numel(b)
            worst = inf;
            diffs.(matlab.lang.makeValidName(nm)) = inf;
            continue;
        end
        d = max(abs(a(1:n) - b(1:n)));
        if isempty(d), d = 0; end
        diffs.(matlab.lang.makeValidName(nm)) = d;
        if d > worst, worst = d; end
    end
    % time grid identity
    tA = double(ds.t(sel));
    tB = double(F.stream.time_s(:));
    t_ok = isequal(numel(tA), numel(tB)) && max(abs(tA - tB)) == 0;
    per.(matlab.lang.makeValidName(vid)) = struct( ...
        'voyage_id', vid, 'max_abs_diff', worst, 'time_grid_equal', t_ok, ...
        'n_channels', numel(fn) - 2, 'pass', (worst == 0) && t_ok, ...
        'channel_max_abs_diff', diffs);
    if ~((worst == 0) && t_ok), allok = false; end
    fprintf('[replay-parity] %s maxAbsDiff=%g time_ok=%d\n', vid, worst, t_ok);
end

r = struct();
r.schema = 'ppe_replay_parity_v1';
r.purpose = 'QUALIFICATION';
r.voyages = voyage_ids;
r.per_voyage = per;
r.pass = allok;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 'replay_parity.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
if bdIsLoaded(ctx.MDL)
    close_system(ctx.MDL, 0);
end
fprintf('[replay-parity] overall=%d\n', r.pass);
end
