%% ========================================================================
%  80_run_mission_v3_prequal.m - Phase 4-C T-020 thin entry: execution of
%  the T-019 frozen MISSION_V3_PREQUALIFICATION plan (9 strata x 3 base
%  draws x 2x2 family = 108 member voyages, nominal; two-run determinism).
%  Orchestration body lives in +mission_v3/producer_prequal.m (numbered
%  files cannot be called as functions - project convention, same as the
%  70/71/72/75 thin entries; eval(fileread) context).
%
%  The run is machine-verified against the frozen chain at start
%  (plan registries self hashes + T-019 qualification stack identity) and
%  ends with the frozen rejection-threshold verdict:
%    - any stratum rejection rate > 0.50 or overall > 0.50 raises
%      GATE:MISSION_V3:PREQUAL:REJECTION AFTER the audit and manifest
%      JSONs are written (controlled FAIL by design, FR-539);
%    - two-run payload hash mismatch raises GATE:MISSION_V3:PREQUAL:TWO_RUN.
%
%  Usage:
%    cd model/simulink_modelE_(Phase4_Physics)
%    eval(fileread('scripts/80_run_mission_v3_prequal.m'))
%
%  Optional base-variable switch (set BEFORE eval(fileread)):
%    T020_MODE = 'pilot' | 'run' | 'tworun'   (default 'tworun')
%      pilot  : single stratum, single base draw, 4 members, output to
%               datasets_v3/mission_v3/prequal_pilot/ (chain smoke test
%               and wall-time measurement; FR-500: never counted as
%               formal generation)
%      run    : one formal pass -> datasets_v3/mission_v3/prequal/
%      tworun : two formal passes (prequal/ + prequal_rerun/) + AC-438
%               payload hash comparison + final audit/manifest
%
%  Exit semantics: controlled GATE errors terminate with a nonzero exit
%  status; audit products are always written first.
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

mode_ = 'tworun';
if evalin('base', 'exist(''T020_MODE'', ''var'')'), mode_ = char(T020_MODE); end

RES_DIR = fullfile(ROOT, 'results', 'mission_v3');
AUDIT_P = fullfile(RES_DIR, 'prequal_strata_audit.json');
MAN_P = fullfile(RES_DIR, 'prequal_manifest.json');
LED_1 = fullfile(ROOT, 'datasets_v3', 'mission_v3', 'prequal', ...
    '_run_ledger.json');
LED_2 = fullfile(ROOT, 'datasets_v3', 'mission_v3', 'prequal_rerun', ...
    '_run_ledger.json');

switch mode_
    case 'pilot'
        plan_obj = jsondecode(fileread(fullfile(ROOT, 'config', ...
            'mission_v3', 'prequal_plan.json')));
        first_stratum = plan_obj.scale.strata{1};
        t0 = tic;
        res = mission_v3.producer_prequal('run', 'RunLabel', ...
            'prequal_pilot', 'StratumFilter', {first_stratum}, ...
            'DrawsPerStratum', 1, 'AuditJsonPath', ...
            fullfile(ROOT, 'datasets_v3', 'mission_v3', 'prequal_pilot', ...
            'prequal_strata_audit.json'), 'ManifestJsonPath', ...
            fullfile(ROOT, 'datasets_v3', 'mission_v3', 'prequal_pilot', ...
            'prequal_manifest.json'), 'VerdictError', false);
        fprintf(['[T020] PILOT done in %.1f s wall; verdict=%s; ', ...
            'audit=%s\n'], toc(t0), res.verdict, res.audit_path);
    case 'run'
        t0 = tic;
        res = mission_v3.producer_prequal('run', 'RunLabel', 'prequal', ...
            'AuditJsonPath', AUDIT_P, 'ManifestJsonPath', MAN_P);
        fprintf('[T020] RUN done in %.1f s wall; verdict=%s\n', ...
            toc(t0), res.verdict);
    case 'tworun'
        t0 = tic;
        res1 = mission_v3.producer_prequal('run', 'RunLabel', ...
            'prequal', 'AuditJsonPath', AUDIT_P, 'ManifestJsonPath', ...
            MAN_P, 'VerdictError', false);
        fprintf(['[T020] RUN 1/2 done in %.1f s wall; verdict=%s ', ...
            '(verdict error deferred to post-comparison)\n'], ...
            toc(t0), res1.verdict);
        t1 = tic;
        res2 = mission_v3.producer_prequal('run', 'RunLabel', ...
            'prequal_rerun', 'AuditJsonPath', AUDIT_P, ...
            'ManifestJsonPath', MAN_P, 'VerdictError', false);
        fprintf('[T020] RUN 2/2 done in %.1f s wall; verdict=%s\n', ...
            toc(t1), res2.verdict);
        cmp_res = mission_v3.producer_prequal('compare', LED_1, LED_2, ...
            AUDIT_P, MAN_P, MAN_P, AUDIT_P);
        fprintf(['[T020] TWO-RUN consistent: %d voyages, 0 payload ', ...
            'hash mismatches\n'], cmp_res.voyages_compared);
    otherwise
        error('T020_MODE must be pilot | run | tworun (got %s)', mode_);
end
