function r = qual_e2e_runner_integration()
% ppe.qual_e2e_runner_integration — E2E_FORMAL_RUNNER_INTEGRATION 资格化
% （恢复授权 §7，Q-E2E-1..18 + 附加 loader 链测试）。
%
% 全部合成/非正式 fixture；0 正式语料消费；0 密封数据访问；无结果性调参。
% 测试对象是正式路径实际调用的代码对象：
%   ppe.formal_terminal_v2   （终端管线：checker v2 → 轨迹 → 审计 → 记录）
%   ppe.formal_pack_skip_v2  （epoch-2 幂等跳过判定，§3）
%   ppe.load_epoch2_freeze_bindings（冻结加载 fail-closed 链，沙盒 ROOT）
%   python -m ppe.acceptance_v2（验收 v2，沙盒 mini-manifest）
% 覆盖注记：run_formal_voyage epoch-2 分支的冻结绑定段由 PE3-1 绑定验证
% （verify_formal_bindings --freeze2）在冻结时点核验（合成 fixture 无法在
% 冻结生成前通过真冻结路径）；本套件证明冻结生成前可证的全部集成行为。
%
% 输出 results/ppe_practical/qualification/e2e_runner_integration_qual.json。

ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
cfg = phase4f_t445_config();
PR = struct('dis_kW', 543.8, 'chg_kW', 819.8, 'dis_kWh', 6.839, ...
    'chg_kWh', 5.625);   % T-A0 canonical（冻结数值）
TF = 40;
C_bus = 40;
checks = struct();

SBX = fullfile(tempdir, 'e2e_qual_test');
if exist(SBX, 'dir'), rmdir(SBX, 's'); end
FDIR = fullfile(SBX, 'formal_epoch2');
VID = 'E2E_SYN_VOYAGE';

% ============ 合成 ds 工厂（全通道，1 Hz 可抽、0.01 s 原生）============
    function ds = mkds(uns)
        n = round(TF * 100) + 1;
        ds = struct('t', (0:0.01:TF)', ...
            'P_DG1_kW', zeros(n, 1), 'P_DG2_kW', zeros(n, 1), ...
            'state_DG1', zeros(n, 1), 'state_DG2', zeros(n, 1), ...
            'P_unserved_kW', uns * ones(n, 1), ...
            'P_prop_demand_kW', 150 * ones(n, 1), ...
            'P_prop_actual_kW', 150 * ones(n, 1), ...
            'P_load_total_kW', 150 * ones(n, 1), ...
            'P_gen_actual_kW', 0 * ones(n, 1), ...
            'P_gen_unserved_kW', zeros(n, 1), ...
            'commitment_state', 0 * ones(n, 1), ...
            'N_dispatch', 0 * ones(n, 1), ...
            'SOC', 0.60 * ones(n, 1), ...
            'P_bat_bus_kW', 150 * ones(n, 1), ...
            'P_bat_actual_kW', 150 * ones(n, 1), ...
            'P_bat_dis_avail_kW', 900 * ones(n, 1), ...
            'P_bat_chg_avail_kW', 1000 * ones(n, 1), ...
            'V_bat_V', 590 * ones(n, 1), 'I_bat_A', 260 * ones(n, 1), ...
            'V_dc_V', 650 * ones(n, 1), ...
            'derate08_active', zeros(n, 1), ...
            'delta_lambda', 0 * ones(n, 1), ...
            'forecast_source_id', 1 * ones(n, 1), ...
            'safety_s1a', zeros(n, 1), 'safety_s1b', zeros(n, 1), ...
            'safety_s2', zeros(n, 1), ...
            'fuel_total_kg', (0:0.0001:TF * 0.0001)', ...
            'last_dlam', 0, 'N_on', 0);
    end

    function ids = mkids(vdir)
        ids = struct();
        ids.voyage_id = VID;
        ids.arm = 'P0';
        ids.run_id = local_run_id('P0', VID);   % 与 skip 判定同公式
        ids.vdir = vdir;
        ids.cfg = cfg;
        ids.PR = PR;
        ids.T_eval = TF;
        ids.C_bus = C_bus;
        ids.freeze_root_sha256 = 'e2e-fixture-freeze-root';
        ids.checker_v2_sha256 = 'e2e-fixture-checker-id';
        ids.cstar_impl_sha256 = 'e2e-fixture-cstar-composite';
        ids.slx_sha256 = 'e2e-fixture-slx';
        ids.teval_identity_sha256 = 'e2e-fixture-teval';
        ids.code_commit_sha = 'e2e-fixture-commit';
        ids.corpus_identity_sha256 = 'e2e-fixture-corpus';
        ids.execution_seed = 'phase4f-ppe-formal-epoch1';
        ids.split_group_id = 'E2E_SYN_GROUP';
        ids.tier = 'SYNTHETIC';
        ids.predictor_identity = 'N/A';
        ids.wall_s = 0.1;
        ids.meta = [];
    end

    function fz = mkfz()
        fz = struct();
        fz.freeze_root_sha256 = 'e2e-fixture-freeze-root';
        fz.cstar_composite_sha256 = 'e2e-fixture-cstar-composite';
        fz.checker_v2_sha256 = 'e2e-fixture-checker-id';
    end

    function ok = err_id(fcn, ident)
        ok = false;
        try
            fcn();
        catch ME
            ok = strcmp(ME.identifier, ident);
        end
    end

