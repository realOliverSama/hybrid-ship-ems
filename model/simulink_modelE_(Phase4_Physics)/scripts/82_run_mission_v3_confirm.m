%% ========================================================================
%  82_run_mission_v3_confirm.m - Phase 4-C T429_CONFIRMATION_V2 thin
%  entry: fresh confirmation corpus generation executing the frozen
%  T429-AMENDMENT-V2 preregistration (results/mission_v3/
%  t429_amendment_v2.json, self_hash 796a8b07..., frozen before
%  generation): 144 voyages x 2 passes = 288 simulations over three
%  conditional-sampling layers (coverage / DEPTH / TRAFFIC).  Orchestration
%  body lives in +mission_v3/producer_confirm.m (numbered files cannot be
%  called as functions - project convention, same as the 70/71/72/75/80
%  thin entries; eval(fileread) context).
%
%  The run is machine-verified against the frozen authority chain at
%  start (pinned amendment self_hash, amendment-declared registry hashes,
%  phase1_freeze + D6 + sprint + t429_freeze_v1 chain, T-016 stack
%  capture non-generator components) and ends with the completeness
%  verdict:
%    - any planned slot without an accepted draw, any FAILED_GENERATION
%      voyage, or a 144-voyage count mismatch raises
%      GATE:MISSION_V3:CONF:INCOMPLETE AFTER the audit and manifest JSONs
%      are written (controlled FAIL by design);
%    - two-run payload hash mismatch raises GATE:MISSION_V3:CONF:TWO_RUN;
%    - a conditional class exceeding 200 candidate attempts raises
%      GATE:MISSION_V3:CONF:ACCEPT_RATE (delegation spec Escalate If);
%    - a cruise-event STEADY_FOLLOW plateau < 180 s raises
%      GATE:MISSION_V3:CONF:FOLLOW_HOST (amended env implementation not
%      in effect - controlled stop).
%
%  Usage:
%    cd model/simulink_modelE_(Phase4_Physics)
%    eval(fileread('scripts/82_run_mission_v3_confirm.m'))
%
%  Optional base-variable switch (set BEFORE eval(fileread)):
%    T429CONF_MODE = 'pilot' | 'run' | 'tworun'   (default 'tworun')
%      pilot  : one draw per layer (coverage first stratum x 1 draw with
%               the full 2x2 family, depth low-band x 1 GOOD_ON, traffic
%               x 1 GOOD_ON+GOOD_OFF = 7 sims) into
%               datasets_v3/mission_v3/t429conf_v2_pilot/ (chain smoke
%               test and wall-time extrapolation; never counted as
%               formal generation)
%      run    : one formal pass -> datasets_v3/mission_v3/t429conf_v2/
%      tworun : two formal passes (t429conf_v2/ + t429conf_v2_rerun/)
%               + 144-pair payload hash comparison + final
%               results/mission_v3/t429conf_v2_manifest.json
%
%  Exit semantics: controlled GATE errors terminate with a nonzero exit
%  status; audit products are always written first.
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

mode_ = 'tworun';
if evalin('base', 'exist(''T429CONF_MODE'', ''var'')'), mode_ = char(T429CONF_MODE); end

RES_DIR = fullfile(ROOT, 'results', 'mission_v3');
MAN_P = fullfile(RES_DIR, 't429conf_v2_manifest.json');
DS_DIR = fullfile(ROOT, 'datasets_v3', 'mission_v3');
AUDIT_1 = fullfile(DS_DIR, 't429conf_v2', '_run_audit.json');
AUDIT_2 = fullfile(DS_DIR, 't429conf_v2_rerun', '_run_audit.json');
LED_1 = fullfile(DS_DIR, 't429conf_v2', '_run_ledger.json');
LED_2 = fullfile(DS_DIR, 't429conf_v2_rerun', '_run_ledger.json');

