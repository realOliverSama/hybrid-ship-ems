%% ========================================================================
%  72_run_cd_interface_gate.m — Phase 4-D T-016：C/D Interface Gate 薄入口
%  编排：MATLAB 侧基线完整性检查（FR-496 项⑩：Model E 冻结物理/控制基线 hash 快照，
%  对抗未授权修改）→ 调用 Python cd_interface_gate（项①~⑨）→ 汇总
%  results/dataset_v3/cd_interface_gate_result.json。
%  纪律：D0 阶段为骨架自检（对象 = pilot fixtures，结果标 NON-FORMAL，DGN-16）；
%        正式执行在 C Mission-v3 就绪 + activation record 预注册值提供后；
%        overall=true 是 T-432 解锁必要条件之一（FR-498），NON-FORMAL 不构成解锁。
%  env 参数化（D-15，缺省行为与历史版本一致）：
%    EMS_CD_VOYAGE_DIR  航次目录（缺省 <ROOT>/datasets_v3/pilot；相对路径锚 ROOT）
%    EMS_CD_FORMALITY   FORMAL | NON-FORMAL（缺省 NON-FORMAL）
%    EMS_CD_RESULT      result JSON 路径（缺省不传 --result，python 侧写其默认
%                       <ROOT>/results/dataset_v3/cd_interface_gate_result.json；
%                       相对路径锚 ROOT）
%  用法：cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/72_run_cd_interface_gate.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

% ---- env 参数（D-15）----
voyDir = getenv('EMS_CD_VOYAGE_DIR');
if isempty(voyDir)
    voyDir = fullfile(ROOT, 'datasets_v3', 'pilot');
elseif ~(startsWith(voyDir, '/') || (ispc && numel(voyDir) >= 2 && voyDir(2) == ':'))
    voyDir = fullfile(ROOT, voyDir);   % 相对路径锚 MODELE_ROOT（与 python 侧解析一致）
end
formality = getenv('EMS_CD_FORMALITY');
if isempty(formality), formality = 'NON-FORMAL'; end
resEnv = getenv('EMS_CD_RESULT');
if isempty(resEnv)
    resPath = fullfile(ROOT, 'results', 'dataset_v3', 'cd_interface_gate_result.json');
    resArg = '';
elseif startsWith(resEnv, '/') || (ispc && numel(resEnv) >= 2 && resEnv(2) == ':')
    resPath = resEnv;
    resArg = sprintf(' --result "%s"', resPath);
else
    resPath = fullfile(ROOT, resEnv);
    resArg = sprintf(' --result "%s"', resPath);
end

% ---- 项⑩：Model E 冻结基线完整性 hash 快照 ----
baseFiles = { ...
    fullfile(ROOT, 'model', 'HybridShip_ModelE.slx'), 'model/HybridShip_ModelE.slx'; ...
    fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'), 'config/ModelE_Physics_Parameters.m'; ...
    fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'), 'config/ModelE_Control_Parameters.m'; ...
    fullfile(ROOT, 'config', 'gate_baseline_modele.json'), 'config/gate_baseline_modele.json'; ...
    fullfile(ROOT, 'config', 'gate_baseline_modele_control.json'), 'config/gate_baseline_modele_control.json'};
hashes = struct('schema', 'modele_baseline_integrity_v1', ...
    'note', '冻结基线 sha256 快照；正式 C/D Gate 将对照 activation record 冻结值', ...
    'files', {{}});
allExist = true;
for i = 1:size(baseFiles, 1)
    p = baseFiles{i, 1};
    if exist(p, 'file')
        rec = struct('path', baseFiles{i, 2}, 'sha256', model.sha256_file(p));
    else
        rec = struct('path', baseFiles{i, 2}, 'sha256', 'MISSING');
        allExist = false;
    end
    hashes.files{end+1} = rec; %#ok<SAGROW>
    fprintf('[72] %-46s %s\n', baseFiles{i, 2}, rec.sha256);
end
baseline = struct('check', '10_modelE_baseline_integrity', 'pass', allExist, ...
    'verdict', 'SNAPSHOT（骨架模式：记录当前 hash，正式版对照 activation 冻结值）', ...
    'files', {hashes.files});
resDir = fullfile(ROOT, 'results', 'dataset_v3', 'cd_interface');
if ~exist(resDir, 'dir'), mkdir(resDir); end
fid = fopen(fullfile(resDir, 'baseline_integrity.json'), 'w');
fwrite(fid, jsonencode(baseline, 'PrettyPrint', true)); fclose(fid);

% ---- Python 侧项①~⑨ ----
PY = getenv('EMS_PYTHON');
if isempty(PY)
    cand = fullfile(fileparts(fileparts(ROOT)), 'model', 'simulink', '.venv_tf', 'bin', 'python');
    if exist(cand, 'file'), PY = cand; else, PY = 'python3'; end
end
actPath = fullfile(fileparts(fileparts(ROOT)), 'docs', 'phase4', 'd_dataset_v3', ...
    't432_activation_record.json');   % 仓库根 docs/（R3-M1：MODELE_ROOT 下无 docs/；正式 JSON 文件名钉死为 t432_activation_record.json）
actArg = '';
if exist(actPath, 'file'), actArg = sprintf(' --activation "%s"', actPath); end
cmd = sprintf('"%s" -m dataset_v3.cd_interface_gate --voyage-dir "%s" --formality %s%s%s', ...
    PY, voyDir, formality, actArg, resArg);
fprintf('[72] running: %s -m dataset_v3.cd_interface_gate --formality %s ...\n', PY, formality);
[st, out] = system(cmd);
disp(out);
if st ~= 0
    error('GATE:72:cd_interface_python_failed — Python 门非零退出（exit=%d）', st);
end
if ~exist(resPath, 'file')
    error('GATE:72:result_missing — cd_interface_gate result JSON 未生成（Gate FAIL 语义，AGENTS.md §12）');
end

% ---- 汇总（⑩ 基线快照并入 result 顶层键）----
res = jsondecode(fileread(resPath));
res.baseline_integrity_matlab = baseline;
fid = fopen(resPath, 'w');
fwrite(fid, jsonencode(res, 'PrettyPrint', true)); fclose(fid);
fprintf('[72] result 已并入 ⑩ 基线快照: %s\n', resPath);
fprintf('[72] ===== C/D GATE 完成（formality=%s）=====\n', formality);
if strcmp(formality, 'FORMAL')
    fprintf('[72] 正式 EXT-CD-GATE：overall=true 方构成 T-432 解锁证据之一（FR-498）\n');
else
    fprintf('[72] 正式执行前置：C Mission-v3 就绪 + activation record 预注册值（FR-495/498）\n');
    fprintf('[72] NON-FORMAL 结果不构成 T-432 解锁（tasks.md 批准亦不构成解锁）\n');
end
