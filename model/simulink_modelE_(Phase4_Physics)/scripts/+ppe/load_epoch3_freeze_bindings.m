function b = load_epoch3_freeze_bindings(ROOT)
% ppe.load_epoch3_freeze_bindings — 加载并 fail-closed 校验 epoch-3 执行冻结。
%
% epoch-3 = 执行路径缺陷修复后的修正纪元（formal_epoch=3）。冻结由
% python/ppe/ep3_freeze.py 生成：**缺 Q-EP3 资格化即拒绝生成**（见该文件的
% BLOCKED 工件语义）。本加载器只接受 overall=true 且全部断言通过的冻结。
%
% 校验链（任一失败即 error）：
%   1. freeze/epoch3_pre_execution_freeze.json 存在且 schema/epoch 正确；
%   2. overall=true；
%   3. 断言全通过：CONTROL_ACTION_REACHES_PMS=proven、PMS_SOURCE_MODE_ARM_BOUND=true、
%      ALL_CONTROL_CRITICAL_WRITERS_FROZEN=true、FORMAL_EPOCH3_RUNS=0、
%      IID_TEST_ACCESS=0、OOD_ACCESS=0；
%   4. 注入链/修复/科学身份逐字节与磁盘一致（任一漂移 ⇒ error）；
%   5. 若存在 BLOCKED 工件则显式提示其存在（诊断用，不参与通过判定）。
%
% 返回扁平结构 b（字段供 epoch-3 runner 消费）。

FZ = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'epoch3_pre_execution_freeze.json');
if ~exist(FZ, 'file')
    BLK = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
        'epoch3_pre_execution_freeze_BLOCKED.json');
    if exist(BLK, 'file')
        error('PPE:E3FZ:BLOCKED', ...
            ['epoch-3 冻结不存在；存在 BLOCKED 工件（前置条件未满足）：%s\n' ...
             '请先完成 Q-EP3 非正式资格化（ppe.qual_ep3）并重跑 ' ...
             'python/ppe/ep3_freeze.py'], BLK);
    end
    error('PPE:E3FZ:MISSING', 'epoch-3 执行冻结缺失：%s', FZ);
end
doc = jsondecode(fileread(FZ));
if ~strcmp(doc.schema, 'ppe_epoch3_pre_execution_freeze_v1')
    error('PPE:E3FZ:SCHEMA', 'epoch-3 冻结 schema 不符：%s', doc.schema);
end
if doc.formal_epoch ~= 3
    error('PPE:E3FZ:EPOCH', 'epoch-3 冻结 formal_epoch ~= 3');
end
if ~doc.overall
    error('PPE:E3FZ:OVERALL', 'epoch-3 冻结 overall=false');
end
a = doc.assertions;
req = {'CONTROL_ACTION_REACHES_PMS', 'PMS_SOURCE_MODE_ARM_BOUND', ...
    'ALL_CONTROL_CRITICAL_WRITERS_FROZEN'};
for i = 1:numel(req)
    if ~isfield(a, req{i})
        error('PPE:E3FZ:ASSERT', '断言缺失：%s', req{i});
    end
end
if ~strcmp(a.CONTROL_ACTION_REACHES_PMS, 'proven') ...
        || ~a.PMS_SOURCE_MODE_ARM_BOUND || ~a.ALL_CONTROL_CRITICAL_WRITERS_FROZEN ...
        || a.FORMAL_EPOCH3_RUNS ~= 0 || a.IID_TEST_ACCESS ~= 0 || a.OOD_ACCESS ~= 0
    error('PPE:E3FZ:ASSERT', 'epoch-3 冻结断言未全通过');
end

% 字节身份（注入链 / 修复 / 科学身份）
chains = {'injection_chain', 'repaired_execution_commit'};
for c = 1:numel(chains)
    if ~isfield(doc, chains{c}), error('PPE:E3FZ:CHAIN', '缺少 %s', chains{c}); end
end
for grp = {'writers', 'repair', 'science'}
    g = grp{1};
    if ~isfield(doc.injection_chain, g), continue; end
    items = doc.injection_chain.(g);
    for i = 1:numel(items)
        it = items(i);
        p = fullfile(ROOT, it.path);
        if ~it.present || ~exist(p, 'file')
            error('PPE:E3FZ:BYTES', '绑定文件缺失：%s', it.path);
        end
        if ~strcmp(model.sha256_file(p), it.sha256)
            error('PPE:E3FZ:BYTES', '绑定文件字节漂移：%s', it.path);
        end
    end
end

b = struct();
b.freeze_root_sha256 = model.sha256_file(FZ);
b.schema = doc.schema;
b.formal_epoch = 3;
b.assertions = a;
b.repaired_execution_commit = doc.repaired_execution_commit;
b.injection_chain = doc.injection_chain;
b.corpus_policy = doc.corpus_policy;
b.run_manifest = doc.run_manifest;
end
