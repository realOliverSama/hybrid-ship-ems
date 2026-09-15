function r = run_t002_tests()
% t445.run_t002_tests — T-002 窗装配 + 真 ZOH 机器测试（合成 harness，不进 Model E 航程）
ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();

cases = struct('name', {}, 'pass', {}, 'detail', {});

d = local_test_assemble(cfg);
cases(end+1) = struct('name', 'assemble_window', 'pass', true, 'detail', d); %#ok<AGROW>

d = local_test_zoh();
cases(end+1) = struct('name', 'zoh_machine', 'pass', true, 'detail', d); %#ok<AGROW>

r = struct();
r.schema = 't445_t002_evidence_v1';
r.task = 'T-002';
r.overall = true;
r.cases = cases;
r.config_sha256 = cfg.config_sha256;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));

outDir = fullfile(ROOT, 'results', 't445_oracle_gate');
if ~exist(outDir, 'dir'), mkdir(outDir); end
outPath = fullfile(outDir, 't002_unit_test_result.json');
fid = fopen(outPath, 'w');
assert(fid > 0, 'T445:T002:WRITE', 'cannot write t002 evidence');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);

ledger = fullfile(outDir, 'access_ledger.jsonl');
row = struct('purpose', 'SYNTHETIC_SMOKE', 'task', 'T-002', ...
    'artifact', 't002_unit_test_result.json', 'utc', r.utc);
fid = fopen(ledger, 'a');
fprintf(fid, '%s\n', jsonencode(row));
fclose(fid);

fprintf('[T-002] overall=true  wrote results/t445_oracle_gate/t002_unit_test_result.json\n');
end

function detail = local_test_assemble(cfg)
Eref = cfg.b1_norm.E_ref_kWh;
Esc = cfg.b1_norm.E_scale_kWh;

t = (0:999).';
P = 600 * ones(size(t));
W = t445.assemble_oracle_window(t, P, 'T_end', 1000, ...
    'E_ref_kWh', Eref, 'E_scale_kWh', Esc);
assert(abs(W.t0(1)) < 1e-12 && abs(W.t0(end) - 990) < 1e-12);
full = W.K_rem == 30;
assert(all(abs(W.E300_true_kWh(full) - 50) < 1e-12), 'const E300');
assert(W.max_rel_1s_vs_bin < 1e-12);
ehat_expect = (50 - Eref) / Esc;
assert(all(abs(W.ehat_B1(full) - ehat_expect) < 1e-12));

t2 = (0:34).';
P2 = 100 + t2;
W2 = t445.assemble_oracle_window(t2, P2, 'T_end', 35, ...
    'E_ref_kWh', Eref, 'E_scale_kWh', Esc);
assert(isequal(W2.t0(:), [0; 10; 20]));
assert(W2.K_rem(1) == 4);
assert(abs(sum(W2.dt_bin_s{1}) - 35) < 1e-12);
assert(W2.K_rem(end) == 2);
assert(abs(sum(W2.dt_bin_s{end}) - 15) < 1e-12);
assert(W2.zero_fill == false);
mask = t2 >= 20 & t2 < 35;
E1 = sum(P2(mask)) / 3600;
assert(abs(W2.E300_true_kWh(end) - E1) < 1e-12);
assert(numel(W2.E300_true_kWh) == numel(W2.t0));

detail = sprintf('n_t0_const=%d shrinking_t0=[0 10 20] max_rel=%g', ...
    numel(W.t0), W.max_rel_1s_vs_bin);
end

function detail = local_test_zoh()
mdl = sprintf('t445zoh%d', round(1e6 * rand()));
cleanup = onCleanup(@() local_bdclose(mdl)); %#ok<NASGU>
if bdIsLoaded(mdl), bdclose(mdl); end
new_system(mdl);
load_system(mdl);
set_param(mdl, 'Solver', 'ode3', 'SolverType', 'Fixed-step', ...
    'FixedStep', '0.01', 'StopTime', '20.02', ...
    'SaveTime', 'on', 'SaveOutput', 'on', 'SaveFormat', 'StructureWithTime', ...
    'ReturnWorkspaceOutputs', 'on');

