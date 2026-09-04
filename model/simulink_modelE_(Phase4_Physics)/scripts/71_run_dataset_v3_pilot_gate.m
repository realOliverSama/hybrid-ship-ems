%% ========================================================================
%  71_run_dataset_v3_pilot_gate.m — Phase 4-D T-015：Dataset v3 pilot Gate（薄入口）
%  编排：调用 Python 管线（dataset_v3.run_pipeline，全链：聚合→窗口→target→split→
%  审计→manifest）→ 读取机器可读 pilot_gate_result.json → overall=true 方为 PASS。
%  纪律：结果标 PILOT（仅证明管线正确性，FR-489，统计不晋升科学基线）；
%        FAIL 时 error('GATE:*') 非零退出（AGENTS.md §12）；禁绝对路径。
%  前置：70 号已导出 pilot 航次（datasets_v3/pilot/*.mat）。
%  用法：cd model/simulink_modelE_(Phase4_Physics); eval(fileread('scripts/71_run_dataset_v3_pilot_gate.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end

% ---- Python 解释器解析（项目既定 .venv_tf；可用环境变量覆盖，禁硬编码绝对路径）----
PY = getenv('EMS_PYTHON');
if isempty(PY)
    cand = fullfile(fileparts(fileparts(ROOT)), 'model', 'simulink', '.venv_tf', 'bin', 'python');
    if exist(cand, 'file')
        PY = cand;
    else
        PY = 'python3';   % PATH 兜底
    end
end

% ---- 前置：pilot 航次存在性 ----
voyDir = fullfile(ROOT, 'datasets_v3', 'pilot');
voyages = dir(fullfile(voyDir, '*.mat'));
fprintf('[71] pilot voyages: %d（%s）\n', numel(voyages), voyDir);
if numel(voyages) == 0
    error('GATE:71:no_pilot_voyages — datasets_v3/pilot 无航次（先运行 70 号 producer）');
end

% ---- 调用 Python 管线 ----
cmd = sprintf('"%s" -m dataset_v3.run_pipeline --voyage-dir "%s" --out-dir "%s" --npz "%s" --result "%s"', ...
    PY, fullfile(voyDir), fullfile(ROOT, 'results', 'dataset_v3', 'pilot'), ...
    fullfile(ROOT, 'datasets_v3', 'pilot', 'windows_v3_pilot.npz'), ...
    fullfile(ROOT, 'results', 'dataset_v3', 'pilot', 'pilot_gate_result.json'));
fprintf('[71] running: %s -m dataset_v3.run_pipeline ...\n', PY);
[st, out] = system(cmd);
disp(out);
if st ~= 0
    error('GATE:71:pipeline_failed — Python 管线非零退出（exit=%d）', st);
end

% ---- 机器可读结果判定 ----
resPath = fullfile(ROOT, 'results', 'dataset_v3', 'pilot', 'pilot_gate_result.json');
res = jsondecode(fileread(resPath));
if ~isfield(res, 'overall') || ~res.overall
    error('GATE:71:pilot_gate_fail — pilot_gate_result.json overall=false（详见 checks）');
end

fprintf('[71] ===== PILOT GATE PASS =====\n');
fprintf('[71] stage=%s voyages=%d windows=%d (train=%d val=%d test=%d)\n', ...
    res.stage, res.n_voyages, res.n_windows, ...
    res.window_counts.train, res.window_counts.val, res.window_counts.test);
fprintf('[71] 结果标 PILOT：仅证明管线正确性（FR-489），统计不晋升 Dataset v3 科学基线\n');
fprintf('[71] pilot_gate_result.json: %s\n', resPath);
