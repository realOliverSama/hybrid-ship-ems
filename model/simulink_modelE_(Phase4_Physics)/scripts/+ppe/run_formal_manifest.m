function run_formal_manifest(varargin)
% ppe.run_formal_manifest — T-010 canonical execution driver (TG3 order).
%
% Iterates the frozen formal execution manifest in canonical order
% (voyage_id ascending; each pack = voyage x arms [P0,C0,C1,C2]), one retry
% per pack on infrastructure-class failure (identity-safe: terminal records
% skip, checkpoints resume, determinism qualified by T-005). A second
% consecutive failure rethrows and STOPS the batch.
%
% Appends, per SUCCESSFUL pack: a FORMAL access-ledger entry (FR-874: legal
% after freeze overall=true; no comparative analysis performed here) and a
% row to results/ppe_practical/formal/_pack_log.jsonl.
%
% Name-value:
%   'Voyages'     cellstr subset (default: all 48 in manifest order)
%   'BridgeToken' char (default 'formal-epoch1')
p = inputParser;
p.addParameter('Voyages', {}, @iscellstr);
p.addParameter('BridgeToken', 'formal-epoch1', @ischar);
p.parse(varargin{:});
opt = p.Results;

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
man = jsondecode(fileread(fullfile(ROOT, 'results', 'ppe_practical', ...
    'freeze', 'formal_execution_manifest.json')));
[~, ix] = sort([man.runs.exec_order]);
run_ids = {man.runs(ix).voyage_id};
seen = {};
for i = 1:numel(run_ids)
    if ~any(strcmp(seen, run_ids{i}))
        seen{end+1} = run_ids{i}; %#ok<AGROW>
    end
end
if isempty(opt.Voyages)
    voyages = seen;
else
    voyages = opt.Voyages;
end

outdir = fullfile(ROOT, 'results', 'ppe_practical', 'formal');
if ~exist(outdir, 'dir'), mkdir(outdir); end
logfile = fullfile(outdir, '_pack_log.jsonl');

n_done = 0;
for v = 1:numel(voyages)
    vid = voyages{v};
    t_pack = tic;
    try
        pack = ppe.run_formal_voyage_pack(vid, 'BridgeToken', opt.BridgeToken);
        local_log(logfile, vid, pack, '');
        n_done = n_done + 1;
        fprintf('[T-010 manifest] %d/%d %s done (%.0f s) warmup=%s\n', ...
            v, numel(voyages), vid, toc(t_pack), pack.warmup_parity);
    catch err
        fprintf('[T-010 manifest] %s failed (%s) — one retry\n', ...
            vid, err.message);
        try
            pack = ppe.run_formal_voyage_pack(vid, ...
                'BridgeToken', opt.BridgeToken);
            local_log(logfile, vid, pack, '');
            n_done = n_done + 1;
            fprintf('[T-010 manifest] %d/%d %s done on retry (%.0f s)\n', ...
                v, numel(voyages), vid, toc(t_pack));
        catch err2
            local_log(logfile, vid, struct('voyage_id', char(vid), ...
                'arms', struct(), 'warmup_parity', 'N/A', 'resumes', -1), ...
                err2.message);
            rethrow(err2);
        end
    end
end
fprintf('[T-010 manifest] complete: %d/%d packs ok\n', n_done, ...
    numel(voyages));
end

function local_log(logfile, vid, pack, errmsg)
utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
arms = struct();
fn = fieldnames(pack.arms);
for i = 1:numel(fn)
    a = pack.arms.(fn{i});
    if isstruct(a) && isfield(a, 'terminal_status')
        arms.(fn{i}) = a.terminal_status;
    else
        arms.(fn{i}) = 'MISSING';
    end
end
row = struct('utc', utc, 'voyage_id', char(vid), 'terminal', arms, ...
    'warmup_parity', pack.warmup_parity, 'resumes', pack.resumes, ...
    'error', errmsg);
fid = fopen(logfile, 'a');
fwrite(fid, [jsonencode(row) newline]);
fclose(fid);
if isempty(errmsg)
    % FORMAL access-ledger entry (FR-874: legal after freeze overall=true)
    entry = struct('actor', 'ppe-main-agent', ...
        'artifact', sprintf('T-010 formal pack %s', char(vid)), ...
        'note', '4-arm formal result generation; no comparative analysis (TG1)', ...
        'purpose', 'FORMAL', 'split_contact', 'iid-formal-corpus', ...
        'utc', utc);
    led = fullfile(ppe.ppe_root(), 'results', 'ppe_practical', ...
        'access_ledger.jsonl');
    fid = fopen(led, 'a');
    fwrite(fid, [jsonencode(entry) newline]);
    fclose(fid);
end
end
