%% ========================================================================
%  75_run_mission_v3_feasibility_sweep.m - Phase 4-C T-005 thin entry:
%  read-only joint feasibility matrix sweep (DGN-05 R2/R3 pre-freeze
%  evidence for the mission-v3 ID support bound candidate envelope).
%  Orchestration body lives in +mission_v3/audits/feasibility_sweep.m
%  (numbered files cannot be called as functions - project convention,
%  same as 70/71/72 thin entries; eval(fileread) context).
%
%  READ-ONLY DISCIPLINE: no Model E file is modified. Scenarios are runtime
%  SCN structs injected through existing base-workspace channels
%  (gate_e.reset_bench_nominal + model.run_scenario_sim). The sweep never
%  edits support bounds, never generates formal voyages, never writes
%  datasets_v3/.
%
%  Usage:
%    cd model/simulink_modelE_(Phase4_Physics)
%    eval(fileread('scripts/75_run_mission_v3_feasibility_sweep.m'))
%
%  Optional base-variable switches (set BEFORE eval(fileread)):
%    T005_MODE = 'pilot' | 'degraded' | 'full'   (default 'full')
%    T005_OUT  = alternative output JSON path (default results/mission_v3/)
%
%  Direct package invocation for pilot/resume/degraded runs:
%    res = mission_v3.audits.feasibility_sweep('Mode','pilot');
%
%  Exit semantics: interior-infeasible combinations are EVIDENCE, not a
%  failure - they are listed and this entry still exits 0 (bound revision
%  is a main-agent amendment decision; spec forbids editing bounds here).
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));

mode_ = 'full';
if evalin('base', 'exist(''T005_MODE'', ''var'')'), mode_ = T005_MODE; end
if evalin('base', 'exist(''T005_OUT'', ''var'')')
    res = mission_v3.audits.feasibility_sweep('Mode', string(mode_), 'OutPath', string(T005_OUT));
else
    res = mission_v3.audits.feasibility_sweep('Mode', string(mode_));
end

fprintf('[75] mode=%s points=%d feasible=%d wall=%.0f s\n', ...
    mode_, res.doc.summary.n_points, res.doc.summary.n_feasible, res.wall_total_s);
if res.doc.summary.candidate_interior_infeasible_n > 0
    fprintf(['[75] NOTE: %d in-candidate-bound infeasible combination(s) - ' ...
        'recorded as evidence for the main agent (no bound editing here)\n'], ...
        res.doc.summary.candidate_interior_infeasible_n);
end
fprintf('[75] JSON: %s\n', res.out_path);