vdir = fullfile(FDIR, VID);
if ~exist(vdir, 'dir'), mkdir(vdir); end

% ---- Q-E2E-15：合法可行 fixture → COMPLETE_FEASIBLE ----
ds_ok = mkds(0);
rec15 = ppe.formal_terminal_v2(ds_ok, mkids(vdir));
checks.Q_E2E_15_feasible_becomes_complete_feasible = ...
    strcmp(rec15.terminal_status, 'COMPLETE_FEASIBLE') && rec15.checker_v2.pass;

% ---- Q-E2E-1：synthetic run 产出 formal_epoch=2 ----
checks.Q_E2E_1_record_formal_epoch_2 = rec15.formal_epoch == 2;

% ---- Q-E2E-2：输出落入 epoch-2 命名空间（非 epoch-1）----
ns_ok = ~strcmp(ppe.formal_outdir(ROOT, 2), ppe.formal_outdir(ROOT, 1)) ...
    && endsWith(char(ppe.formal_outdir(ROOT, 2)), 'formal_epoch2') ...
    && startsWith(rec15.trajectory_artifact, FDIR);
checks.Q_E2E_2_epoch2_namespace = ns_ok;

% ---- Q-E2E-3/4：checker v2 被真实调用且分类字段源自 checker v2 ----
dir2 = ppe.feasibility_contract_v2(ds_ok, cfg, PR, TF, C_bus);
q3 = isfield(rec15, 'checker_v2') ...
    && strcmp(char(rec15.checker_v2.schema), 'hard_feasibility_v2') ...
    && isequal(rec15.checker_v2.items, dir2.items) ...
    && isequal(rec15.checker_v2.violations, dir2.violations);
ds_bad = mkds(50);   % 仅改 checker 相关信号
rec14 = ppe.formal_terminal_v2(ds_bad, mkids(vdir));
q3 = q3 && ~strcmp(rec14.terminal_status, rec15.terminal_status) ...
    && rec14.checker_v2.pass == false;
checks.Q_E2E_3_checker_v2_actually_called = q3;
checks.Q_E2E_4_classification_from_checker_v2 = ...
    isfield(rec14, 'checker_v2') && isfield(rec14.checker_v2, 'items') ...
    && isfield(rec14.checker_v2, 'violations') ...
    && isfield(rec14.checker_v2, 'diagnostics') ...
    && ~rec14.checker_v2.pass;

% ---- Q-E2E-14：合法 HARD_INFEASIBLE → COMPLETE_HARD_INFEASIBLE（非基础设施失败）----
% rec14 覆写了 P0.json —— 重新落盘可行 fixture 以保持后续一致性
checks.Q_E2E_14_legit_hard_infeasible = ...
    strcmp(rec14.terminal_status, 'COMPLETE_HARD_INFEASIBLE');
