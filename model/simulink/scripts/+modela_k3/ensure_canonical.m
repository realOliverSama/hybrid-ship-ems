function ensure_canonical(ROOT)
% modela_k3.ensure_canonical — 自愈：恢复被并发会话覆盖的规范文件
% 唯一权威副本存放在 scripts/+modela_k3/canonical_data/（独立命名，不会被覆盖）。
% 若目标文件与权威副本内容不一致，则从权威副本恢复。
% 目标：config/ModelA_Parameters.m, config/ModelA_Scenarios.m,
%       scripts/01..06, README_ModelA.md
    if nargin < 1
        ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    end
    srcDir = fullfile(fileparts(mfilename('fullpath')), 'canonical_data');
    targets = {
        'config/ModelA_Parameters.m'
        'config/ModelA_Scenarios.m'
        'scripts/01_audit_legacy_model.m'
        'scripts/02_build_modelA.m'
        'scripts/03_run_smoke_tests.m'
        'scripts/04_run_scenarios.m'
        'scripts/05_export_dataset.m'
        'scripts/06_validate_results.m'
        'README_ModelA.md'
        };
    restored = {};
    for i = 1:numel(targets)
        tgt = fullfile(ROOT, targets{i});
        [~, nm, ext] = fileparts(targets{i});
        src = fullfile(srcDir, [nm ext]);
        if ~exist(src, 'file'), continue; end
        if ~exist(tgt, 'file') || ~filesEqual(src, tgt)
            if ~exist(fileparts(tgt), 'dir'), mkdir(fileparts(tgt)); end
            copyfile(src, tgt, 'f');
            restored{end+1} = targets{i}; %#ok<AGROW>
        end
    end
    if ~isempty(restored)
        fprintf('[modela] 已从权威副本恢复 %d 个文件: %s\n', ...
            numel(restored), strjoin(restored, ', '));
    end
end

function eq = filesEqual(a, b)
    eq = false;
    try
        fa = dir(a); fb = dir(b);
        if numel(fa) ~= 1 || numel(fb) ~= 1, return; end
        if fa.bytes ~= fb.bytes, return; end
        eq = isequal(modela_k3.md5file(a), modela_k3.md5file(b));
    catch
        eq = false;
    end
end
