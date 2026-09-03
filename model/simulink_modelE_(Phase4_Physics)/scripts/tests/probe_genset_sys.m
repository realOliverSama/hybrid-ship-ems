% probe_genset_sys.m — 在子子系统内复刻 buildGensetSystemE 序列定位 add_line 失败点
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'), fullfile(ROOT, 'config'));
mdl = 'probe_gs_tmp';
if bdIsLoaded(mdl), close_system(mdl, 0); end
new_system(mdl); load_system(mdl);
sys = [mdl '/06_Genset_System'];
add_block('simulink/Ports & Subsystems/Subsystem', sys, 'Position', [100 100 400 300]);
delete_block([sys '/In1']); delete_block([sys '/Out1']);
ins = {'DG1_cmd_start','DG1_P_ref_kW','DG2_cmd_start','DG2_P_ref_kW','DG_cmd_stop'};
for k = 1:numel(ins)
    add_block('simulink/Sources/In1', [sys '/' ins{k}], 'Position', [15 30+55*(k-1) 45 60+55*(k-1)]);
end
outs = {'P_DG1_kW','state_DG1','below_min_DG1','start_count_DG1', ...
    'P_DG2_kW','state_DG2','below_min_DG2','start_count_DG2','P_gen_actual_kW'};
for k = 1:numel(outs)
    add_block('simulink/Sinks/Out1', [sys '/' outs{k}], 'Position', [640 20+50*(k-1) 670 50+50*(k-1)]);
end
add_block('simulink/Signal Routing/Demux', [sys '/stop_demux'], 'Position', [90 240 100 280], 'Outputs', '2');
add_block('simulink/Sources/Constant', [sys '/cmd_stop_default'], 'Position', [15 248 55 272], 'Value', '[0;0]');
add_line(sys, 'DG_cmd_stop/1', 'stop_demux/1');
unitPos = {[150 30 310 140], [150 170 310 280]};
for u = 1:2
    tag = num2str(u);
    fn = [sys '/GensetUnitE_DG' tag];
    add_block('simulink/User-Defined Functions/MATLAB Function', fn, 'Position', unitPos{u});
    try
        setMFCode2(fn, model.code_genset_unit_e(), {'DG_P_rated_kW','DG_P_min_kW','DG_ramp_kW_per_s','DG_tau_s', ...
            'N_ticks_starting','N_ticks_sync','N_ticks_cooldown','Pgen_init'});
        ph = get_param(fn, 'PortHandles');
        fprintf('DG%s block: in=%d out=%d\n', tag, numel(ph.Inport), numel(ph.Outport));
    catch e
        fprintf('DG%s setMFCode error: %s\n', tag, e.message);
    end
    try
        phU = get_param(fn, 'PortHandles');
        phS = get_param([sys '/DG' tag '_cmd_start'], 'PortHandles');
        add_line(sys, phS.Outport(1), phU.Inport(1));
        add_line(sys, ['stop_demux/' num2str(u)], [fn '/2']);
        add_line(sys, ['DG' tag '_P_ref_kW/1'], [fn '/3']);
        add_line(sys, [fn '/1'], ['P_DG' tag '_kW/1']);
        add_line(sys, [fn '/2'], ['state_DG' tag '/1']);
        add_line(sys, [fn '/3'], ['below_min_DG' tag '/1']);
        add_line(sys, [fn '/4'], ['start_count_DG' tag '/1']);
        fprintf('DG%s wiring OK\n', tag);
    catch e
        fprintf('DG%s wiring FAIL: %s\n', tag, e.message);
        % 打印块实际端口状态
        ph = get_param(fn, 'PortHandles');
        fprintf('  now: in=%d out=%d\n', numel(ph.Inport), numel(ph.Outport));
        rt = sfroot; ch = rt.find('-isa', 'Stateflow.EMChart', 'Path', fn);
        fprintf('  EMChart found: %d\n', numel(ch));
    end
end
close_system(mdl, 0);

function setMFCode2(blkPath, code, paramNames)
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