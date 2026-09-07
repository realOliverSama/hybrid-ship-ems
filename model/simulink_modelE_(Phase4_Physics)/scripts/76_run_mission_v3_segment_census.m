%% ========================================================================
%  76_run_mission_v3_segment_census.m - Phase 4-C T-004 thin entry:
%  READ-ONLY segment duration census -> T-429 slice geometry freeze
%  PROPOSAL (DGN-12 R8; FR-547). Orchestration body lives in
%  +mission_v3/+audits/segment_duration_census.m (numbered files cannot
%  be called as functions - project convention, same as 70..75 thin
%  entries; eval(fileread) context).
%
%  READ-ONLY DISCIPLINE: pure registry arithmetic - no Model E simulation,
%  no T-429 metric execution, no registry/config/.specs edits, no git
%  operations. All proposed geometry values are proposals for the main
%  agent; the freeze happens via T-007.
%
%  Usage:
%    cd model/simulink_modelE_(Phase4_Physics)
%    eval(fileread('scripts/76_run_mission_v3_segment_census.m'))
%
%  Optional base-variable switches (set BEFORE eval(fileread)):
%    T004_OUT            - alternative output JSON path (char)
%    T004_SPEED_OVERRIDE - struct role -> m/s (sensitivity rerun; default
%                          basename gains _sensitivity suffix)
%    T004_GEOM_OVERRIDE  - struct subset of slice_duration_s /
%                          edge_buffer_s / stride_s (sensitivity rerun)
%
%  Exit semantics: candidate-geometry violations are EVIDENCE and an
%  escalation item for the main agent, not a script failure - this entry
%  still exits 0. Registry drift / missing inputs fail closed via
%  error ids T004:G*.
%
%  NOTE: no Model E config provisioning here - the census is registry-only
%  arithmetic (conditional_dag_config.json supplies the stretch lower
%  bound), so config/*.m files are deliberately not run.
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

callArgs = {};
if evalin('base', 'exist(''T004_OUT'', ''var'')')
    callArgs = [callArgs, {'OutPath', char(string(evalin('base', 'T004_OUT')))}]; %#ok<AGROW>
end
if evalin('base', 'exist(''T004_SPEED_OVERRIDE'', ''var'')')
    callArgs = [callArgs, {'SpeedOverride', evalin('base', 'T004_SPEED_OVERRIDE')}]; %#ok<AGROW>
end
if evalin('base', 'exist(''T004_GEOM_OVERRIDE'', ''var'')')
    callArgs = [callArgs, {'GeomOverride', evalin('base', 'T004_GEOM_OVERRIDE')}]; %#ok<AGROW>
end

doc = mission_v3.audits.segment_duration_census(callArgs{:});

fprintf('[76] task=%s candidate_geometry=%g/%g/%g overall=%s\n', ...
    doc.task_id, ...
    doc.proposal.candidate_geometry.slice_duration_s, ...
    doc.proposal.candidate_geometry.edge_buffer_s, ...
    doc.proposal.candidate_geometry.stride_s, ...
    doc.proposal.overall_verdict);
fprintf('[76] JSON: see [T004] lines above\n');
