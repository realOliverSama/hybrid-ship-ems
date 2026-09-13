function worker_provision(root, mdl)
% fleet_v3.worker_provision - per-worker base-workspace provision and
% model load for parallel family tasks (T-018 Phase 4-D; PCT path).
%
%   fleet_v3.worker_provision(root, mdl)
%
% Every pool worker is a separate process with its own base workspace
% and its own Simulink model instance, so each task provisions once at
% task start.  Idempotent: re-running the frozen config scripts into the
% base workspace and load_system on an already-loaded model are no-ops
% in effect (the scripts re-write the same values; load_system returns
% the loaded model).  The frozen files themselves are never modified.
%
% Error ids: GATE:FLEETV3:PROV:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths.

    if ~(ischar(root) || isstring(root)) || ~(ischar(mdl) || isstring(mdl))
        error('GATE:FLEETV3:PROV:ARG', 'root and mdl must be char paths');
    end
    cfg = fullfile(char(root), 'config');
    for s = {'ModelE_Physics_Parameters.m', 'ModelE_Control_Parameters.m', ...
             'ModelE_TestScenarios.m'}
        f = fullfile(cfg, s{1});
        if ~exist(f, 'file')
            error('GATE:FLEETV3:PROV:IO', 'frozen config not found: %s', f);
        end
        evalin('base', sprintf('run(''%s'')', f));
    end
    m = fullfile(char(root), 'model', [char(mdl) '.slx']);
    if ~exist(m, 'file')
        error('GATE:FLEETV3:PROV:IO', 'Model E not found: %s', m);
    end
    load_system(m);
end