rec15 = ppe.formal_terminal_v2(ds_ok, mkids(vdir));   % 恢复 vdir 一致状态

% ---- Q-E2E-5/6/7：轨迹/审计工件创建 + 哈希与字节一致 ----
tpath = fullfile(vdir, 'P0.traj.v1.mat');
apath = fullfile(vdir, 'P0.audit.v1.json');
checks.Q_E2E_5_trajectory_artifact_created = exist(tpath, 'file') > 0;
checks.Q_E2E_6_trajectory_hash_matches_bytes = ...
    strcmp(model.sha256_file(tpath), rec15.trajectory_sha256);
q7 = exist(apath, 'file') > 0 ...
    && strcmp(model.sha256_file(apath), rec15.audit_sha256) ...
    && exist(fullfile(vdir, 'P0.traj.v1.sidecar.json'), 'file') > 0;
S = load(tpath);
q7 = q7 && isfield(S, 'traj') && isfield(S.traj, 'safety_hard_count') ...
    && ~isfield(S.traj, 'P_prop_limit_kW');
checks.Q_E2E_7_audit_artifact_created_and_valid = q7;

% ---- Q-E2E-8/12：工件持久化失败 → 无终端记录（写序保证）----
vdir8 = fullfile(SBX, 'failcase', VID);
if ~exist(vdir8, 'dir'), mkdir(vdir8); end
ids8 = mkids(vdir8);
ds_missing_traj_ch = mkds(0);
ds_missing_traj_ch = rmfield(ds_missing_traj_ch, 'V_dc_V');  % checker 不需、轨迹需
q8 = err_id(@() ppe.formal_terminal_v2(ds_missing_traj_ch, ids8), ...
    'PPE:TRAJ:FAILCLOSED') ...
    && ~exist(fullfile(vdir8, 'P0.json'), 'file') ...
    && ~exist(fullfile(vdir8, 'P0.traj.v1.mat'), 'file');
checks.Q_E2E_8_record_only_after_artifacts = q8;

% ---- Q-E2E-13：checker v2 FAIL CLOSED 传播为臂失败（无记录）----
vdir13 = fullfile(SBX, 'failcase13', VID);
if ~exist(vdir13, 'dir'), mkdir(vdir13); end
ids13 = mkids(vdir13);
ds_missing_checker = mkds(0);
ds_missing_checker = rmfield(ds_missing_checker, 'P_DG1_kW');
q13 = err_id(@() ppe.formal_terminal_v2(ds_missing_checker, ids13), ...
    'PPE:FC2:FAILCLOSED') ...
    && ~exist(fullfile(vdir13, 'P0.json'), 'file');
checks.Q_E2E_13_failclosed_propagates = q13;

% ---- Q-E2E-9：同完成身份重跑安全跳过 ----
ev = fullfile(vdir, 'P0.json');
[skip9, ~, why9] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
checks.Q_E2E_9_exact_identity_skips = skip9 ...
    && strcmp(why9, 'complete-identity-match');

% ---- Q-E2E-10：epoch-1 终端记录不构成 epoch-2 完成 ----
ep1dir = fullfile(SBX, 'formal_epoch1_fake', VID);
if ~exist(ep1dir, 'dir'), mkdir(ep1dir); end
ep1rec = struct('schema', 'ppe_formal_arm_voyage_v1', 'formal_epoch', 1, ...
    'run_id', local_run_id('P0', VID), 'voyage_id', VID, 'arm', 'P0', ...
    'freeze_sha256', 'epoch1-freeze-root', ...
    'terminal_status', 'COMPLETE_HARD_INFEASIBLE');
fid = fopen(fullfile(ep1dir, 'P0.json'), 'w');
fwrite(fid, jsonencode(ep1rec));
fclose(fid);
[skip10a, ~, ~] = ppe.formal_pack_skip_v2( ...
    fullfile(ep1dir, 'P0.json'), mkfz(), VID, 'P0');
