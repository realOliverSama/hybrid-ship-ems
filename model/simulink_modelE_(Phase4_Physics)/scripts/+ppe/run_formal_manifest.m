function run_formal_manifest(varargin)
% ppe.run_formal_manifest — T-010 canonical execution driver (TG3 order).
%
% !!! FormalEpoch 是必填 name-value（1|2）：禁止隐式 epoch 推断（恢复授权 §2）。
%
% Iterates the frozen formal execution manifest in canonical order
% (voyage_id ascending; each pack = voyage x arms [P0,C0,C1,C2]), one retry
% per pack on infrastructure-class failure (identity-safe: terminal records
% skip, arms are atomic). A second consecutive failure rethrows and STOPS
% the batch.
%
% FormalEpoch = 1（历史路径）：pack 日志/台账写 epoch-1 命名空间。
% FormalEpoch = 2（恢复执行路径）：pack 日志写 results/ppe_practical/
%   formal_epoch2/_pack_log.jsonl；台账条目带 formal_epoch=2 与
%   freeze_root_sha256；epoch-1 证据不可变。
%
% Name-value:
%   'FormalEpoch'   REQUIRED numeric 1|2
%   'Voyages'       cellstr subset (default: all 48 in manifest order)
%   'BridgeToken'   char (default 'formal-epoch<N>')
p = inputParser;
p.addParameter('FormalEpoch', [], @(x) isnumeric(x) && any(x == [1, 2]));
p.addParameter('Voyages', {}, @iscellstr);
p.addParameter('BridgeToken', '', @ischar);
p.parse(varargin{:});
opt = p.Results;
if isempty(opt.FormalEpoch)
    error('PPE:MANIFEST:EPOCH', ...
        'FormalEpoch 必填（1|2）：禁止隐式 epoch 推断（恢复授权 §2）');
end
if isempty(opt.BridgeToken)
    opt.BridgeToken = sprintf('formal-epoch%d', opt.FormalEpoch);
end

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

outdir = ppe.formal_outdir(ROOT, opt.FormalEpoch);
if ~exist(outdir, 'dir'), mkdir(outdir); end
logfile = fullfile(outdir, '_pack_log.jsonl');

fz2root = '';
if opt.FormalEpoch == 2
    fz2 = ppe.load_epoch2_freeze_bindings(ROOT);
    fz2root = fz2.freeze_root_sha256;
    if fz2.epoch2_result_bearing_runs ~= 0
        % 冻结声明为预执行状态：如已有结果性产出，仍可幂等续跑（跳过语义
        % 保证身份安全），此处仅提示不阻止。
        fprintf('[T-010/e2] note: freeze recorded %d result-bearing runs\n', ...
            fz2.epoch2_result_bearing_runs);
    end
end

n_done = 0;
for v = 1:numel(voyages)
    vid = voyages{v};
    t_pack = tic;
    try
        pack = ppe.run_formal_voyage_pack(vid, 'FormalEpoch', ...
            opt.FormalEpoch, 'BridgeToken', opt.BridgeToken, ...
            'OutDir', outdir);
        local_log(logfile, vid, pack, '', opt.FormalEpoch, fz2root);
        n_done = n_done + 1;
        fprintf('[T-010 manifest] %d/%d %s done (%.0f s) warmup=%s\n', ...
            v, numel(voyages), vid, toc(t_pack), pack.warmup_parity);
    catch err
        fprintf('[T-010 manifest] %s failed (%s) — one retry\n', ...
            vid, err.message);
        try
            pack = ppe.run_formal_voyage_pack(vid, 'FormalEpoch', ...
                opt.FormalEpoch, 'BridgeToken', opt.BridgeToken, ...
                'OutDir', outdir);
            local_log(logfile, vid, pack, '', opt.FormalEpoch, fz2root);
            n_done = n_done + 1;
            fprintf('[T-010 manifest] %d/%d %s done on retry (%.0f s)\n', ...
                v, numel(voyages), vid, toc(t_pack));
        catch err2
            local_log(logfile, vid, struct('voyage_id', char(vid), ...
                'arms', struct(), 'warmup_parity', 'N/A', 'resumes', -1), ...
                err2.message, opt.FormalEpoch, fz2root);
            rethrow(err2);
        end
    end
end
fprintf('[T-010 manifest] complete: %d/%d packs ok\n', n_done, ...
    numel(voyages));
end

function local_log(logfile, vid, pack, errmsg, formal_epoch, fz2root)
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
row = struct('utc', utc, 'formal_epoch', formal_epoch, ...
    'voyage_id', char(vid), 'terminal', arms, ...
    'warmup_parity', pack.warmup_parity, 'resumes', pack.resumes, ...
    'error', errmsg);
if formal_epoch == 2
    row.freeze_root_sha256 = fz2root;
end
fid = fopen(logfile, 'a');
fwrite(fid, [jsonencode(row) newline]);
fclose(fid);
if isempty(errmsg)
    % FORMAL access-ledger entry (FR-874: legal after freeze overall=true)
    entry = struct('actor', 'ppe-main-agent', ...
        'artifact', sprintf('T-010 formal pack %s (epoch %d)', ...
        char(vid), formal_epoch), ...
        'note', '4-arm formal result generation; no comparative analysis (TG1)', ...
        'purpose', 'FORMAL', 'split_contact', 'iid-formal-corpus', ...
        'formal_epoch', formal_epoch, 'utc', utc);
    if formal_epoch == 2
        entry.freeze_root_sha256 = fz2root;
    end
    led = fullfile(ppe.ppe_root(), 'results', 'ppe_practical', ...
        'access_ledger.jsonl');
    fid = fopen(led, 'a');
    fwrite(fid, [jsonencode(entry) newline]);
    fclose(fid);
end
end
