function r = check_g6_runtime(e01_a, e01_b, t_elapsed_s)
% gate_e.check_g6_runtime — G6 运行时判据（design §5.3，regression）——**fail-closed 版**
%   Phase 4-B T-014 amendment 2（D-B19/D-B20 关闭）：旧实现遇尺寸不匹配/缺字段即**跳过该字段**
%   ——A2 期 E01b 携带 E09 遗留注入 + 航次终止时刻差一采样 → 全字段跳过 → 空洞 PASS。
%   新语义（fail-closed，全部条件必须同时成立才 PASS）：
%     REQUIRED   = 第一跑（锚点）字段全集 ∪ 第二跑字段全集（对称）
%     缺字段 / 尺寸不匹配 / 时间轴不匹配 / 比较计数为 0 → comparability FAIL（整体 FAIL）
%     逐位不一致 → FAIL
%     PASS 当且仅当：全部 required 字段可比 ∧ 全部实际比较 ∧ 全部逐位一致
%   元数据：required_fields / compared_fields / skipped_fields / comparability_pass；
%   门禁不变量：N_compared == N_required 且 N_skipped == 0（T-014 用户裁决）。
r.name = 'G6_runtime';
r.gate_class = 'regression';
r.checks = {};

fa = fieldnames(e01_a);
fb = fieldnames(e01_b);
required = fa;                                  % 锚点字段全集为 required
extraB = setdiff(fb, required);
required = union(required, fb);                 % 对称化：任一侧字段都要求被比较
required = sort(required);

compared = {}; skipped = {}; mismatches = {};
comparability_ok = true;
det_ok = true;

for k = 1:numel(required)
    nm = required{k};
    hasA = isfield(e01_a, nm);
    hasB = isfield(e01_b, nm);
    if ~hasA || ~hasB
        comparability_ok = false;
        skipped{end+1} = sprintf('%s(missing_%s)', nm, char('A'*hasA + 'B'*(~hasA))); %#ok<SAGROW>
        continue;
    end
    va = e01_a.(nm); vb = e01_b.(nm);
    if ~(isnumeric(va) && isnumeric(vb))
        comparability_ok = false;
        skipped{end+1} = sprintf('%s(non_numeric)', nm); %#ok<SAGROW>
        continue;
    end
    if ~isequal(size(va), size(vb))
        comparability_ok = false;
        skipped{end+1} = sprintf('%s(size_%s_vs_%s)', nm, mat2str(size(va)), mat2str(size(vb))); %#ok<SAGROW>
        continue;
    end
    % 时间轴字段（t）逐位比较即隐含时间轴一致性；非标量场尺寸一致即同网格
    compared{end+1} = nm; %#ok<SAGROW>
    if ~isequal(va, vb)
        det_ok = false;
        mismatches{end+1} = nm; %#ok<SAGROW>
    end
end

n_req = numel(required);
n_cmp = numel(compared);
n_skip = numel(skipped);
count_ok = (n_cmp == n_req) && (n_cmp > 0);
extra_ok = isempty(extraB);                     % 第二跑不得携带锚点之外的字段

r.checks{end+1} = struct('item', 'comparability_fail_closed', ...
    'pass', comparability_ok, ...
    'required_fields', n_req, 'compared_fields', n_cmp, ...
    'skipped_fields', n_skip, 'skipped_detail', {skipped}, ...
    'extra_fields_in_run_b', {extraB}); %#ok<SAGROW>
r.checks{end+1} = struct('item', 'comparison_count_equals_required', ...
    'pass', count_ok, 'N_compared', n_cmp, 'N_required', n_req); %#ok<SAGROW>
r.checks{end+1} = struct('item', 'no_extra_fields_in_run_b', 'pass', extra_ok, ...
    'extra', {extraB}); %#ok<SAGROW>
r.checks{end+1} = struct('item', 'determinism_two_runs_bitwise_identical', ...
    'pass', det_ok && comparability_ok && count_ok && extra_ok, ...
    'differing_fields', strjoin(mismatches, ',')); %#ok<SAGROW>
r.checks{end+1} = struct('item', 'runtime_informational', 'pass', true, ...
    'total_wall_time_s', t_elapsed_s); %#ok<SAGROW>

r.required_fields = n_req;
r.compared_fields = n_cmp;
r.skipped_fields = n_skip;
r.comparability_pass = comparability_ok && count_ok && extra_ok;
r.pass = r.comparability_pass && det_ok && (n_cmp == n_req) && (n_skip == 0) && extra_ok;
end