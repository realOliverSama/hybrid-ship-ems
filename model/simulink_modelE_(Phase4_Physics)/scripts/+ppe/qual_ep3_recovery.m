function r = qual_ep3_recovery()
% ppe.qual_ep3_recovery — Generation-B 恢复资格化（NONFORMAL；候选字节专用）。
%
% 回归覆盖 generation-A 正式路径曾漏掉的五类缺陷 + 权威 FC-3 绑定 + 命名空间
% 隔离。动态（真实 Model-E runtime）部分由 ppe.qual_ep3b_runner_smoke 承担，
% 本函数汇总两部分并写资格化工件：
%   results/ppe_practical/execution_path_recovery/qual_ep3_recovery/
%   qual_ep3_recovery_result.json
%
% R1 C2 bridge 缺失 => fail-closed（PPE:WINFUN:BRIDGE，单元级）
% R2 P0 正式分支存在（anchor 用 C0 persistence 窗；单元级）
% R3 ctrace/audit 工件完整性（动态，见 smoke）
% R4 跨航次轨迹不覆盖（动态，见 smoke）
% R5 FC-3 调用形式（5 位置参数 + PR 源自 T-A0 推导；静态）
% R6 权威 FC-3 checker 身份（sha 33a8309a...；字节级）
% R7 命名空间隔离守卫存在（静态）
% R8 四臂 wiring（动态，见 smoke）
% 汇总 pass = R1..R7 全过 && smoke.pass && c2_integration.pass

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'scripts'), fullfile(ROOT, 'config'));
OUT = fullfile(ROOT, 'results', 'ppe_practical', 'execution_path_recovery', ...
    'qual_ep3_recovery');
if ~exist(OUT, 'dir'), mkdir(OUT); end
RUNNER = fullfile(ROOT, 'scripts', '+ppe', 'run_epoch3_voyage.m');
WINFUN = fullfile(ROOT, 'scripts', '+ppe', 'winfun_plan.m');
runner_src = fileread(RUNNER);
winfun_src = fileread(WINFUN);

checks = struct();

% ---- R1: C2 without bridge fails closed (unit level, no sim) ----
plan_file = fullfile(ROOT, 'artifacts', 'ppe_formal', 'plans', ...
    'FLTA_S2_D01004_M1.json');
opts = struct('T_eval', 4508, ...
    'c1_windows_file', fullfile(ROOT, 'artifacts', 'ppe_formal', 'c1_windows', ...
        'FLTA_S2_D01004_M1.mat'), ...
    'voyage_id', 'FLTA_S2_D01004_M1', 'water_depth_m', 20, 'ctrl_mode', 0);
wf = ppe.winfun_plan(plan_file, 'C2', opts);
id1 = '';
try
    wf(300, [], []);   % anchor t0; no bridge, no history needed (fails first)
    err1 = false;
catch ME
    err1 = true;
    id1 = ME.identifier;
end
checks.R1_c2_bridge_failclosed = err1 && strcmp(id1, 'PPE:WINFUN:BRIDGE');

% ---- R2: P0 formal branch (anchor serves the persistence window; unit) ----
wf0 = ppe.winfun_plan(plan_file, 'P0', opts);
ds = struct('t', (291:300)', 'P_load_total_kW', 600 * ones(300 - 290 + 1, 1));
[Pbar, prov] = wf0(300, ds, []);
checks.R2_p0_anchor_branch = numel(Pbar) >= 1 && all(isfinite(Pbar)) ...
    && strcmp(prov.source_id, 'C0_PERSISTENCE');

% ---- R5: FC-3 invocation form (static) ----
checks.R5_fc3_call_form = contains(runner_src, ...
    'feasibility_contract_v2(ds, cfg, PR, T_eval, local_cbus(ROOT))', ...
    'IgnoreCase', false) ...
    && contains(runner_src, 'reserve_derivation_TA0.json');
checks.R5_fc3_call_not_namevalue = ~contains(runner_src, ...
    "feasibility_contract_v2(ds, cfg, 'T_frozen_s'");

% ---- R6: authoritative FC-3 checker identity (bytes) ----
FC3_AUTH = '33a8309ad6f585bed5203cd0223484a0df3eda5ba8d4eeb8193e1ba85c34a4a9';
fc3p = fullfile(ROOT, 'python', 'ppe', 'fc3_core.py');
checks.R6_fc3_authoritative_bytes = exist(fc3p, 'file') == 2 ...
    && strcmp(model.sha256_file(fc3p), FC3_AUTH);
checks.R6_runner_records_v2_and_replay_policy = contains(runner_src, ...
    'feasibility_contract_v2') && isfile(fullfile(ROOT, 'python', 'ppe', ...
    'fc3_replay_execute.py'));

% ---- R7: namespace isolation guards (static) ----
checks.R7_namespace_guards = contains(runner_src, 'PPE:EP3:NS') ...
    && contains(runner_src, 'PPE:EP3:QHOOK') ...
    && contains(runner_src, 'formal_epoch3_genB') == false;

% ---- R1b/R8 static: runner carries the C2 token fail-closed + genB loader ----
checks.R1b_runner_c2_token_guard = contains(runner_src, 'C2BridgeToken') ...
    && contains(runner_src, 'PPE:EP3:BRIDGE');
checks.R8b_runner_genb_loader = contains(runner_src, ...
    'load_epoch3b_freeze_bindings') ...
    && contains(runner_src, 'assert_genb_path');

% ---- dynamic part: real-runtime four-arm wiring + evidence completeness ----
sm = ppe.qual_ep3b_runner_smoke();
checks.R3_ctrace_audit_complete = sm.r3_evidence_complete;
checks.R4_cross_voyage_no_overwrite = sm.r4_cross_voyage_no_overwrite;
checks.R8_four_arm_wiring = sm.r8_four_arm_wiring;
checks.c2_integration = sm.c2_integration_pass;

allf = fieldnames(checks);
pass = true;
for i = 1:numel(allf)
    v = checks.(allf{i});
    if ~(islogical(v) && v)
        pass = false;
        fprintf('  RECOVERY FAIL %s\n', allf{i});
    else
        fprintf('  RECOVERY ok   %s\n', allf{i});
    end
end

r = struct();
r.schema = 'ppe_qual_ep3_recovery_v1';
r.pass = pass;
r.NONFORMAL = true;
r.FORMAL_EPOCH3_RESULT = false;
r.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
r.checks = checks;
r.smoke = struct('path', rel_(fullfile(ROOT, 'results', 'ppe_practical', ...
    'execution_path_recovery', 'qual_ep3b_runner_smoke', ...
    'qual_ep3b_runner_smoke_final.json')));
if ~pass
    r.note = 'fail-closed: recovery qualification did not pass';
end
fid = fopen(fullfile(OUT, 'qual_ep3_recovery_result.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[EP3-RECOVERY] pass=%d\n', pass);
if ~pass
    error('PPE:E3REC:FAIL', '恢复资格化未通过');
end
end

function s = rel_(p)
s = strrep(p, '\', '/');
end
