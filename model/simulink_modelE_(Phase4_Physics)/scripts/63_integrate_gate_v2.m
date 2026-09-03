%% ========================================================================
%  63_integrate_gate_v2.m — T-310：law7-v2 门控集成（G0~G3 chart 变体 + ECMS_NN_* v2 注入）
%  输入：results/ecms_phase25/deploy/selected_model_weights_v2.json（T-310 导出）
%        results/ecms_phase25/gating/eml_ecms_step_mode{0..3}.m（EML 变体，gen_eml_gate.py）
%  产物：config/ecms_nn_weights_v2.mat（不入库）+ 模型 chart Script 按 GATE_MODE 应用（save_system）
%  用法：matlab -batch "GATE_MODE=0; eval(fileread('scripts/63_integrate_gate_v2.m'))"
%  ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'), ...
    fullfile(ROOT, 'scripts', '+ecms25'), fullfile(ROOT, 'python', 'export'));

if ~exist('GATE_MODE', 'var'), GATE_MODE = 0; end
assert(GATE_MODE >= 0 && GATE_MODE <= 3 && mod(GATE_MODE, 1) == 0, 'ECMS25:GATE', 'GATE_MODE 非法');

depPath = fullfile(ROOT, 'results', 'ecms_phase25', 'deploy', 'selected_model_weights_v2.json');
emlPath = fullfile(ROOT, 'results', 'ecms_phase25', 'gating', sprintf('eml_ecms_step_mode%d.m', GATE_MODE));
assert(exist(depPath, 'file') == 2, 'ECMS25:GATE', '缺少部署 JSON: %s', depPath);
assert(exist(emlPath, 'file') == 2, 'ECMS25:GATE', '缺少 EML 变体: %s', emlPath);
DJ = jsondecode(fileread(depPath));

%% ---- ECMS_NN_* v2（base workspace + config mat）----
ECMS_NN_W1 = DJ.weights.W1;      % [302,64]
ECMS_NN_b1 = DJ.weights.b1(:).'; % [1,64]
ECMS_NN_W2 = DJ.weights.W2;      % [64,32]
ECMS_NN_b2 = DJ.weights.b2(:).'; % [1,32]
ECMS_NN_W3 = DJ.weights.W3;      % [32,3]（q10,q50,q90）
ECMS_NN_b3 = DJ.weights.b3(:).'; % [1,3]
ECMS_NN_XMean = DJ.norm.X_mean(:);
ECMS_NN_XStd  = DJ.norm.X_std(:);
ECMS_NN_YMean = DJ.norm.y_mean;
ECMS_NN_YStd  = DJ.norm.y_std;
assert(isequal(size(ECMS_NN_W3), [32 3]), 'ECMS25:GATE', 'W3 形状异常');
assert(numel(ECMS_NN_XMean) == 302, 'ECMS25:GATE', 'XMean 长度异常');
assert(all(ECMS_NN_XStd > 0), 'ECMS25:GATE', 'XStd 含非正元素');
% S20 标幺常数（与 law=6/ECMS_NN v1 同源，Phase 1 kS_calibration.json）
kJson = fullfile(ROOT, 'results', 'ecms_phase1', 'kS_calibration.json');
KJ = jsondecode(fileread(kJson));
ECMS_NN_Ebar_H_kWh  = KJ.oracle.E_bar_H_kWh;
ECMS_NN_Escale_kWh  = KJ.oracle.E_scale_kWh;
ECMS_NN_WeightsOK = 1;

%% ---- chart 应用（模式字面量内嵌于 EML 文件）----
addpath('model');
load_system(fullfile(ROOT, 'model', 'HybridShip_ModelD_DCBus.slx'));
rt = sfroot;
ch = rt.find('-isa', 'Stateflow.EMChart');
target = [];
for i = 1:numel(ch)
    try
        if contains(ch(i).Script, 'ECMS_NN') && contains(ch(i).Path, 'EMS_NN_AECMS')
            target = ch(i); break;
        end
    catch
    end
end
assert(~isempty(target), 'ECMS25:GATE', '未找到 law7 EML chart（EMS_NN_AECMS/ecms_step）');
newCode = fileread(emlPath);
target.Script = newCode;
save_system('HybridShip_ModelD_DCBus');

%% ---- 写 v2 mat（worker 注入源）----
matOut = fullfile(ROOT, 'config', 'ecms_nn_weights_v2.mat');
save(matOut, 'ECMS_NN_W1', 'ECMS_NN_b1', 'ECMS_NN_W2', 'ECMS_NN_b2', ...
    'ECMS_NN_W3', 'ECMS_NN_b3', 'ECMS_NN_XMean', 'ECMS_NN_XStd', ...
    'ECMS_NN_YMean', 'ECMS_NN_YStd', 'ECMS_NN_Ebar_H_kWh', 'ECMS_NN_Escale_kWh', ...
    'ECMS_NN_WeightsOK');
fprintf('[63] GATE_MODE=%d chart 已应用并保存；mat -> %s\n', GATE_MODE, matOut);
fprintf('[63] W3 %dx%d | S1=%.3f S2=%.3f kWh（内嵌于 chart）\n', ...
    size(ECMS_NN_W3,1), size(ECMS_NN_W3,2), DJ.gate.S1_kwh, DJ.gate.S2_kwh);
