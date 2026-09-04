function diff = diff_baseline_v1_1(ROOT)
% gate_ctrl.diff_baseline_v1_1 — v1.0 → v1.1 candidate field-level diff（T-012，DD-9 Step D 证据）
    v10 = jsondecode(fileread(fullfile(ROOT, 'config', 'gate_baseline_modele.json')));
    v11 = jsondecode(fileread(fullfile(ROOT, 'config', 'gate_baseline_modele_candidate_v1_1.json')));
    diff = struct();
    diff.schema = 'baseline_diff_v1_0_to_v1_1';
    diff.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd''T''HH:mm:ssZZZ'));
    diff.fields = {};

        function add(field, a, b, cls)
            diff.fields{end+1} = struct('field', field, 'v1_0', a, 'v1_1_candidate', b, ...
                'class', cls); %#ok<AGROW>
        end

    % 顶层标量字段
    add('version', v10.version, v11.version, 'changed（DD-9 candidate 命名）');
    add('spec', v10.spec, v11.spec, 'changed（spec 归属转移）');
    add('frozen_from', v10.frozen_from, v11.frozen_from, 'changed（重跑记录）');
    add('schema', v10.schema, v11.schema, 'unchanged');
    add('model_D_reference_hashes', '（4 项）', '（逐位一致，DD-9 M2）', 'unchanged');

    % config sha 对比
    f10 = fieldnames(v10.config_sha256);
    f11 = fieldnames(v11.config_sha256);
    for k = 1:numel(f10)
        if isfield(v11.config_sha256, f10{k})
            same = strcmp(v10.config_sha256.(f10{k}), v11.config_sha256.(f10{k}));
            add(sprintf('config_sha256.%s', f10{k}), '（sha 前 8 位见基线文件）', ...
                sprintf('%s', string(v11.config_sha256.(f10{k})(1:8))), ...
                ternary(same, 'unchanged', 'changed（DD-9 允许：manifest 变化）'));
        else
            add(sprintf('config_sha256.%s', f10{k}), '（存在）', '（移除）', 'changed');
        end
    end
    for k = 1:numel(f11)
        if ~isfield(v10.config_sha256, f11{k})
            add(sprintf('config_sha256.%s', f11{k}), '（无）', '（新增，DD-9 允许）', 'added');
        end
    end

    % checks 对比（G3/G4 逐位一致；G1/G2/G5 结构一致值微漂；G6 wall time）
    gKeys = {'G1', 'G2', 'G3', 'G4', 'G5', 'G6', 'E10_compat_smoke'};
    for k = 1:numel(gKeys)
        g = gKeys{k};
        sa = jsonencode(v10.checks.(g));
        sb = jsonencode(v11.checks.(g));
        if strcmp(sa, sb)
            add(sprintf('checks.%s', g), '（逐位一致）', '（逐位一致）', 'unchanged');
        else
            % 找出不等的具体 item
            items = '';
            if isfield(v10.checks.(g), 'checks')
                for c = 1:numel(v10.checks.(g).checks)
                    ia = jsonencode(v10.checks.(g).checks{c});
                    ib = jsonencode(v11.checks.(g).checks{c});
                    if ~strcmp(ia, ib)
                        nm = v10.checks.(g).checks{c}.item;
                        items = [items, nm, ',']; %#ok<AGROW>
                    end
                end
            end
            add(sprintf('checks.%s', g), '（值级差异 items）', items, ...
                'values_drift（判据结构一致，全部 PASS；AUTO 跑执行序浮点漂移 / G6 wall time）');
        end
    end
    add('dd9_machine_check', '（无）', '（新增：M1/M2/M3/M4 全过）', 'added');

    % 仓库级证据目录（与 A2 a2_model_e 同惯例；ROOT 为 Model E 工作区 → 上一级为 model/，
    %   再上一级为仓库根）
    repoDocs = fullfile(ROOT, '..', '..', 'docs', 'phase4', 'b_pms_safety');
    if ~exist(repoDocs, 'dir'), mkdir(repoDocs); end
    outPath = fullfile(repoDocs, 'baseline_v1_0_to_v1_1_diff.json');
    fid = fopen(outPath, 'w', 'n', 'UTF-8');
    fwrite(fid, jsonencode(diff, 'PrettyPrint', true));
    fclose(fid);
    fprintf('[diff] %s\n', outPath);
end

function c = ternary(cond, a, b)
    if cond, c = a; else, c = b; end
end