q10 = ~skip10a;
movefile(fullfile(vdir, 'P0.json'), fullfile(SBX, 'P0.real.json'));
copyfile(fullfile(ep1dir, 'P0.json'), fullfile(vdir, 'P0.json'));
[skip10b, ~, why10b] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
q10 = q10 && ~skip10b && strcmp(why10b, 'identity-mismatch:formal_epoch');
movefile(fullfile(SBX, 'P0.real.json'), fullfile(vdir, 'P0.json'));
checks.Q_E2E_10_epoch1_never_counts_as_complete = q10;

% ---- Q-E2E-11：错误冻结身份不算完成 ----
fz_wrong = mkfz();
fz_wrong.freeze_root_sha256 = 'a-different-freeze-root';
[skip11, ~, why11] = ppe.formal_pack_skip_v2(ev, fz_wrong, VID, 'P0');
fz_ck = mkfz();
fz_ck.checker_v2_sha256 = 'another-checker-id';
[skip11b, ~, why11b] = ppe.formal_pack_skip_v2(ev, fz_ck, VID, 'P0');
checks.Q_E2E_11_wrong_freeze_identity_rejected = ~skip11 ...
    && strcmp(why11, 'identity-mismatch:freeze_root') ...
    && ~skip11b && strcmp(why11b, 'identity-mismatch:checker_v2_identity');

% ---- Q-E2E-12：轨迹缺失（12a）→ 原样恢复；审计缺失（12c）→ 原样恢复 ----
tkeep = fullfile(SBX, 'traj.bak');
movefile(tpath, tkeep);
[skip12a, ~, why12a] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
movefile(tkeep, tpath);   % movefile 原样恢复字节
akeep = fullfile(SBX, 'audit.bak');
movefile(apath, akeep);
[skip12c, ~, why12c] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
movefile(akeep, apath);
checks.Q_E2E_12_missing_artifact_prevents_complete = ...
    ~skip12a && strcmp(why12a, 'artifact-missing:trajectory') ...
    && ~skip12c && strcmp(why12c, 'artifact-missing:audit');

% ---- Q-E2E-18：损坏工件（字节被改）→ 哈希失配拒绝 ----
tkeep = fullfile(SBX, 'traj.bak');
copyfile(tpath, tkeep);
fcorrupt = fopen(tpath, 'a');
fwrite(fcorrupt, 'x');
fclose(fcorrupt);
[skip18a, ~, why18a] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
delete(tpath);
movefile(tkeep, tpath);   % 原样恢复
atxt = fileread(apath);
atxt = strrep(atxt, '"any_violation"', '"any_violationX"');
fid = fopen(apath, 'w');
fwrite(fid, atxt);
fclose(fid);
[skip18b, ~, why18b] = ppe.formal_pack_skip_v2(ev, mkfz(), VID, 'P0');
% 恢复 vdir 一致状态（终端管线重跑；身份字段确定性见 Q-E2E-17）
rec15 = ppe.formal_terminal_v2(ds_ok, mkids(vdir));
checks.Q_E2E_18_corrupted_artifact_rejected = ~skip18a ...
    && strcmp(why18a, 'artifact-hash-mismatch:trajectory') ...
    && ~skip18b && strcmp(why18b, 'artifact-hash-mismatch:audit');

% ---- Q-E2E-17：臂原子（无中途 resume）+ 同输入重跑身份不变 ----
% 注：trajectory/audit 哈希不纳入逐位比较 —— MAT v7.3 字节不确定性不由本
% 资格化声明；跳过语义的正确性由「记录+工件同时写」保证（Q-E2E-12/18），
% 不依赖重跑字节逐位一致。
checks.Q_E2E_17a_atomic_no_resume = err_id( ...
    @() ppe.run_formal_voyage(VID, 'P0', 'FormalEpoch', 2, ...
    'ResumeFrom', struct('extra', 1)), 'PPE:FORMAL:ATOMIC');
