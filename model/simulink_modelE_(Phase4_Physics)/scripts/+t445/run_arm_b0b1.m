function out = run_arm_b0b1(arm, SCN, varargin)
% t445.run_arm_b0b1 — B0 (law=5) / B1 (law=6 scalar oracle) 单次全程 sim
%
% arm: 'B0' | 'B1'
% SCN: ModelE_TestScenarios 工况结构体
% Name-value: StopTime, EHatData (N×3, B1), K_S (B1 default 0.01)
%
% B1 经 t445.make_simin_zoh 覆盖 Interpolate=off；不落盘 .slx。
p = inputParser;
p.addParameter('StopTime', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('EHatData', [], @isnumeric);
p.addParameter('K_S', 0.01, @isnumeric);
p.parse(varargin{:});
opt = p.Results;

ROOT = t445.modele_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
if ~evalin('base', "exist('ECMS_Lambda', 'var')")
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m')));
end

MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end

if strcmp(arm, 'B0')
    mode = 5;
elseif strcmp(arm, 'B1')
    mode = 6;
else
    error('T445:ARM', 'arm must be B0 or B1, got %s', arm);
end

stopT = opt.StopTime;
if isempty(stopT)
    if isfield(SCN, 'stop_time_s') && ~isempty(SCN.stop_time_s)
        stopT = SCN.stop_time_s;
    else
        stopT = 4000;
    end
end

t445.inject_scenario(MDL, SCN, mode, stopT);
if strcmp(arm, 'B1')
    if isempty(opt.EHatData)
        opt.EHatData = evalin('base', 'ECMS_EHat_Data');
    end
    assignin('base', 'ECMS_EHat_Data', opt.EHatData);
    assignin('base', 'ECMS_K_S', opt.K_S);
    in = Simulink.SimulationInput(MDL);
    in = t445.make_simin_zoh(in);
    in = in.setModelParameter('StopTime', get_param(MDL, 'StopTime'));
    out = sim(in);
else
    assignin('base', 'ECMS_K_S', 0);
    out = sim(MDL);
end
end
