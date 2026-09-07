%% 73_generate_fleet_v3.m - T-018 Phase 4-D fleet dataset v3 generation
% (Stage A / micro-smoke thin entry; D-side fleet qualification).
%
% FROZEN BASIS: docs/phase4/d_dataset_v3/t432_activation_record.md
% (legal authority) + t432_activation_record.json (machine copy - THIS
% script consumes the JSON only).  All thresholds, seeds, draw segments,
% bands, guards, samplers and retry/gate values are consumed from the
% machine record via fleet_v3.plan_load; nothing numeric is decided in
% this entry.
%
% Modes (FLEET_MODE below):
%   'stage_a'      Stage A qualification corpus: IID 9 strata x 2 draws
%                  (k=900..901, 4 members each) + OOD 6 conditions x 1
%                  family (k=900, 4 members each); artifacts under
%                  datasets_v3/qualification/, manifest under
%                  results/dataset_v3/qualification/.
%   'smoke'        T-018 Phase 1 micro-smoke: IID first 2 strata x 1
%                  draw + OOD 6 conditions x 1 family, all at k=950 on
%                  the stage_a seed (disjoint from every formal
%                  segment); artifacts under datasets_v3/
%                  qualification_smoke/, products disposable.
%   'stage_b_iid'  Phase 2 (NOT_IMPLEMENTED in Phase 1; the internal
%                  generators already carry the formal Stage B
%                  parameterization via the plan).
%   'stage_b_ood'  Phase 2 (NOT_IMPLEMENTED in Phase 1).
%
% Invocation (thin-entry convention; this is a SCRIPT, mfilename is
% empty under -batch, hence the pwd fallback):
%   cd <MODELE_ROOT>
%   matlab -batch "eval(fileread('scripts/73_generate_fleet_v3.m'))"
%
% Errors: every failure path raises GATE:FLEETV3:* (machine-gate
% semantics; the smoke verdicts are printed as key=value lines at the
% end of a successful run).
%
% Discipline: ASCII-only source; no absolute paths (ROOT from
% mfilename, pwd fallback); read-only consumption of +mission_v3/,
% config/mission_v3/, Model E and the activation record.

FLEET_MODE = 'smoke';   % 'stage_a' | 'smoke' | Phase 2: stage_b_*

ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;   % -batch thin-entry fallback (script run in place)
end
addpath(fullfile(ROOT, 'scripts'));
REPO_ROOT = fileparts(fileparts(ROOT));
ACTIVATION_JSON = fullfile(REPO_ROOT, 'docs', 'phase4', ...
    'd_dataset_v3', 't432_activation_record.json');

switch FLEET_MODE
    case {'stage_a', 'smoke'}
        plan = fleet_v3.plan_load(ACTIVATION_JSON, FLEET_MODE);
        ctx = fleet_v3.ctx_build(plan);
        out_iid = fleet_v3.gen_iid(plan, ctx);
        out_ood = fleet_v3.gen_ood(plan, ctx);
        res = fleet_v3.manifest_stage_a(plan, out_iid, out_ood);
        try
            close_system(ctx.MDL, 0);
        catch
            % model not loaded (nothing to close)
        end
        fprintf('FLEETV3_MODE=%s\n', FLEET_MODE);
        fprintf('FLEETV3_IID_VERDICT=%s\n', out_iid.verdict);
        fprintf('FLEETV3_IID_VOYAGES=%d ACCEPTED=%d\n', ...
            numel(out_iid.voyages), res.manifest.iid.n_accepted);
        fprintf('FLEETV3_OOD_CONDITIONS=%d VOYAGES=%d ACCEPTED=%d\n', ...
            numel(out_ood.conditions), numel(out_ood.voyages), ...
            res.manifest.ood.n_accepted);
        fprintf('FLEETV3_OOD_STRESS_REMOVED=%d\n', ...
            out_ood.n_stress_removed);
        for ci = 1:numel(out_ood.conditions)
            c = out_ood.conditions(ci);
            fprintf(['FLEETV3_OOD_%s axis=%s attempts=%d accept=1 ', ...
                'guard_reject=%d target_reject=%d stress=%d\n'], ...
                c.condition_id, c.ood_axis, c.attempts_used, ...
                c.reject_guard_band, c.reject_no_target, ...
                c.stress_set_removed);
        end
        fprintf('FLEETV3_MANIFEST=%s\n', res.manifest_path);
        % verdict gate (T-018 qualification semantics): every IID member
        % ACCEPT and every OOD condition fully populated at the member
        % level - a condition whose members failed generation is NOT a
        % pass (failed-voyage handling records it, the verdict gates it)
        n_bad_iid = 0;
        for vi = 1:numel(out_iid.voyages)
            if iscell(out_iid.voyages)
                vrow = out_iid.voyages{vi};
            else
                vrow = out_iid.voyages(vi);
            end
            if ~strcmp(char(vrow.disposition), 'ACCEPT')
                n_bad_iid = n_bad_iid + 1;
            end
        end
        exp_members = double(plan.ood.members_per_family);
        if strncmp(FLEET_MODE, 'stage_b', 7)
            exp_members = exp_members * ...
                double(plan.ood.per_condition_families);
        end
        n_bad_cond = 0;
        for ci = 1:numel(out_ood.conditions)
            if double(out_ood.conditions(ci).accepted_voyages) ...
                    < exp_members
                n_bad_cond = n_bad_cond + 1;
                fprintf('FLEETV3_CONDITION_INCOMPLETE %s accept=%d/%d\n', ...
                    out_ood.conditions(ci).condition_id, ...
                    out_ood.conditions(ci).accepted_voyages, ...
                    exp_members);
            end
        end
        if n_bad_iid > 0 || n_bad_cond > 0
            error('GATE:FLEETV3:VERDICT', ...
                ['smoke/stage_a verdict FAIL: %d non-ACCEPT IID member(s), ', ...
                 '%d incomplete OOD condition(s) (see manifest/ledger)'], ...
                n_bad_iid, n_bad_cond);
        end
        fprintf('FLEETV3_VERDICT=PASS\n');
    case {'stage_b_iid', 'stage_b_ood'}
        error('GATE:FLEETV3:NOT_IMPLEMENTED', ...
            ['mode ''%s'' is Phase 2 scope: the internal generators ', ...
             'already carry the formal Stage B parameterization (seed ', ...
             'segments, families, gates) via fleet_v3.plan_load, but ', ...
             'Stage B execution is enabled only after the Stage A ', ...
             'qualified stack freeze'], FLEET_MODE);
    otherwise
        error('GATE:FLEETV3:ARG', ...
            'unknown FLEET_MODE: %s', FLEET_MODE);
end