vdir17 = fullfile(SBX, 'detcase', VID);
if ~exist(vdir17, 'dir'), mkdir(vdir17); end
rec17a = ppe.formal_terminal_v2(mkds(0), mkids(vdir17));
rec17b = ppe.formal_terminal_v2(mkds(0), mkids(vdir17));
excl = {'utc', 'trajectory_sha256', 'audit_sha256', ...
    'trajectory_artifact', 'audit_artifact', 'trajectory_sidecar', 'wall_s'};
rec17a = rmfields(rec17a, excl);
rec17b = rmfields(rec17b, excl);
% isequaln：checker 诊断通道的 NaN 哨兵（bus_residual_bias_kW 无偏置时）
% 对 NaN-aware 相等是合法的
checks.Q_E2E_17b_identity_stable = isequaln(rec17a, rec17b);

% ---- 附加：epoch-2 冻结加载链 fail-closed（沙盒 ROOT）----
[ldr_ok, ldr_neg] = local_loader_tests(ROOT, SBX);
checks.Q_E2E_19_loader_chain_positive = ldr_ok;
checks.Q_E2E_20_loader_chain_failclosed = ldr_neg;

% ---- Q-E2E-16：验收 v2 正确计数 synthetic fixture 且不计 epoch-1 ----
fid = fopen(fullfile(vdir, 'warmup_parity.json'), 'w');
fwrite(fid, jsonencode('PASS'));
fclose(fid);
checks.Q_E2E_16_acceptance_v2_sandbox = local_acceptance_tests( ...
    ROOT, SBX, FDIR, ep1dir, TF);

% ---- 汇总 ----
fn = fieldnames(checks);
allpass = true;
for i = 1:numel(fn)
    v = checks.(fn{i});
    if ~(islogical(v) && v), allpass = false; fprintf('  FAIL %s\n', fn{i}); end
end
r = struct('schema', 'ppe_e2e_runner_integration_qual_v1', ...
    'purpose', 'QUALIFICATION', 'pass', allpass, 'checks', checks, ...
    'coverage_note', ['synthetic fixtures through the real terminal ' ...
    'pipeline, skip predicate, freeze-loader chain and acceptance v2; ' ...
    'voyage-level freeze binding verified by PE3-1 binding verification ' ...
    'against the regenerated freeze (pre-freeze limitation, documented)'], ...
    'implementation_sha256', struct( ...
    'run_formal_voyage_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'run_formal_voyage.m')), ...
    'run_formal_voyage_pack_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'run_formal_voyage_pack.m')), ...
    'run_formal_manifest_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'run_formal_manifest.m')), ...
    'formal_terminal_v2_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'formal_terminal_v2.m')), ...
    'formal_pack_skip_v2_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'formal_pack_skip_v2.m')), ...
    'write_traj_v1_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'write_traj_v1.m')), ...
    'write_audit_v1_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'write_audit_v1.m')), ...
    'load_epoch2_freeze_bindings_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'load_epoch2_freeze_bindings.m')), ...
    'formal_outdir_m', model.sha256_file(fullfile(ROOT, ...
    'scripts', '+ppe', 'formal_outdir.m')), ...
    'acceptance_v2_py', model.sha256_file(fullfile(ROOT, ...
    'python', 'ppe', 'acceptance_v2.py'))), ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z')));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 'e2e_runner_integration_qual.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[e2e-qual] pass=%d checks=%d\n', r.pass, numel(fn));
if ~r.pass
    error('PPE:E2E:FAIL', 'E2E runner integration qualification FAIL');
end
end

% ==========================================================================
function s = rmfields(s, names)
for i = 1:numel(names)
    if isfield(s, names{i}), s = rmfield(s, names{i}); end
end
end

function [ok_pos, ok_neg] = local_loader_tests(ROOT, SBX)
% 沙盒 ROOT 内构建最小但 schema 正确的冻结/门/超链，证明加载器 fail-closed 链
lr = fullfile(SBX, 'loaderroot');
if exist(lr, 'dir'), rmdir(lr, 's'); end
fzd = fullfile(lr, 'results', 'ppe_practical', 'freeze');
cfgd = fullfile(lr, 'config');
ppd = fullfile(lr, 'scripts', '+ppe');
for d = {fzd, cfgd, ppd}
    if ~exist(d{1}, 'dir'), mkdir(d{1}); end
end
copyfile(fullfile(ROOT, 'config', 'phase4f_ppe_hard_feasibility.json'), ...
    fullfile(cfgd, 'phase4f_ppe_hard_feasibility.json'));
files = {'run_cstar.m', 'feasibility_contract_v2.m', 'write_traj_v1.m', ...
    'write_audit_v1.m', 'run_formal_manifest.m', ...
    'run_formal_voyage_pack.m', 'run_formal_voyage.m', ...
    'formal_terminal_v2.m'};
for i = 1:numel(files)
    copyfile(fullfile(ROOT, 'scripts', '+ppe', files{i}), ...
        fullfile(ppd, files{i}));
end
copyfile(fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'formal_execution_manifest.json'), fullfile(fzd, 'formal_execution_manifest.json'));

