% probe_genset_block.m — 最小复现：GensetUnitE 块 setMFCode 后端口是否物化
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'), fullfile(ROOT, 'config'));
mdl = 'probe_genset_tmp'; E = ROOT;
if bdIsLoaded(mdl), close_system(mdl, 0); end
new_system(mdl); load_system(mdl);
fn = [mdl '/GensetUnitE_test'];
add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', [100 100 300 200]);
try
    setMFCode(fn, model.code_genset_unit_e(), {'DG_P_rated_kW','DG_P_min_kW','DG_ramp_kW_per_s','DG_tau_s', ...
        'N_ticks_starting','N_ticks_sync','N_ticks_cooldown','Pgen_init'});
catch e
    fprintf('setMFCode error: %s\n', e.message);
end
ph = get_param(fn, 'PortHandles');
fprintf('inports=%d outports=%d\n', numel(ph.Inport), numel(ph.Outport));
% 对照: propeller 块
fn2 = [mdl '/Prop_test'];
add_block('simulink/User-Defined Functions/MATLAB Function', fn2, 'Position', [100 300 300 400]);
try
    setMFCode(fn2, model.code_propeller_e(), {'K_Fw','K_Fv','K_Qw','K_Qv'});
catch e
    fprintf('setMFCode error (prop): %s\n', e.message);
end
ph2 = get_param(fn2, 'PortHandles');
fprintf('prop: inports=%d outports=%d\n', numel(ph2.Inport), numel(ph2.Outport));
close_system(mdl, 0);

function setMFCode(blkPath, code, paramNames, paramSizes, twoPass)
    if nargin < 3, paramNames = {}; end
    if nargin < 4, paramSizes = {}; end
    if nargin < 5, twoPass = false; end
    rt = sfroot;
    ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', blkPath);
    if numel(ch) ~= 1, error('MATLAB Function 块未找到: %s', blkPath); end
    ch.Script = code;
    ds = ch.find('-isa', 'Stateflow.Data');
    for i = 1:numel(paramNames)
        d = [];
        for j = 1:numel(ds)
            if strcmp(ds(j).Name, paramNames{i}), d = ds(j); break; end
        end
        if isempty(d), d = Stateflow.Data(ch); d.Name = paramNames{i}; end
        d.Scope = 'Parameter';
    end
end