function b = load_epoch2_freeze_bindings(ROOT)
% ppe.load_epoch2_freeze_bindings — 加载并 fail-closed 校验 epoch-2 执行冻结。
%
% 校验链（任一失败即 error；恢复授权 §2/§4/§12）：
%   1. freeze/epoch2_pre_execution_freeze_v2.json 存在且 schema 正确；
%   2. gate EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json overall=true，且其
%      freeze_file_sha256 与冻结文件当前字节哈希逐位一致（MATLAB 可复验的
%      字节根，跨语言无歧义）；
%   3. 超链标记 epoch2_freeze_supersession.json：旧缺陷冻结根
%      SUPERSEDED_BEFORE_FORMAL_EXECUTION 且指向本冻结根；
%   4. 执行关键字节身份（runner/checker v2/轨迹/审计写入器/manifest/C*
%      复合/run_cstar 修正案绑定/hf 配置）与磁盘当前字节逐位一致。
%
% 返回扁平结构 b（字段供 run_formal_voyage/_pack 直接消费）。

FZ = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'epoch2_pre_execution_freeze_v2.json');
if ~exist(FZ, 'file')
    error('PPE:E2FZ:MISSING', 'epoch-2 执行冻结缺失：%s', FZ);
end
doc = jsondecode(fileread(FZ));
if ~strcmp(doc.schema, 'ppe_epoch2_pre_execution_freeze_v3')
    error('PPE:E2FZ:SCHEMA', 'epoch-2 冻结 schema 不符：%s', doc.schema);
end
if doc.formal_epoch ~= 2
    error('PPE:E2FZ:EPOCH', '冻结 formal_epoch ~= 2');
end

GATE = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'EPOCH2_PRE_EXECUTION_FREEZE_GATE_V2.json');
if ~exist(GATE, 'file')
    error('PPE:E2FZ:GATE', 'epoch-2 预执行 Gate 工件缺失：%s', GATE);
end
gate = jsondecode(fileread(GATE));
if ~gate.overall
    error('PPE:E2FZ:GATE', 'epoch-2 预执行 Gate overall=false');
end
root = model.sha256_file(FZ);
if ~strcmp(root, gate.freeze_file_sha256)
    error('PPE:E2FZ:ROOT', '冻结字节根漂移（gate 绑定 != 当前字节）');
end

SUP = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'epoch2_freeze_supersession.json');
if ~exist(SUP, 'file')
    error('PPE:E2FZ:SUP', '旧冻结超链标记缺失：%s', SUP);
end
sup = jsondecode(fileread(SUP));
if ~strcmp(sup.disposition, 'SUPERSEDED_BEFORE_FORMAL_EXECUTION')
    error('PPE:E2FZ:SUP', '旧冻结处置状态不符');
end
if ~strcmp(sup.superseded_freeze_root, doc.supersedes_freeze_root_sha256) ...
        || ~strcmp(sup.replacement_freeze_root, root)
    error('PPE:E2FZ:SUP', '超链标记与冻结文件不互认');
end

bd = doc.bindings;
b.freeze_root_sha256 = root;
b.schema = doc.schema;
b.formal_epoch = 2;
b.reserves = doc.canonical_reserves;
b.modele_slx_sha256 = bd.modele_slx_sha256;
b.teval_table_file_sha256 = bd.teval_table_file_sha256;
b.teval_table_sha256 = bd.teval_table_sha256;
b.hard_feasibility_config_file_sha256 = bd.hard_feasibility_config_file_sha256;
b.corpus_identity_sha256 = bd.corpus_identity_sha256;
b.code_commit_sha = bd.code_commit_sha;
b.cstar_execution_seed = doc.cstar_execution_seed;
b.cstar_composite_sha256 = bd.cstar_composite_sha256;
b.run_cstar_amended_sha256 = bd.run_cstar_amended_sha256;
b.checker_v2_sha256 = bd.checker_v2_sha256;
b.traj_writer_sha256 = bd.traj_writer_sha256;
b.audit_writer_sha256 = bd.audit_writer_sha256;
b.runner_manifest_sha256 = bd.runner_manifest_sha256;
b.runner_pack_sha256 = bd.runner_pack_sha256;
b.runner_voyage_sha256 = bd.runner_voyage_sha256;
b.terminal_pipeline_sha256 = bd.terminal_pipeline_sha256;
b.formal_execution_manifest_file_sha256 = bd.formal_execution_manifest_file_sha256;
b.output_namespace = doc.output_namespace;
b.predictor = bd.predictor;
b.epoch2_result_bearing_runs = doc.epoch2_result_bearing_runs;
b.supersedes_freeze_root_sha256 = doc.supersedes_freeze_root_sha256;

% ---- 字节身份复验（执行关键文件）----
chk = {
    fullfile(ROOT, 'scripts', '+ppe', 'run_cstar.m'), bd.run_cstar_amended_sha256, 'run_cstar.m';
    fullfile(ROOT, 'scripts', '+ppe', 'feasibility_contract_v2.m'), bd.checker_v2_sha256, 'feasibility_contract_v2.m';
    fullfile(ROOT, 'scripts', '+ppe', 'write_traj_v1.m'), bd.traj_writer_sha256, 'write_traj_v1.m';
    fullfile(ROOT, 'scripts', '+ppe', 'write_audit_v1.m'), bd.audit_writer_sha256, 'write_audit_v1.m';
    fullfile(ROOT, 'scripts', '+ppe', 'run_formal_manifest.m'), bd.runner_manifest_sha256, 'run_formal_manifest.m';
    fullfile(ROOT, 'scripts', '+ppe', 'run_formal_voyage_pack.m'), bd.runner_pack_sha256, 'run_formal_voyage_pack.m';
    fullfile(ROOT, 'scripts', '+ppe', 'run_formal_voyage.m'), bd.runner_voyage_sha256, 'run_formal_voyage.m';
    fullfile(ROOT, 'scripts', '+ppe', 'formal_terminal_v2.m'), bd.terminal_pipeline_sha256, 'formal_terminal_v2.m';
    fullfile(ROOT, 'results', 'ppe_practical', 'freeze', 'formal_execution_manifest.json'), bd.formal_execution_manifest_file_sha256, 'formal_execution_manifest.json';
    fullfile(ROOT, 'config', 'phase4f_ppe_hard_feasibility.json'), bd.hard_feasibility_config_file_sha256, 'phase4f_ppe_hard_feasibility.json';
    };
for i = 1:size(chk, 1)
    got = model.sha256_file(chk{i, 1});
    if ~strcmp(got, chk{i, 2})
        error('PPE:E2FZ:DRIFT', '%s 字节漂移：冻结绑定 %s… != 当前 %s…', ...
            chk{i, 3}, chk{i, 2}(1:12), got(1:12));
    end
end
if ~strcmp(ppe.cstar_impl_sha(), bd.cstar_composite_sha256)
    error('PPE:E2FZ:DRIFT', 'C* 复合身份漂移（恢复执行禁止改动 C*）');
end
end