bd = struct( ...
    'run_cstar_amended_sha256', model.sha256_file(fullfile(ppd, 'run_cstar.m')), ...
    'checker_v2_sha256', model.sha256_file(fullfile(ppd, 'feasibility_contract_v2.m')), ...
    'traj_writer_sha256', model.sha256_file(fullfile(ppd, 'write_traj_v1.m')), ...
    'audit_writer_sha256', model.sha256_file(fullfile(ppd, 'write_audit_v1.m')), ...
    'runner_manifest_sha256', model.sha256_file(fullfile(ppd, 'run_formal_manifest.m')), ...
    'runner_pack_sha256', model.sha256_file(fullfile(ppd, 'run_formal_voyage_pack.m')), ...
    'runner_voyage_sha256', model.sha256_file(fullfile(ppd, 'run_formal_voyage.m')), ...
    'terminal_pipeline_sha256', model.sha256_file(fullfile(ppd, 'formal_terminal_v2.m')), ...
    'formal_execution_manifest_file_sha256', model.sha256_file(fullfile(fzd, 'formal_execution_manifest.json')), ...
    'hard_feasibility_config_file_sha256', model.sha256_file(fullfile(cfgd, 'phase4f_ppe_hard_feasibility.json')), ...
    'modele_slx_sha256', 'e2e-fixture-slx', ...
    'teval_table_file_sha256', 'e2e-fixture-teval-file', ...
    'teval_table_sha256', 'e2e-fixture-teval', ...
    'corpus_identity_sha256', 'e2e-fixture-corpus', ...
    'code_commit_sha', 'e2e-fixture-commit', ...
    'cstar_composite_sha256', ppe.cstar_impl_sha(), ...
    'predictor', struct('keras', struct('sha256', 'x'), ...
    'selected_config', struct('sha256', 'x'), ...
    'formal_norm_v3', struct('sha256', 'x'), ...
    'predictor_norm_ext_v1', struct('sha256', 'x')));
doc = struct('schema', 'ppe_epoch2_pre_execution_freeze_v3', ...
    'formal_epoch', 2, ...
    'canonical_reserves', struct('P_reserve_dis_kW', 543.8, ...
    'E_reserve_dis_kWh', 6.839, 'P_reserve_chg_kW', 819.8, ...
    'E_reserve_chg_kWh', 5.625), ...
    'supersedes_freeze_root_sha256', 'defective-root', ...
    'cstar_execution_seed', 'phase4f-ppe-formal-epoch1', ...
    'output_namespace', 'results/ppe_practical/formal_epoch2', ...
    'epoch2_result_bearing_runs', 0, ...
    'bindings', bd);
fzfile = fullfile(fzd, 'epoch2_pre_execution_freeze_v2.json');
fid = fopen(fzfile, 'w');
fwrite(fid, jsonencode(doc));
fclose(fid);
root_sha = model.sha256_file(fzfile);
sup = struct('disposition', 'SUPERSEDED_BEFORE_FORMAL_EXECUTION', ...
    'superseded_freeze_root', 'defective-root', ...
    'replacement_freeze_root', root_sha);
fid = fopen(fullfile(fzd, 'epoch2_freeze_supersession.json'), 'w');
fwrite(fid, jsonencode(sup));
fclose(fid);
gate = struct('schema', 'PPE_EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2', ...
    'overall', true, 'freeze_file_sha256', root_sha);