switch mode_
    case 'pilot'
        am = jsondecode(fileread(fullfile(RES_DIR, ...
            't429_amendment_v2.json')));
        first_cov = '';
        alloc = am.confirmation_plan_exact.draw_allocation;
        for i = 1:numel(alloc)
            if ~strncmp(char(alloc(i).stratum), 'STRAT_WORK_MISSION', 18)
                first_cov = char(alloc(i).stratum);
                break;
            end
        end
        if isempty(first_cov)
            error('GATE:MISSION_V3:CONF:ARG', ...
                'pilot could not resolve the first coverage stratum');
        end
        pilot_dir = fullfile(DS_DIR, 't429conf_v2_pilot');
        if ~exist(pilot_dir, 'dir'), mkdir(pilot_dir); end
        t_all = tic;

        t0 = tic;
        rc = mission_v3.producer_confirm('layer', 'Layer', 'coverage', ...
            'RunLabel', 't429conf_v2_pilot', 'StratumFilter', {first_cov}, ...
            'DrawsOverride', 1, 'AuditJsonPath', ...
            fullfile(pilot_dir, '_audit_coverage.json'), ...
            'ManifestJsonPath', ...
            fullfile(pilot_dir, '_manifest_coverage.json'), ...
            'VerdictError', false);
        fprintf('[T429CONF] PILOT coverage done in %.1f s; verdict=%s\n', ...
            toc(t0), rc.verdict);

        t0 = tic;
        rd = mission_v3.producer_confirm('layer', 'Layer', 'depth', ...
            'RunLabel', 't429conf_v2_pilot', 'DrawsOverride', 1, ...
            'AuditJsonPath', fullfile(pilot_dir, '_audit_depth.json'), ...
            'ManifestJsonPath', ...
            fullfile(pilot_dir, '_manifest_depth.json'), ...
            'VerdictError', false);
        fprintf('[T429CONF] PILOT depth done in %.1f s; verdict=%s\n', ...
            toc(t0), rd.verdict);

        t0 = tic;
        rt = mission_v3.producer_confirm('layer', 'Layer', 'traffic', ...
            'RunLabel', 't429conf_v2_pilot', 'DrawsOverride', 1, ...
            'AuditJsonPath', fullfile(pilot_dir, '_audit_traffic.json'), ...
            'ManifestJsonPath', ...
            fullfile(pilot_dir, '_manifest_traffic.json'), ...
            'VerdictError', false);
        fprintf('[T429CONF] PILOT traffic done in %.1f s; verdict=%s\n', ...
            toc(t0), rt.verdict);

        wall_pilot = toc(t_all);
        n_sims = numel(rc.manifest.voyages) + numel(rd.manifest.voyages) ...
            + numel(rt.manifest.voyages);
        % 288 = the FULL tworun simulation total (144 voyages x 2 passes)
        est_tworun_h = wall_pilot / max(1, n_sims) * 288 / 3600;
        fprintf(['[T429CONF] PILOT total %.1f s wall for %d sims; ', ...
            'extrapolated full tworun (288 sims x 2 passes) ~= %.2f h ', ...
            '(escalation budget 6 h)\n'], wall_pilot, n_sims, ...
            est_tworun_h);
        if est_tworun_h > 6
            error(['GATE:MISSION_V3:CONF:BUDGET', ...
                ' pilot-extrapolated tworun wall %.2f h exceeds the 6 h ', ...
                'delegation budget (stop and report; no run started)'], ...
                est_tworun_h);
        end
    case 'run'
        t0 = tic;
        res = mission_v3.producer_confirm('run', 'RunLabel', ...
            't429conf_v2', 'AuditJsonPath', AUDIT_1, 'ManifestJsonPath', ...
            MAN_P);
        fprintf('[T429CONF] RUN done in %.1f s wall; verdict=%s\n', ...
            toc(t0), res.verdict);
    case 'tworun'
        t0 = tic;
        res1 = mission_v3.producer_confirm('run', 'RunLabel', ...
            't429conf_v2', 'AuditJsonPath', AUDIT_1, 'ManifestJsonPath', ...
            MAN_P, 'VerdictError', false);
        fprintf(['[T429CONF] RUN 1/2 done in %.1f s wall; verdict=%s ', ...
            '(completeness error deferred to post-comparison)\n'], ...
            toc(t0), res1.verdict);
        t1 = tic;
        res2 = mission_v3.producer_confirm('run', 'RunLabel', ...
            't429conf_v2_rerun', 'AuditJsonPath', AUDIT_2, ...
            'ManifestJsonPath', MAN_P, 'VerdictError', false);
        fprintf('[T429CONF] RUN 2/2 done in %.1f s wall; verdict=%s\n', ...
            toc(t1), res2.verdict);
        t2 = tic;
        cmp = mission_v3.producer_confirm('compare', LED_1, LED_2, ...
            AUDIT_2, MAN_P, MAN_P, AUDIT_2);
        fprintf(['[T429CONF] COMPARE done in %.1f s: %d payload hash ', ...
            'pairs consistent=%d\n'], toc(t2), cmp.voyages_compared, ...
            cmp.consistent);
        fprintf('[T429CONF] TWORUN total %.1f s wall; manifest=%s\n', ...
            toc(t0), MAN_P);
    otherwise
        error('GATE:MISSION_V3:CONF:ARG', ...
            'unknown T429CONF_MODE: %s (pilot|run|tworun)', mode_);
end
