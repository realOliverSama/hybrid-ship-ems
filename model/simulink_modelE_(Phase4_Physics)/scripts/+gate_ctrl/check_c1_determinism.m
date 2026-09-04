function r = check_c1_determinism(dsA, dsB, label)
% gate_ctrl.check_c1_determinism — 判据 1：确定性（FR-455 判据 1，G6 口径复用）
%   Phase 4-B T-014 amendment 2（D-B20）：**fail-closed 版**——缺字段/尺寸不匹配/非数值/
%   比较计数 0 → comparability FAIL（整体 FAIL），禁止跳过式空洞 PASS（语义同 G6）。
    r.name = 'C1_determinism';
    r.label = label;
    r.checks = {};

    required = sort(union(fieldnames(dsA), fieldnames(dsB)));
    compared = {}; skipped = {}; mismatches = {};
    comparability_ok = true;
    det_ok = true;

    for k = 1:numel(required)
        nm = required{k};
        if ~isfield(dsA, nm) || ~isfield(dsB, nm)
            comparability_ok = false;
            skipped{end+1} = sprintf('%s(missing)', nm); %#ok<SAGROW>
            continue;
        end
        va = dsA.(nm); vb = dsB.(nm);
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
        compared{end+1} = nm; %#ok<SAGROW>
        if ~isequal(va, vb)
            det_ok = false;
            mismatches{end+1} = nm; %#ok<SAGROW>
        end
    end

    n_req = numel(required); n_cmp = numel(compared); n_skip = numel(skipped);
    count_ok = (n_cmp == n_req) && (n_cmp > 0);
    r.comparability_pass = comparability_ok && count_ok;
    r.required_fields = n_req;
    r.compared_fields = n_cmp;
    r.skipped_fields = n_skip;
    r.checks{end+1} = struct('item', 'comparability_fail_closed', ...
        'pass', r.comparability_pass, 'required_fields', n_req, ...
        'compared_fields', n_cmp, 'skipped_fields', n_skip, ...
        'skipped_detail', {skipped}); %#ok<SAGROW>
    r.checks{end+1} = struct('item', 'two_runs_bitwise_identical', ...
        'pass', det_ok && r.comparability_pass, ...
        'differing_fields', strjoin(mismatches, ',')); %#ok<SAGROW>
    r.pass = r.comparability_pass && det_ok;
end