add_block('simulink/Sources/From Workspace', [mdl '/src'], ...
    'Position', [40 40 140 80], ...
    'VariableName', 'ZOH_IN', 'SampleTime', '0.01', ...
    'Interpolate', 'on', 'OutDataTypeStr', 'double');
try
    set_param([mdl '/src'], 'OutputAfterFinalValue', 'HoldingFinalValue');
catch
end
add_block('simulink/Continuous/Integrator', [mdl '/intg'], 'Position', [200 40 260 80]);
add_block('simulink/Sinks/Out1', [mdl '/y'], 'Position', [320 40 350 60]);
add_block('simulink/Sinks/Out1', [mdl '/yi'], 'Position', [320 90 350 110]);
add_line(mdl, 'src/1', 'y/1');
add_line(mdl, 'src/1', 'intg/1');
add_line(mdl, 'intg/1', 'yi/1');

assignin('base', 'ZOH_IN', [0 0; 10 1; 20 2]);

inOff = t445.make_simin_zoh(Simulink.SimulationInput(mdl), [mdl '/src']);
inOn = Simulink.SimulationInput(mdl);
inOn = inOn.setBlockParameter([mdl '/src'], 'Interpolate', 'on');

outOff = sim(inOff);
outOn = sim(inOn);

[tOff, yOff] = local_xy(outOff, 1);
[tOn, yOn] = local_xy(outOn, 1);
[~, yiOff] = local_xy(outOff, 2);
[~, yiOn] = local_xy(outOn, 2);

vOff = @(tq) local_at(tOff, yOff, tq);
assert(abs(vOff(9.99) - 0) < 1e-12, 'zoh 9.99');
assert(abs(vOff(10.00) - 1) < 1e-12, 'zoh 10.00');
assert(abs(vOff(10.01) - 1) < 1e-12, 'zoh 10.01');

near = tOn >= 9.90 & tOn <= 10.10 & abs(tOn - 10.00) > 1e-12;
assert(any(near), 'neighbourhood empty');
yOffNear = interp1(tOff, yOff, tOn(near), 'previous');
diffN = max(abs(yOn(near) - yOffNear));
assert(diffN > 1e-4, 'T445:ZOH:NODIFF', ...
    'Interpolate=on vs off neighbourhood maxdiff=%g', diffN);

dOff = local_at(tOff, yiOff, 10.00) - local_at(tOff, yiOff, 9.99);
dOn = local_at(tOn, yiOn, 10.00) - local_at(tOn, yiOn, 9.99);
assert(abs(dOff) < 5e-4, 'T445:ZOH:MINOR', 'ZOH integrator d=%g expected ~0', dOff);
assert(dOn > 5e-4, 'T445:ZOH:ONINT', 'on integrator d=%g expected ~0.01', dOn);

in2 = t445.make_simin_zoh(Simulink.SimulationInput(mdl), [mdl '/src']);
got = '';
bp = in2.BlockParameters;
for k = 1:numel(bp)
    nm = '';
    if isprop(bp(k), 'Name')
        nm = bp(k).Name;
    elseif isfield(bp(k), 'Name')
        nm = bp(k).Name;
    end
    if strcmp(nm, 'Interpolate')
        if isprop(bp(k), 'Value'), got = bp(k).Value; else, got = bp(k).Value; end
    end
end
assert(strcmp(got, 'off'), 'adapter Interpolate not off (got ''%s'')', got);

detail = sprintf('neigh_maxdiff=%g dInt_off=%g dInt_on=%g', diffN, dOff, dOn);
end

function local_bdclose(mdl)
try
    if bdIsLoaded(mdl), bdclose(mdl); end
catch
end
end

function v = local_at(t, y, tq)
[~, i] = min(abs(t - tq));
assert(abs(t(i) - tq) < 1e-9, 'T445:ZOH:TIME', 'no sample at %g (nearest %g)', tq, t(i));
v = y(i);
end

function [t, y] = local_xy(out, idx)
yout = out.yout;
if isstruct(yout) && isfield(yout, 'signals')
    t = yout.time(:);
    y = yout.signals(idx).values(:);
else
    el = yout{idx};
    t = el.Values.Time(:);
    y = el.Values.Data(:);
end
end
