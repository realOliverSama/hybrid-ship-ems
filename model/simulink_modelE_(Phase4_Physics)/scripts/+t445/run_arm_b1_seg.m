function [ds, meta] = run_arm_b1_seg(SCN, ehat_tbl, varargin)
% t445.run_arm_b1_seg — B1 分段 SimState（同一 ehat ZOH 表，供 T-010 等价 Gate）
p = inputParser;
p.addParameter('StopTime', [], @(x) isempty(x) || isnumeric(x));
p.addParameter('StartT0', 0, @isnumeric);
p.addParameter('InitialOp', [], @(x) true);
p.parse(varargin{:});
opt = p.Results;
ROOT = t445.modele_root();
cfg = phase4f_t445_config();
MDL = 'HybridShip_ModelE';
if ~bdIsLoaded(MDL)
    load_system(fullfile(ROOT, 'model', [MDL '.slx']));
end
stopT = opt.StopTime;
if isempty(stopT), stopT = 60; end
t445.inject_scenario(MDL, SCN, 6, stopT);
assignin('base', 'ECMS_K_S', cfg.lambda_law.K_S_B1);
dt = cfg.oracle_window.refresh_s;
t0s = (opt.StartT0:dt:(stopT - dt)).';
op = opt.InitialOp;
ds = [];
for i = 1:numel(t0s)
    t0 = t0s(i);
    t_stop = t0 + dt;
    row = ehat_tbl(abs(ehat_tbl(:,1) - t0) < 1e-9, :);
    if isempty(row)
        row = ehat_tbl(1, :);
        row(1) = t0;
    end
    assignin('base', 'ECMS_EHat_Data', row);
    in = Simulink.SimulationInput(MDL);
    in = t445.make_simin_zoh(in);
    in = in.setModelParameter('StopTime', sprintf('%.4f', t_stop));
    in = in.setModelParameter('SaveFinalState', 'on');
    in = in.setModelParameter('SaveOperatingPoint', 'on');
    in = in.setModelParameter('FinalStateName', 'xFinal');
    if ~isempty(op)
        in = in.setInitialState(op);
    end
    out = sim(in);
    op = out.xFinal;
    dseg = model.extract_logsout(out);
    ds = local_append(ds, dseg, t0);
end
meta = struct('n_segments', numel(t0s), 'op', op);
end

function ds = local_append(ds, dseg, t0)
if isempty(ds)
    ds = dseg;
    return;
end
fn = fieldnames(dseg);
mask = dseg.t > t0 + 1e-12;
for i = 1:numel(fn)
    nm = fn{i};
    v = dseg.(nm);
    if isnumeric(v) && numel(v) == numel(dseg.t)
        ds.(nm) = [ds.(nm); v(mask)];
    end
end
end
