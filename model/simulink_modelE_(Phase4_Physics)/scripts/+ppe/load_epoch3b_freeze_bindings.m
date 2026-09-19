function b = load_epoch3b_freeze_bindings(ROOT, varargin)
% ppe.load_epoch3b_freeze_bindings — Generation-B epoch-3 freeze loader.
%
% 读取并 fail-closed 校验 generation-B 预执行冻结：
%   results/ppe_practical/freeze/epoch3_pre_execution_freeze_genb.json
%   (schema ppe_epoch3_pre_execution_freeze_genb_v1, campaign_generation='B')
%
% 相对 generation-A loader 的新增校验：
%   - execution_closure 逐文件字节身份（机械采集的 result-bearing 闭包；
%     闭包外文件不得无声改变正式输出）；
%   - matlab_which_all 关键函数解析快照存在；
%   - 断言集含 EXECUTION_CLOSURE_BOUND / CLEAN_TRACKED_TREE /
%     QUAL_RECOVERY_PASS / QUAL_RUNNER_SMOKE_PASS /
%     FC3_AUTHORITATIVE_IDENTITY / ATTEMPT_A_EXCLUDED /
%     FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS == 0；
%   - fc3_authoritative.sha256 == 权威 checker 身份（33a8309a...）。
%
% Name-value 'AllowProvisional'（缺省 false；仅 smoke 前置路径可用）。
pp = inputParser;
pp.addParameter('AllowProvisional', false, @(x) islogical(x) || isnumeric(x));
pp.parse(varargin{:});
allow_prov = logical(pp.Results.AllowProvisional);

FZ = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'epoch3_pre_execution_freeze_genb.json');
FZP = fullfile(ROOT, 'results', 'ppe_practical', 'freeze', ...
    'epoch3_pre_execution_freeze_genb_provisional.json');
if allow_prov && ~exist(FZ, 'file') && exist(FZP, 'file')
    % provisional freeze：仅限资格化（smoke）前置路径
    FZ = FZP;
end
if ~exist(FZ, 'file')
    error('PPE:E3B:MISSING', 'generation-B freeze 缺失：%s', FZ);
end
doc = jsondecode(fileread(FZ));
if ~(strcmp(doc.schema, 'ppe_epoch3_pre_execution_freeze_genb_v1') || ...
        strcmp(doc.schema, 'ppe_epoch3_pre_execution_freeze_genb_provisional_v1'))
    error('PPE:E3B:SCHEMA', 'schema 不符：%s', doc.schema);
end
if allow_prov && isfield(doc, 'provisional') && ~doc.provisional
    % final freeze 恰好就位：按 final 校验（无豁免）
    allow_prov = false;
end
if ~(isfield(doc, 'campaign_generation') && strcmp(doc.campaign_generation, 'B'))
    error('PPE:E3B:GEN', 'campaign_generation ~= B');
end
if ~doc.overall
    error('PPE:E3B:OVERALL', 'generation-B freeze overall=false');
end

a = doc.assertions;
mustbe = {'EXECUTION_CLOSURE_BOUND', 'CLEAN_TRACKED_TREE', ...
    'QUAL_RECOVERY_PASS', 'QUAL_RUNNER_SMOKE_PASS', ...
    'FC3_AUTHORITATIVE_IDENTITY', 'ATTEMPT_A_EXCLUDED'};
provisional_missing = {};
for i = 1:numel(mustbe)
    if ~isfield(a, mustbe{i})
        if allow_prov, provisional_missing{end + 1} = mustbe{i}; continue; end %#ok<AGROW>
        error('PPE:E3B:ASSERT', '断言缺失：%s', mustbe{i});
    end
    if ~a.(mustbe{i})
        if allow_prov, provisional_missing{end + 1} = mustbe{i}; continue; end %#ok<AGROW>
        error('PPE:E3B:ASSERT', '断言为假：%s', mustbe{i});
    end
end
if isfield(a, 'FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS') ...
        && a.FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS ~= 0 && ~allow_prov
    error('PPE:E3B:ASSERT', 'FORMAL_AUTHORITATIVE_CAMPAIGN_RUNS ~= 0');
end

% ---- 权威 FC-3 身份 ----
FC3_AUTH = '33a8309ad6f585bed5203cd0223484a0df3eda5ba8d4eeb8193e1ba85c34a4a9';
if ~isfield(doc, 'fc3_authoritative') ...
        || ~strcmp(doc.fc3_authoritative.sha256, FC3_AUTH)
    error('PPE:E3B:FC3', '权威 FC-3 checker 身份不符');
end

% ---- 执行闭包逐文件字节校验（fail-closed）----
cl = doc.execution_closure;
for i = 1:numel(cl)
    it = cl(i);
    p = fullfile(ROOT, it.path);
    if ~it.present || ~exist(p, 'file')
        error('PPE:E3B:BYTES', '闭包文件缺失：%s', it.path);
    end
    if ~strcmp(model.sha256_file(p), it.sha256)
        if allow_prov
            provisional_missing{end + 1} = it.path; %#ok<AGROW>
        else
            error('PPE:E3B:BYTES', '闭包文件字节漂移：%s', it.path);
        end
    end
end

b = struct();
b.freeze_root_sha256 = model.sha256_file(FZ);
b.schema = doc.schema;
b.formal_epoch = 3;
b.campaign_generation = 'B';
b.campaign_namespace = doc.campaign_namespace;
b.assertions = a;
b.git_head = doc.git_head;
b.execution_closure = cl;
b.matlab_which_all = doc.matlab_which_all;
b.fc3_authoritative = doc.fc3_authoritative;
b.attempt_a_exclusion_ledger = doc.attempt_a_exclusion_ledger;
b.corpus_policy = doc.corpus_policy;
b.run_manifest = doc.run_manifest;
b.provisional = allow_prov;
b.provisional_missing = provisional_missing;
end