gatefile = fullfile(fzd, 'EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json');
fid = fopen(gatefile, 'w');
fwrite(fid, jsonencode(gate));
fclose(fid);

ok_pos = false;
try
    b = ppe.load_epoch2_freeze_bindings(lr);
    ok_pos = b.formal_epoch == 2 ...
        && strcmp(b.freeze_root_sha256, root_sha) ...
        && b.reserves.P_reserve_dis_kW == 543.8;
catch
    ok_pos = false;
end

% 负例 1：门 overall=false
gate.overall = false;
fid = fopen(gatefile, 'w');
fwrite(fid, jsonencode(gate));
fclose(fid);
neg1 = err_id_local(@() ppe.load_epoch2_freeze_bindings(lr), 'PPE:E2FZ:GATE');
gate.overall = true;
fid = fopen(gatefile, 'w');
fwrite(fid, jsonencode(gate));
fclose(fid);
% 负例 2：冻结字节根漂移
raw = fileread(fzfile);
fid = fopen(fzfile, 'a');
fwrite(fid, ' ');
fclose(fid);
neg2 = err_id_local(@() ppe.load_epoch2_freeze_bindings(lr), 'PPE:E2FZ:ROOT');
fid = fopen(fzfile, 'w');
fwrite(fid, raw);
fclose(fid);
% 负例 3：超链不互认（改后必须恢复，否则 neg4 误报 SUP）
sup2 = sup;
sup2.replacement_freeze_root = 'other-root';
fid = fopen(fullfile(fzd, 'epoch2_freeze_supersession.json'), 'w');
fwrite(fid, jsonencode(sup2));
fclose(fid);
neg3 = err_id_local(@() ppe.load_epoch2_freeze_bindings(lr), 'PPE:E2FZ:SUP');
fid = fopen(fullfile(fzd, 'epoch2_freeze_supersession.json'), 'w');
fwrite(fid, jsonencode(sup));
fclose(fid);
% 负例 4：绑定文件字节漂移
f = fullfile(ppd, 'write_traj_v1.m');
rawf = fileread(f);
fid = fopen(f, 'a');
fwrite(fid, '% drift');
fclose(fid);
neg4 = err_id_local(@() ppe.load_epoch2_freeze_bindings(lr), 'PPE:E2FZ:DRIFT');
fid = fopen(f, 'w');
fwrite(fid, rawf);
fclose(fid);
ok_neg = neg1 && neg2 && neg3 && neg4;
end

function ok = err_id_local(fcn, ident)
ok = false;
try
    fcn();
catch ME
    ok = strcmp(ME.identifier, ident);
end
end

function rid = local_run_id(arm, vid)
% 与 formal_pack_skip_v2 / run_formal_voyage 相同的确定性 run_id 公式
md = java.security.MessageDigest.getInstance('SHA-256');
raw = uint8(sprintf('ppe|%s|%s', char(arm), char(vid)));
d = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
rid = h(1:16);
end

function ok = local_acceptance_tests(ROOT, SBX, FDIR, ep1dir, TF)
% 沙盒 mini-manifest + mini-freeze：验收 v2 正确计数 1 个 synthetic 身份、
% 拒绝 epoch-1 风格输出；损坏工件被拒。
pydir = fullfile(ROOT, 'python');
oldpp = getenv('PYTHONPATH');
setenv('PYTHONPATH', pydir);
cleanup = onCleanup(@() setenv('PYTHONPATH', oldpp)); %#ok<NASGU>

vid = 'E2E_SYN_VOYAGE';
recp = fullfile(FDIR, vid, 'P0.json');
rec = jsondecode(fileread(recp));
% mini manifest（结构同正式，n=1；runs 为 JSON 数组）
runrow = struct('T_eval', TF, 'arm', 'P0', 'exec_order', 0, ...
    'run_id', char(rec.run_id), 'voyage_id', vid, 'tier', 'SYNTHETIC', ...
    'split_group_id', 'E2E_SYN_GROUP');
