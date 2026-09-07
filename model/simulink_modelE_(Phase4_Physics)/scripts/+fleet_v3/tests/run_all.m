function overall = run_all(varargin)
% fleet_v3.tests.run_all - T-018 Phase 4-D unit-test entry for the
% +fleet_v3 package (Stage A / smoke generation kit).
%
%   run_all()            'run' mode (default): five suites
%   overall = run_all()  true iff everything passed
%
% Suites (each executed via run(...); suite scripts are never modified):
%   test_plan_load               activation parse + bands + samplers +
%                                segments + provenance hash stability
%   test_predicate_eval          canonical token predicate pos/neg +
%                                whitelist + grammar guards
%   test_ood_judge               A1/A2/B1 band boundaries + guard bands +
%                                STRESS removal + token priority + 700 kW
%   test_propulsion_feasibility  monotonicity + rating + clamp warning +
%                                argument guards
%   test_export_ood              FORMAL bridge + OOD meta rewrite
%                                round-trip + untouched exporter face
%   test_ood_plan_face           amendment-01 OOD floor 1.3 + default
%                                face unchanged + UKC clamp
%
% Usage from the Model E workspace root (thin-entry convention):
%   matlab -batch "run('scripts/+fleet_v3/tests/run_all.m')"
% or function-style after addpath('scripts/+fleet_v3/tests').
%
% Failure semantics: a suite failure is caught and recorded; the
% remaining suites still execute (audit completeness).  At the end, any
% failure raises GATE:FLEETV3:TESTS.
%
% Discipline: ASCII-only source; no absolute paths; body locals are
% rr_-prefixed: the suites are executed via run(...) and their script
% variables land in this workspace under the ra_ prefix - the runner
% prefix prevents any clobbering of the runner state (run_all.m FD6
% precedent of the C-side mission_v3 tests).

    rr_suites = {'test_plan_load', 'test_predicate_eval', ...
        'test_ood_judge', 'test_propulsion_feasibility', ...
        'test_export_ood', 'test_ood_plan_face'};
    % resolve the package parents: tests/ -> +fleet_v3 -> scripts (the
    % packages fleet_v3 / mission_v3 live under scripts/)
    addpath(fileparts(fileparts(fileparts(mfilename('fullpath')))));
    rr_results = struct([]);
    for rr_i = 1:numel(rr_suites)
        rr_name = rr_suites{rr_i};
        rr_ok = true;
        rr_err = '';
        try
            run(fullfile(fileparts(mfilename('fullpath')), ...
                [rr_name '.m']));
        catch rr_ME
            rr_ok = false;
            rr_err = char(rr_ME.message);
        end
        rr_row = struct('suite', rr_name, 'passed', rr_ok, ...
            'error', rr_err);
        if isempty(rr_results)
            rr_results = rr_row;
        else
            rr_results(end+1) = rr_row; %#ok<AGROW>
        end
        if rr_ok
            rr_mark = 'PASS';
        else
            rr_mark = 'FAIL';
        end
        fprintf('  suite %s: %s\n', rr_name, rr_mark);
    end

    rr_n_fail = 0;
    for rr_i = 1:numel(rr_results)
        if ~rr_results(rr_i).passed
            rr_n_fail = rr_n_fail + 1;
            fprintf('  FAILED %s: %s\n', rr_results(rr_i).suite, ...
                rr_results(rr_i).error);
        end
    end
    overall = (rr_n_fail == 0);
    if ~overall
        error('GATE:FLEETV3:TESTS', ...
            '%d of %d fleet_v3 suite(s) failed', rr_n_fail, ...
            numel(rr_results));
    end
    fprintf('fleet_v3.tests.run_all: ALL PASS (%d suites)\n', ...
        numel(rr_suites));
end
