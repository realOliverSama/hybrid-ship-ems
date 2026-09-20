function r = qual_ep3b_runner_smoke(varargin)
% ppe.qual_ep3b_runner_smoke — Generation-B 正式 runner 动态资格化
% （NONFORMAL；同一入口 ppe.run_epoch3_voyage；不产生正式结果）。
%
% R3 证据完整性：每条运行 7 个工件（终端记录/sourcesel×2/traj×2/ctrace/audit）
% R4 跨航次轨迹不覆盖：两个不同合成航次先后写入同一 OutDir，两条 traj 必须共存
% R8 四臂 wiring：P0(law5)/C0/C1/C2(law6) 绑定回读 + 分支 + 选中参考
% 额外：genB 正式命名空间（formal_epoch3_genB）在资格化期间必须保持 0 条；
%       C2 bridge 端到端由 ppe.qual_c2_integration 覆盖。
p = inputParser;
p.addParameter('T', 120, @(x) isnumeric(x) && x > 20);
p.parse(varargin{:});
opt = p.Results;
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
OUT = fullfile(ROOT, 'results', 'ppe_practical', ...
    'execution_path_recovery', 'qual_ep3b_runner_smoke', 'out');
GENB_NS = fullfile(ROOT, 'results', 'ppe_practical', 'formal_epoch3_genB');
if exist(GENB_NS, 'dir')
    n = numel(dir(fullfile(GENB_NS, '*', '*')));
    if n ~= 0
        error('PPE:E3BSMOKE:NS', 'genB 正式命名空间已有 %d 条结果', n);
    end
end

VOYA = 'EP3B_SMOKE_A';
VOYB = 'EP3B_SMOKE_B';
runs = {VOYA, 'P0'; VOYA, 'C0'; VOYA, 'C1'; VOYA, 'C2'; VOYB, 'C0'};
recs = structs(5);
for i = 1:size(runs, 1)
    recs(i).voy = runs{i, 1};
    recs(i).arm = runs{i, 2};
    recs(i).rec = ppe.run_epoch3_voyage(runs{i, 1}, runs{i, 2}, ...
        'Mode', 'smoke', 'T', opt.T, 'OutDir', OUT);
end

lawExp = containers.Map({'P0', 'C0', 'C1', 'C2'}, {5, 6, 6, 6});
wiring = true;
for i = 1:numel(recs)
    rec = recs(i).rec;
    wantLaw = lawExp(recs(i).arm);
    if ~(rec.pms_source_mode == 1 && rec.ems_ctrl_law == wantLaw)
        wiring = false;
        fprintf('  wiring FAIL %s %s\n', recs(i).voy, recs(i).arm);
    end
end

% R3: evidence completeness (7 files per run, inside per-voyage dir)
ev_ok = true;
for i = 1:numel(recs)
    vd = fullfile(OUT, recs(i).voy);
    a = recs(i).arm;
    need = {sprintf('%s.json', a), sprintf('%s.sourcesel.v1.mat', a), ...
        sprintf('%s.sourcesel.v1.sidecar.json', a), ...
        sprintf('%s.traj.v1.mat', a), sprintf('%s.traj.v1.sidecar.json', a), ...
        sprintf('%s.audit.v1.json', a), sprintf('%s.ctrace.v1.json', a)};
    for k = 1:numel(need)
        if ~exist(fullfile(vd, need{k}), 'file')
            ev_ok = false;
            fprintf('  evidence MISS %s %s\n', recs(i).voy, need{k});
        end
    end
end

% R4: cross-voyage trajectories coexist (no overwrite)
trajA = fullfile(OUT, VOYA, 'C0.traj.v1.mat');
trajB = fullfile(OUT, VOYB, 'C0.traj.v1.mat');
cross_ok = exist(trajA, 'file') == 2 && exist(trajB, 'file') == 2 ...
    && ~strcmp(trajA, trajB);

% genB formal namespace must still be empty (no result-bearing runs)
genb_n = 0;
if exist(GENB_NS, 'dir')
    genb_n = numel(dir(fullfile(GENB_NS, '*', '*')));
end

ok = wiring && ev_ok && cross_ok && genb_n == 0;

r = struct();
r.schema = 'ppe_qual_ep3b_runner_smoke_v1';
r.pass = ok;
r.NONFORMAL = true;
r.FORMAL_EPOCH3_RESULT = false;
r.entry_path = 'ppe.run_epoch3_voyage (same entry used by formal gen-B)';
r.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
r.r3_evidence_complete = ev_ok;
r.r4_cross_voyage_no_overwrite = cross_ok;
r.r8_four_arm_wiring = wiring;
r.genb_formal_runs = genb_n;
r.runs = cellfun(@(x) sprintf('%s/%s', x{1}, x{2}), ...
    num2cell(runs, 2), 'UniformOutput', false);

% C2 bridge end-to-end (existing epoch-2 qualification, same bridge path)
c2p = fullfile(ROOT, 'results', 'ppe_practical', 'qualification', ...
    'c2_integration_qual.json');
try
    ppe.qual_c2_integration();
    c2d = jsondecode(fileread(c2p));
catch ME
    c2d = struct('pass', false, 'error', ME.message);
end
r.c2_integration_pass = isfield(c2d, 'pass') && logical(c2d.pass);

fid = fopen(fullfile(ROOT, 'results', 'ppe_practical', ...
    'execution_path_recovery', 'qual_ep3b_runner_smoke', ...
    'qual_ep3b_runner_smoke_final.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[EP3B-SMOKE] pass=%d wiring=%d evidence=%d cross=%d genb_runs=%d c2=%d\n', ...
    ok, wiring, ev_ok, cross_ok, genb_n, r.c2_integration_pass);
if ~ok || ~r.c2_integration_pass
    error('PPE:E3BSMOKE:FAIL', 'genB runner smoke 未通过');
end
end

function s = structs(n)
s = repmat(struct(), n, 1);
end