man = struct('schema', 'ppe_formal_execution_manifest', 'study', 'e2e-sbx', ...
    'n_runs', 1, 'n_voyages', 1, 'arms', 'P0', ...
    'execution_order_rule', 'e2e', 'manifest_sha256', 'e2e', ...
    'teval_table_sha256', char(rec.teval_identity_sha256));
man.runs = {runrow};   % cell → JSON 数组（struct() 的 cell 语义坑：不能内联）
manp = fullfile(SBX, 'mini_manifest.json');
fid = fopen(manp, 'w');
fwrite(fid, jsonencode(man));
fclose(fid);
bd = struct('checker_v2_sha256', char(rec.checker_v2.sha256), ...
    'teval_table_sha256', char(rec.teval_identity_sha256), ...
    'cstar_composite_sha256', char(rec.cstar_impl_sha256), ...
    'modele_slx_sha256', char(rec.slx_sha256), ...
    'formal_execution_manifest_file_sha256', model.sha256_file(manp));
fz = struct('schema', 'ppe_epoch2_pre_execution_freeze_v3', ...
    'formal_epoch', 2, 'epoch2_result_bearing_runs', 1, 'bindings', bd);
fzp = fullfile(SBX, 'mini_freeze.json');
fid = fopen(fzp, 'w');
fwrite(fid, jsonencode(fz));
fclose(fid);
gate = struct('schema', 'PPE_EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2', ...
    'overall', true, 'freeze_file_sha256', model.sha256_file(fzp));
fid = fopen(fullfile(SBX, 'EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json'), 'w');
fwrite(fid, jsonencode(gate));
fclose(fid);
% 记录的 freeze_root 必须等于 mini 冻结文件字节根（验收 §6 核验）：
recroot = model.sha256_file(fzp);
rtxt = fileread(recp);
rtxt = strrep(rtxt, 'e2e-fixture-freeze-root', recroot);
fid = fopen(recp, 'w');
fwrite(fid, rtxt);
fclose(fid);
outp = fullfile(SBX, 'acceptance_sbx.json');

% 正例：synthetic epoch-2 fixture → pass（epoch-1 假目录根本不被扫描）
[st1, out1] = system(sprintf( ...
    'python3 -m ppe.acceptance_v2 --formal-dir "%s" --freeze "%s" --manifest "%s" --out "%s" --allow-nonproduction', ...
    FDIR, fzp, manp, outp));
a1 = jsondecode(fileread(outp));
ok_pos = st1 == 0 && a1.pass && a1.n_found_terminals == 1 ...
    && contains(out1, 'pass=True');

% 负例 A：把 epoch-1 风格目录当验收对象 → 拒绝（记录身份全不符）
outa = fullfile(SBX, 'acceptance_ep1.json');
[st2, ~] = system(sprintf( ...
    'python3 -m ppe.acceptance_v2 --formal-dir "%s" --freeze "%s" --manifest "%s" --out "%s" --allow-nonproduction', ...
    fileparts(ep1dir), fzp, manp, outa));
a2 = jsondecode(fileread(outa));
neg_a = st2 ~= 0 && ~a2.pass;

% 负例 B：损坏审计工件 → 拒绝
ap = fullfile(FDIR, vid, 'P0.audit.v1.json');
akeep = fullfile(SBX, 'audit2.bak');
copyfile(ap, akeep);
atxt = fileread(ap);
atxt = strrep(atxt, '"any_violation"', '"any_violationX"');
fid = fopen(ap, 'w');
fwrite(fid, atxt);
fclose(fid);
outb = fullfile(SBX, 'acceptance_corrupt.json');
[st3, ~] = system(sprintf( ...
    'python3 -m ppe.acceptance_v2 --formal-dir "%s" --freeze "%s" --manifest "%s" --out "%s" --allow-nonproduction', ...
    FDIR, fzp, manp, outb));
if exist(akeep, 'file')
    delete(ap);
    movefile(akeep, ap);
end
neg_b = st3 ~= 0;

ok = ok_pos && neg_a && neg_b;
end
