function rec = formal_terminal_v2(ds, ids)
% ppe.formal_terminal_v2 — epoch-2 终端管线（恢复授权 §4 事务序）。
%
% 唯一被正式 epoch-2 路径（run_formal_voyage FormalEpoch=2）与 E2E 集成资格化
% （qual_e2e_runner_integration，合成 fixture）共用的记录组装代码对象。
%
% 事务序（顺序即正确性；任何一步失败抛错 → 臂不产生终端记录 → 臂原子重跑）：
%   1. 仿真完成（ds 已就绪，由调用方保证）；
%   2. checker v2 评估（正式分类唯一权威；FAIL CLOSED 异常原样传播）；
%   3. 1 Hz 轨迹工件原子写 + 字节哈希复验；
%   4. 高速率审计工件原子写 + 字节哈希复验；
%   5. 哈希入 rec，终端记录原子写（tmp → movefile）；
%   6. 返回 rec（此时臂才视为 COMPLETE）。
%
% ids（struct，必需字段）：
%   voyage_id, arm, run_id, vdir            —— 输出命名空间 <OutDir>/<voyage_id>
%   cfg（pc.t445）, PR（canonical reserves struct: dis_kW/chg_kW/dis_kWh/
%     chg_kWh）, T_eval, C_bus               —— checker v2 输入
%   freeze_root_sha256, checker_v2_sha256, cstar_impl_sha256, slx_sha256,
%     teval_identity_sha256, code_commit_sha, corpus_identity_sha256,
%     execution_seed                          —— 身份绑定
%   split_group_id, tier, predictor_identity  —— manifest/T_eval 行字段
%   wall_s, meta（runner meta 或 []）          —— 运行元数据
%
% 终端记录 schema：ppe_formal_arm_voyage_v2，写至 <vdir>/<arm>.json。

if ~isfield(ids, 'run_id') || isempty(ids.run_id)
    error('PPE:TERM2:ARG', 'ids.run_id 缺失');
end

% ---- 2) checker v2：正式分类唯一权威（旧 t445.hard_feasibility 降级为诊断）----
r2 = ppe.feasibility_contract_v2(ds, ids.cfg, ids.PR, ids.T_eval, ids.C_bus);
if r2.pass
    status = 'COMPLETE_FEASIBLE';
else
    status = 'COMPLETE_HARD_INFEASIBLE';
end

fec = t445.metrics_fec(ds, ids.cfg);

% ---- 3) 轨迹工件：原子写 + 复验 ----
tr = ppe.write_traj_v1(ds, ids.vdir, ids.voyage_id, ids.arm, ids.run_id);
if ~exist(tr.path, 'file')
    error('PPE:TERM2:PERSIST', '轨迹工件写入后缺失：%s', tr.path);
end
sha_re = model.sha256_file(tr.path);
if ~strcmp(sha_re, tr.sha256)
    error('PPE:TERM2:PERSIST', '轨迹哈希复验不一致：%s', tr.path);
end

% ---- 4) 高速率审计工件：原子写 + 复验 ----
aud = ppe.write_audit_v1(ds, ids.cfg, ids.vdir, ids.voyage_id, ids.arm, ids.run_id);
sha_rea = model.sha256_file(aud.path);
if ~strcmp(sha_rea, aud.sha256)
    error('PPE:TERM2:PERSIST', '审计哈希复验不一致：%s', aud.path);
end

% ---- 5) 终端记录组装（哈希已存在并复验后才允许入 rec）----
rec = struct();
rec.schema = 'ppe_formal_arm_voyage_v2';
rec.run_id = ids.run_id;
rec.voyage_id = char(ids.voyage_id);
rec.split_group_id = char(ids.split_group_id);
rec.tier = char(ids.tier);
rec.arm = char(ids.arm);
rec.formal_epoch = 2;
rec.freeze_root_sha256 = ids.freeze_root_sha256;
rec.checker_v2 = struct('sha256', ids.checker_v2_sha256, ...
    'schema', r2.schema, 'pass', r2.pass, 'items', r2.items, ...
    'violations', r2.violations, 'diagnostics', r2.diagnostics, ...
    'detail', r2.detail);
rec.terminal_status = status;
rec.classification_authority = 'ppe.feasibility_contract_v2';
rec.trajectory_sha256 = tr.sha256;
rec.trajectory_artifact = char(tr.path);
rec.trajectory_sidecar = [char(tr.path(1:end-4)), '.sidecar.json'];
rec.audit_sha256 = aud.sha256;
rec.audit_artifact = char(aud.path);
rec.canonical_reserves = ids.PR;
rec.fuel_total_kg = fec.fuel_raw_kg;
rec.SOC0 = fec.SOC0;
rec.SOC_end = fec.SOC_end;
rec.J_FEC_kg = fec.J_FEC_kg;
if isstruct(ids.meta) && isfield(ids.meta, 'warmup_state')
    % D3 COMMON_WARMUP_PARITY 依赖：pack 从 C0/C1/C2 记录读取该边界状态
    rec.warmup_state = ids.meta.warmup_state;
    rec.oob_query_count = ids.meta.oob_query_count;
    rec.infeasible_fallback_n = ids.meta.infeasible_fallback_n;
    rec.n_segments = ids.meta.n_segments;
    rec.dlambda_n = numel(ids.meta.dlambda_1);
end
rec.legacy_diagnostics = struct('note', ...
    'epoch-1 t445.hard_feasibility retained as non-authoritative continuity diagnostic', ...
    'hard_feasibility_pass', local_meta_hf_pass(ids.meta));
rec.cstar_impl_sha256 = ids.cstar_impl_sha256;
rec.slx_sha256 = ids.slx_sha256;
rec.teval_identity_sha256 = ids.teval_identity_sha256;
rec.code_commit_sha = ids.code_commit_sha;
rec.corpus_identity_sha256 = ids.corpus_identity_sha256;
rec.execution_seed = ids.execution_seed;
rec.T_eval = ids.T_eval;
rec.predictor_identity = ids.predictor_identity;
rec.wall_s = ids.wall_s;
rec.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z'));

% 终端记录原子写：tmp → movefile（最后一步；成功返回即 COMPLETE）
fp = fullfile(ids.vdir, [char(ids.arm) '.json']);
tmp = [fp '.writing'];
fid = fopen(tmp, 'w');
fwrite(fid, jsonencode(rec, 'PrettyPrint', true));
fclose(fid);
movefile(tmp, fp, 'f');
fprintf('[T-010/e2] %s %s -> %s (%.0f s wall) traj=%s...\n', ...
    char(ids.voyage_id), char(ids.arm), status, ids.wall_s, tr.sha256(1:8));
end

function p = local_meta_hf_pass(meta)
p = '';
if isstruct(meta) && isfield(meta, 'hard_feasibility') ...
        && isstruct(meta.hard_feasibility) && isfield(meta.hard_feasibility, 'pass')
    if meta.hard_feasibility.pass, p = 'PASS'; else, p = 'FAIL'; end
end
end
