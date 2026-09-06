function out = producer_confirm(mode, varargin)
% mission_v3.producer_confirm - T429_CONFIRMATION_V2 fresh confirmation
% corpus producer (execution of the T429-AMENDMENT-V2 preregistration
% results/mission_v3/t429_amendment_v2.json, self_hash 796a8b07d0b3c521
% d10c41f4291ca8b8a54fb32925e93f926c02ec10c08427d8, frozen before
% generation; delegation spec
% experiments/mission_v3/T429CONF_generation_spec.md).
%
%   ctx = mission_v3.producer_confirm('ctx')
%   out = mission_v3.producer_confirm('layer', name-values)
%   out = mission_v3.producer_confirm('run' [, name-values])
%   row = mission_v3.producer_confirm('member', ctx, si, bdi, layer, ...
%                                     depth_class, dag, rl, fam, k [, pre])
%   out = mission_v3.producer_confirm('compare', ledger1, ledger2, ...
%           audit2Path, manifest2Path, outManifestPath, outAuditPath)
%
% MODES
%   'ctx'     provision context: pinned amendment verification (self_hash
%             against the preregistered value), frozen registry
%             verification (environment_regime_params +
%             identifiability_contrast_registry against the amendment
%             declared hashes), frozen chain verification (phase1_freeze +
%             D6 + sprint binding + t429_freeze_v1 presence), stack
%             verification (all non-generator components of the T-016
%             frozen qualification capture unchanged; fresh generator
%             composite recorded for T429_FREEZE_v2), normalized draw
%             allocation from confirmation_plan_exact, follow-guarantee
%             constants, depth bands, lead grid, producer self hash.
%   'layer'   one layer generation (coverage | depth | traffic) with the
%             conditional draw walk + member expansion; used by the
%             pilot.  Name-values:
%               'Layer'            required: 'coverage' | 'depth' |
%                                  'traffic'
%               'RunLabel'         't429conf_v2' (default); names the
%                                  dataset dir datasets_v3/mission_v3/
%                                  <label>/
%               'StratumFilter'    cellstr subset of the layer strata
%               'DrawsOverride'    [] (default = frozen plan values);
%                                  pilot only: whole number of draws
%               'AuditJsonPath'    required (pilot writes into its own
%                                  dataset dir)
%               'ManifestJsonPath' required
%               'Resume'           true (default): reuse complete ledger
%                                  rows whose artifact already exists
%               'VerdictError'     true (default): raise
%                                  GATE:MISSION_V3:CONF:INCOMPLETE when
%                                  the completeness verdict is FAIL
%                                  (audit/manifest written BEFORE the
%                                  error, controlled FAIL)
%               'ProvisionBase'    true (default)
%   'run'     one full confirmation pass over the frozen plan:
%             coverage (6 strata x 4 draws x 4 members) + depth (12 low +
%             12 normal x GOOD_ON) + traffic (12 draws x GOOD_ON+GOOD_OFF)
%             = 144 member voyages.  Name-values: 'RunLabel',
%             'AuditJsonPath', 'ManifestJsonPath', 'Resume',
%             'VerdictError', 'ProvisionBase' (defaults as in 'layer';
%             DrawsOverride is NOT accepted in 'run' - frozen plan only).
%   'member'  single member execution (unit-testable).  Optional pre
%             struct: field SimOnly=true stops right before the
%             simulation (returns the hotel guard fact only).
%   'compare' two-run determinism comparison of two run ledgers (144
%             payload hash pairs), final audit/manifest rewrite with the
%             two-run consistency section, then the controlled error when
%             the second-run completeness verdict is FAIL.
%
% FROZEN VALUES (never re-decided here; all read from the amendment JSON
% except the three pinned constants below whose source is the delegation
% spec / the amendment itself):
%   seed root (confirmation_plan_exact.seed_root =
%   MISSION_V3_T429CONF_V2_SEED_2026_09_06, disjoint from the prequal
%   seed space); stratum seed segments [k*1000, (k+1)*1000) with k the
%   0-based draw_allocation order; draw quotas per stratum (4x4 coverage
%   x6, 24 GOOD_ON depth, 12x2 traffic); conditional sampling bands
%   (depth low [1.5,2.0] pinned from the amendment class_A text +
%   delegation spec Fixed Decision 2; depth normal [2.2,2.6] read
%   machine-readably from
%   identifiability_contrast_registry.json DEPTH_LOW_ID_VS_NORMAL_ID
%   .normal_id_depth_band and cross-checked against the pinned low-band
%   maximum); traffic condition (cruise segment contains >= 1 event);
%   STEADY_FOLLOW guarantee (plateau fraction and minimum read from
%   fix3b_traffic_host.duration); sampling-reject discipline (resample
%   with draw increment, no slot); FAILED-slot retry max 2 (amendment
%   retry_discipline "same seed/config retry <= 2").
%
% CONDITIONAL WALK (fixed accounting): each candidate base draw index is
% evaluated exactly once (env_realize 'sample' with MaxAttempts=1,
% ExhaustAction='return_ledger').  Outcomes: ENV_REJECTED (frozen env
% judgment: support bounds / structural heldout tokens) and COND_REJECTED
% (depth band miss or zero cruise events; counted as sampling rejects,
% never occupying a ledger slot) both advance the draw cursor by one;
% only accepted draws whose condition passes occupy slots and expand to
% members.  Depth classes are filled strictly low-band first
% (first 12 accepted), then normal-band (next 12); the draw cursor is
% shared across the class boundary.  Escalation guard: a conditional
% class that needs more than 200 candidate attempts (env + condition
% rejects) raises GATE:MISSION_V3:CONF:ACCEPT_RATE (delegation spec
% Escalate If clause; controlled stop, no silent acceptance-rate tuning).
%
% STEADY_FOLLOW HOST ASSERTION (new, GATE:MISSION_V3:CONF:FOLLOW_HOST):
% for every accepted TRAFFIC-layer draw, every traffic event on a cruise
% segment must satisfy plateau_fraction * duration_s >= steady_follow_min
% (0.8 * d >= 180 s; fraction and minimum read from the amendment
% fix3b_traffic_host.duration).  Violation means the amended environment
% implementation is not in effect - controlled stop, never a resample.
%
% TWO-RUN DETERMINISM: the conditional walk is a pure function of the
% seed root and the frozen registries, so both passes replay identical
% accepted draws and members; 144 payload hash pairs must match
% (GATE:MISSION_V3:CONF:TWO_RUN).
%
% R3/R4 RED LINE: no T-429 result metric (SBD / BA / confidence bound /
% confusion matrix or any other outcome metric) is ever computed or
% emitted; a field-name blacklist self-check runs before every JSON
% write (GATE:MISSION_V3:CONF:T429).
%
% Discipline: ASCII-only source; no absolute paths; no chained indexing;
% ROOT resolved from mfilename with pwd fallback; thin dispatch + local
% functions; Model E is never modified (runtime injection only through
% the scenario_compile whitelist); traffic-OFF members compose a
% realization COPY with traffic events stripped (T-020 existing
% semantics; the truth hash stays the as-drawn payload per the T-014
% ruling).

    if ~(ischar(mode) || isstring(mode))
        error('GATE:MISSION_V3:CONF:ARG', 'mode must be a char vector');
    end
    switch char(mode)
        case 'ctx'
            out = local_make_ctx();
        case 'layer'
            out = local_layer_run(varargin);
        case 'run'
            out = local_run(varargin);
        case 'member'
            out = local_member_wrapper(varargin);
        case 'compare'
            out = local_compare(varargin);
        otherwise
            error('GATE:MISSION_V3:CONF:ARG', ...
                'unknown mode: %s', char(mode));
    end
end

%% ======================================================================
%  pinned frozen constants (delegation spec / amendment transcription)
%% ======================================================================
function h = CONF_AMENDMENT_SELF_HASH()
% T429-AMENDMENT-V2 preregistered self_hash (delegation spec authority
% chain; verified by independent canonical recomputation before the pin
% was transcribed here).
    h = ['796a8b07d0b3c521d10c41f4291ca8b8a54fb32925e93f926c02ec10c08427d8'];
end

function b = CONF_DEPTH_LOW_BAND()
% LOW_ID_BAND [1.5, 2.0], closed interval (amendment
% fix2_depth_token_safe_layer.class_A "HT in LOW_ID_BAND [1.5,2.0]" and
% confirmation_plan_exact.sampling_conditions.depth_low; Day-3 section D
% single-variable attribution keeps the band unchanged).  The normal band
% is NOT pinned here: it is read from the frozen contrast registry and
% cross-checked against this pinned maximum.
    b = [1.5, 2.0];
end

function n = CONF_MAX_CLASS_ATTEMPTS()
% Conditional-class candidate-attempt escalation cap (delegation spec
% Escalate If: "depth low > 200 attempts/12 groups -> stop and report").
    n = 200;
end

function n = CONF_RETRY_MAX()
% FAILED-slot retry max (amendment retry_discipline: same seed/config
% retry <= 2).
    n = 2;
end

function c = CONF_LAYERS()
    c = {'coverage', 'depth', 'traffic'};
end

%% ======================================================================
%  context
%% ======================================================================
function ctx = local_make_ctx()
    ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
        ROOT = pwd;
    end
    ctx = struct();
    ctx.ROOT = ROOT;
    ctx.MDL = 'HybridShip_ModelE';
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    % ---- pinned amendment verification ----
    am_path = fullfile(ROOT, 'results', 'mission_v3', ...
        't429_amendment_v2.json');
    if exist(am_path, 'file') ~= 2
        error('GATE:MISSION_V3:CONF:AMENDMENT', ...
            'T429-AMENDMENT-V2 preregistration file not found');
    end
    am = local_read_json_nodir(am_path);
    am_sh = am.self_hash;
    if ~strcmp(char(am_sh.value), CONF_AMENDMENT_SELF_HASH())
        error('GATE:MISSION_V3:CONF:AMENDMENT', ...
            ['amendment self_hash %s does not reproduce the pinned ', ...
             'preregistered value %s (file changed after freeze)'], ...
            char(am_sh.value), CONF_AMENDMENT_SELF_HASH());
    end
    if ~logical(am.frozen_before_generation)
        error('GATE:MISSION_V3:CONF:AMENDMENT', ...
            'amendment is not marked frozen_before_generation');
    end
    ctx.amendment = am;
    ctx.amendment_rel = 'results/mission_v3/t429_amendment_v2.json';

    cpe = am.confirmation_plan_exact;
    ctx.seed_root = char(cpe.seed_root);
    ctx.passes = double(cpe.passes);
    ctx.expected_artifacts = double(cpe.expected_artifacts);
    ctx.alloc = local_normalize_alloc(cpe.draw_allocation, ROOT);

    % ---- registry verification against the amendment declared hashes ----
    rgc = am.registry_and_generator_changes;
    env = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'environment_regime_params.json');
    env_sh = env.self_hash;
    ctx.hash_env_params = char(env_sh.value);
    if ~strcmp(ctx.hash_env_params, char(rgc.environment_regime_params.new))
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            ['environment_regime_params self_hash %s is not the ', ...
             'amendment declared new hash %s'], ctx.hash_env_params, ...
            char(rgc.environment_regime_params.new));
    end
    ctr = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'identifiability_contrast_registry.json');
    ctr_sh = ctr.self_hash;
    ctx.hash_contrast = char(ctr_sh.value);
    if ~strcmp(ctx.hash_contrast, char(rgc.identifiability_contrast_registry.new))
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            ['identifiability_contrast_registry self_hash %s is not ', ...
             'the amendment declared new hash %s'], ctx.hash_contrast, ...
            char(rgc.identifiability_contrast_registry.new));
    end
    if ~strcmp(char(rgc.model_e_baselines), 'zero change')
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            'amendment no longer declares model_e_baselines zero change');
    end

    % ---- depth bands ----
    band_n = local_normal_depth_band(ctr);
    band_lo = CONF_DEPTH_LOW_BAND();
    ctx.band_low = struct('min', band_lo(1), ...
        'max', band_lo(2), 'min_inclusive', true, ...
        'max_inclusive', true, 'source', ['pinned from T429-AMENDMENT-V2 ', ...
        'fix2 class_A text + delegation spec Fixed Decision 2']);
    ctx.band_normal = struct('min', band_n.min, 'max', band_n.max, ...
        'min_inclusive', logical(band_n.min_inclusive), ...
        'max_inclusive', logical(band_n.max_inclusive), ...
        'source', ['identifiability_contrast_registry.json ', ...
        'DEPTH_LOW_ID_VS_NORMAL_ID.normal_id_depth_band']);
    if ~(ctx.band_low.max < ctx.band_normal.min)
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            ['pinned low-band max %.4f does not sit below the registry ', ...
             'normal-band min %.4f (band drift guard)'], ...
            ctx.band_low.max, ctx.band_normal.min);
    end

    % ---- STEADY_FOLLOW guarantee constants (fix3b) ----
    f3b = am.fix3b_traffic_host;
    dur = f3b.duration;
    ev_tot = double(dur.event_total_s);
    sf_s = double(dur.steady_follow_s);
    ctx.follow_frac = sf_s(2) / ev_tot(2);
    ctx.steady_min_s = sf_s(1);
    if ~(ctx.follow_frac > 0) || ~(ctx.follow_frac <= 1)
        error('GATE:MISSION_V3:CONF:ARG', ...
            'fix3b steady-follow plateau fraction is not in (0, 1]');
    end
    ctx.follow_event_total_s = ev_tot;
    ctx.follow_steady_s = sf_s;

    % ---- cruise role for the traffic condition ----
    tpl_reg = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'mission_template_registry.json');
    role_enum = local_cellstr_of(tpl_reg.segment_role_enum);
    if ~ismember('cruise', role_enum)
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            'segment_role_enum lacks the cruise role');
    end
    ctx.cruise_role = 'cruise';

    % ---- caps ----
    ctx.max_class_attempts = CONF_MAX_CLASS_ATTEMPTS();
    ctx.retry_max = CONF_RETRY_MAX();

    % ---- frozen chain (phase1_freeze + D6 + sprint + t429_freeze_v1) ----
    ctx.chain = local_verify_chain(ROOT);

    % ---- stack verification (T-016 frozen capture, non-generator) ----
    ctx.stack = local_verify_stack(ROOT);

    % ---- lead grid / schema / stream map ----
    w0 = mission_v3.eligibility('windows', 0);
    ctx.leads = (1:double(w0.n_lead_bins)) * double(w0.lead_bin_s);

    ctx.schema = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_producer_schema.json');
    ctx.sigreg = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_signal_registry.json');
    ctx.stream_map = local_stream_map(ctx);

    ctx.producer_sha = local_sha256_file([mfilename('fullpath'), '.m']);
    ctx.model_ready = false;
    ctx.run_label = 't429conf_v2';
    ctx.artifact_dir = fullfile(ctx.ROOT, 'datasets_v3', 'mission_v3', ...
        't429conf_v2');
end

function alloc = local_normalize_alloc(draw_allocation, ROOT)
% Normalize confirmation_plan_exact.draw_allocation (JSON array) into a
% struct array with the layer classification and the seed segment base
% [k*1000, (k+1)*1000) (delegation spec Fixed Decision 1, k 0-based in
% draw_allocation order).  Layer classification is the mechanical reading
% of the amendment entries: WORK_MISSION__DRY -> depth, WORK_MISSION__NORMAL
% -> traffic, everything else -> coverage (purposes Fix2 DEPTH / Fix3b
% TRAFFIC / Fix1).
    entries = local_struct_cell(draw_allocation);
    alloc = struct('si', {}, 'stratum_id', {}, 'template_id', {}, ...
        'regime_id', {}, 'layer', {}, 'n_draws', {}, ...
        'members_spec', {}, 'draw_start', {});
    n_cov = 0;
    n_dep = 0;
    n_trf = 0;
    streg = local_read_json(fullfile(ROOT, 'config', 'mission_v3'), ...
        'scenario_stratum_registry.json');
    for i = 1:numel(entries)
        e = entries{i};
        st = char(e.stratum);
        parts = strsplit(st, '__');
        if numel(parts) ~= 2
            error('GATE:MISSION_V3:CONF:PLAN', ...
                'allocation stratum id not of the form <TPL>__<REG>: %s', st);
        end
        tpl = strrep(char(parts{1}), 'STRAT_', '');
        reg = char(parts{2});
        if strcmp(st, 'STRAT_WORK_MISSION__DRY')
            layer = 'depth';
            n_dep = n_dep + 1;
        elseif strcmp(st, 'STRAT_WORK_MISSION__NORMAL')
            layer = 'traffic';
            n_trf = n_trf + 1;
        else
            layer = 'coverage';
            n_cov = n_cov + 1;
        end
        % stratum registry presence + ACTIVE status
        found = false;
        rows = local_struct_cell(streg.strata);
        for j = 1:numel(rows)
            r = rows{j};
            if strcmp(char(r.scenario_stratum_id), st)
                found = true;
                if ~strcmp(char(r.status), 'ACTIVE')
                    error('GATE:MISSION_V3:CONF:PLAN', ...
                        'allocation stratum is not ACTIVE in the registry: %s', st);
                end
                if ~strcmp(char(r.mission_template_id), tpl) ...
                        || ~strcmp(char(r.environment_regime), reg)
                    error('GATE:MISSION_V3:CONF:PLAN', ...
                        ['allocation stratum %s disagrees with the ', ...
                         'registry template/regime'], st);
                end
                break;
            end
        end
        if ~found
            error('GATE:MISSION_V3:CONF:PLAN', ...
                'allocation stratum not in the frozen registry: %s', st);
        end
        row = struct('si', double(i), 'stratum_id', st, ...
            'template_id', tpl, 'regime_id', reg, 'layer', layer, ...
            'n_draws', double(e.draws), 'members_spec', char(e.members), ...
            'draw_start', double((i - 1) * 1000));
        alloc(i) = row;
    end
    if (n_cov ~= 6) || (n_dep ~= 1) || (n_trf ~= 1) || (numel(entries) ~= 8)
        error('GATE:MISSION_V3:CONF:PLAN', ...
            ['allocation shape drifted from the frozen plan shape ', ...
             '(6 coverage + 1 depth + 1 traffic entries expected; got ', ...
             '%d/%d/%d of %d)'], n_cov, n_dep, n_trf, numel(entries));
    end
end

function band = local_normal_depth_band(ctr)
    contrasts = local_struct_cell(ctr.contrasts);
    band = [];
    for i = 1:numel(contrasts)
        c = contrasts{i};
        if strcmp(char(c.contrast_id), 'DEPTH_LOW_ID_VS_NORMAL_ID')
            band = c.normal_id_depth_band;
            break;
        end
    end
    if isempty(band)
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            'contrast DEPTH_LOW_ID_VS_NORMAL_ID not found in the registry');
    end
    if ~isnumeric(band.min) || ~isnumeric(band.max) ...
            || ~(double(band.min) < double(band.max))
        error('GATE:MISSION_V3:CONF:REGISTRY', ...
            'normal_id_depth_band is not a valid ordered band');
    end
end

%% ======================================================================
%  frozen chain + stack verification (fail-closed, ctx build)
%% ======================================================================
function chain = local_verify_chain(ROOT)
    res = fullfile(ROOT, 'results', 'mission_v3');
    frz = local_read_json(res, 'phase1_freeze.json');
    d6 = local_read_json(res, 'phase1_freeze_amendment_d6.json');
    spr = local_read_json(res, 'phase1_freeze_amendment_sprint.json');
    base_h = local_self_hash_of(frz);
    d6_h = local_self_hash_of(d6);
    spr_h = local_self_hash_of(spr);
    if ~strcmp(char(d6.amends.self_hash), base_h) ...
            || ~strcmp(char(spr.amends.self_hash), base_h)
        error('GATE:MISSION_V3:CONF:CHAIN', ...
            'D6/sprint amendments do not bind the base phase1_freeze hash');
    end
    chain_list = local_struct_cell(spr.amendment_chain);
    if numel(chain_list) < 2 ...
            || ~strcmp(char(chain_list{2}.self_hash), d6_h)
        error('GATE:MISSION_V3:CONF:CHAIN', ...
            'sprint amendment chain does not bind the D6 amendment');
    end
    t429_path = fullfile(res, 't429_freeze.json');
    if exist(t429_path, 'file') ~= 2
        error('GATE:MISSION_V3:CONF:CHAIN', ...
            't429_freeze.json (v1 preserved freeze) not found');
    end
    t429 = local_read_json_nodir(t429_path);
    if ~strcmp(char(t429.schema), 'mission_v3_t429_freeze_v1')
        error('GATE:MISSION_V3:CONF:CHAIN', ...
            't429_freeze.json is not the v1 schema');
    end
    chain = struct();
    chain.phase1_freeze_self_hash = base_h;
    chain.amendment_d6_self_hash = d6_h;
    chain.amendment_sprint_self_hash = spr_h;
    chain.t429_freeze_v1 = 'present (preserved_v1; hash pinned in the amendment)';
end

function h = local_self_hash_of(obj)
    if ~isfield(obj, 'self_hash') || ~isfield(obj.self_hash, 'value')
        error('GATE:MISSION_V3:CONF:CHAIN', ...
            'frozen object lacks self_hash.value');
    end
    h = char(obj.self_hash.value);
end

function stack = local_verify_stack(ROOT)
    fid_path = fullfile(ROOT, 'results', 'mission_v3', ...
        'qualification_stack_identity.json');
    frozen_identity = local_read_json_nodir(fid_path);
    fresh = mission_v3.stack_identity('collect', ROOT);
    fcomps = local_struct_cell(frozen_identity.components);
    ncomps = local_struct_cell(fresh.components);
    n_checked = 0;
    for j = 1:numel(fcomps)
        c = fcomps{j};
        nm = char(c.component);
        if strcmp(nm, 'mission_v3_generator')
            continue;
        end
        cj = local_find_comp(ncomps, nm);
        if isempty(cj)
            error('GATE:MISSION_V3:CONF:STACK', ...
                'frozen stack component missing in fresh collect: %s', nm);
        end
        if ~strcmp(char(cj.sha256), char(c.sha256))
            error('GATE:MISSION_V3:CONF:STACK', ...
                'frozen stack component changed since T-016: %s', nm);
        end
        n_checked = n_checked + 1;
    end
    gen_dir = fullfile(ROOT, 'scripts', '+mission_v3');
    [h_all, names_all] = local_gen_hashes(gen_dir);
    stack = struct();
    stack.frozen_capture = 'results/mission_v3/qualification_stack_identity.json';
    stack.non_generator_components_verified = double(n_checked);
    stack.generator_composite_fresh = local_gen_composite(names_all, h_all);
    stack.generator_note = ['composite includes both producers added ', ...
        'after the T-016 capture (producer_prequal.m, producer_confirm.m); ', ...
        'recorded fresh for T429_FREEZE_v2, all frozen-time scientific ', ...
        'components verified unchanged above'];
end

function [hashes, names] = local_gen_hashes(gen_dir)
    lst = dir(fullfile(gen_dir, '*.m'));
    names = cell(1, 0);
    hashes = cell(1, 0);
    for i = 1:numel(lst)
        if lst(i).isdir, continue; end
        names{end+1} = lst(i).name; %#ok<AGROW>
        hashes{end+1} = local_sha256_file(fullfile(gen_dir, ...
            lst(i).name)); %#ok<AGROW>
    end
    [names, si] = sort(names);
    hashes = hashes(si);
end

function h = local_gen_composite(names, hashes)
    parts = cell(1, numel(names));
    for i = 1:numel(names)
        parts{i} = ['"' names{i} '":"' hashes{i} '"'];
    end
    canon = ['{' strjoin(parts, ',') '}'];
    h = local_sha256_bytes(unicode2native(canon, 'UTF-8'));
end

function cj = local_find_comp(comps, name)
    cj = [];
    for j = 1:numel(comps)
        c = comps{j};
        if strcmp(char(c.component), name)
            cj = c;
            return;
        end
    end
end

%% ======================================================================
%  provision + one member execution (prequal chain precedent)
%% ======================================================================
function local_provision(ctx)
    ROOT = ctx.ROOT;
    cfg_dir = fullfile(ROOT, 'config');
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_Physics_Parameters.m')));
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_Control_Parameters.m')));
    evalin('base', sprintf("run('%s')", ...
        fullfile(cfg_dir, 'ModelE_TestScenarios.m')));
    load_system(fullfile(ROOT, 'model', [ctx.MDL '.slx']));
    ctx.model_ready = true;
end

function row = local_member_wrapper(arg_cell)
    ctx = arg_cell{1};
    si = arg_cell{2};
    bdi = arg_cell{3};
    layer = arg_cell{4};
    depth_class = arg_cell{5};
    dag = arg_cell{6};
    rl = arg_cell{7};
    fam = arg_cell{8};
    k = arg_cell{9};
    pre = struct();
    if numel(arg_cell) >= 10
        pre = arg_cell{10};
    end
    if ~logical(ctx.model_ready)
        local_provision(ctx);
    end

    mem = fam.members(k);
    arms_v = mem.arms;
    voyage_id = sprintf('CONF_S%d_D%04d_M%d', si, bdi, k);
    row = local_blank_row(si, layer, depth_class, char(dag.stratum_id), ...
        char(dag.template_id), char(dag.regime_id), bdi, fam, k, voyage_id);
    row.arms_traffic = char(arms_v.traffic);
    row.arms_forecast = char(arms_v.forecast);
    row.traffic_override = char(mem.traffic_override);

    use_rl = rl;
    if strcmp(row.traffic_override, 'OFF')
        use_rl.traffic = rl.traffic([]);
    end

    plan = mission_v3.plan_compose('compose', use_rl);
    comp = mission_v3.scenario_compile('compile', plan, use_rl);
    scn = comp.scn;

    p_base = dag.p_hotel_base_kW;
    d_hotel = dag.delta_p_hotel_unplanned;
    expect_hotel = double(p_base) + double(d_hotel);
    if abs(double(scn.hotel_kW) - expect_hotel) > 1e-9
        error('GATE:MISSION_V3:PREQUAL:HOTEL_GUARD', ...
            ['voyage %s: scn.hotel_kW %.17g != p_hotel_base %.17g + ', ...
             'delta_p_hotel_unplanned %.17g (T-011 caller contract)'], ...
            voyage_id, double(scn.hotel_kW), double(p_base), ...
            double(d_hotel));
    end

    mission_v3.scenario_compile('check_injection', comp.injection_list);
    gate_e.reset_bench_nominal();
    il = comp.injection_list;
    for j = 1:numel(il)
        e = il(j);
        assignin('base', e.var_name, e.value);
    end

    if isfield(pre, 'SimOnly') && logical(pre.SimOnly)
        row.scn_hotel_kW = double(scn.hotel_kW);
        row.plan_t_end_s = double(plan.t_grid_s(end));
        return;
    end

    t_sim = tic;
    simout = model.run_scenario_sim(ctx.MDL, scn, 0, scn.stop_time_s);
    row.sim_wall_s = toc(t_sim);
    ds = gate_ctrl.get_ds(simout);

    sel = local_grid1hz(ds.t);
    t1 = ds.t(sel);
    n1 = numel(t1);
    if n1 < 1
        error('CONF_RUNTIME:EMPTY_STREAM', ...
            'voyage %s: simulation produced an empty time grid', voyage_id);
    end
    stream = struct();
    stream.time_s = t1(:);
    stream = local_fill_stream(ctx, ds, sel, n1, stream, voyage_id);
    stream.ctrl_mode = uint8(stream.ctrl_mode);
    stream.scenario_id = voyage_id;

    D = double(t1(end));
    row.duration_s = D;
    anchors = (300:10:(D - 300))';

    tg_col = plan.t_grid_s(:);
    cur_col = scn.current_value_mps(:);
    ou = rl.ou;
    npre = double(round(ou.prehistory_s));
    t_pre = (-npre:-1)';
    t_src = [t_pre; tg_col];
    u_src = [zeros(npre, 1); cur_col];

    prof = row.arms_forecast;
    nA = numel(anchors);
    fb_all = [];
    for m = 1:nA
        a = anchors(m);
        sl = t_src <= a;
        fb = mission_v3.forecast_issue('issue', u_src(sl), t_src(sl), ...
            a, ctx.leads, prof, 'FormalSeed', ctx.seed_root, ...
            'BaseDrawIndex', double(bdi), 'RegimeId', row.regime_id, ...
            'DeltaWL', double(rl.water.delta_wl_m));
        if isempty(fb_all)
            fb_all = repmat(fb, nA, 1);
        end
        fb_all(m) = fb;
    end

    wtr = rl.water;
    kf = mission_v3.kf_snapshots('build_grid', plan, anchors, fb_all, ...
        'HChartM', double(wtr.h_chart_m));

    row_ok = false(nA, 1);
    for m = 1:nA
        mm = kf.missing_mask(m, :);
        cs = kf.contract_status{m};
        row_ok(m) = ~any(mm(:)) && strcmp(cs, 'OK');
    end
    voy = struct('duration_s', D, 'kf_issue_time_s', ...
        kf.issue_time_s(:), 'kf_row_ok', row_ok);
    elig = mission_v3.eligibility('judge', voy);

    fb_last = fb_all(end);
    prov = fb_last.provenance;
    fprov = struct();
    fprov.forecast_generator_id = char(prov.generator_id);
    fprov.forecast_generator_version = char(prov.version);
    fprov.forecast_generator_hash = char(prov.hash);
    fprov.issue_time_s = double(prov.issue_time_s);
    fprov.source_cutoff_time_s = double(prov.source_cutoff_s);
    fprov.forecast_provenance_class = char(prov.provenance_class_formal);

    meta = struct();
    meta.voyage_id = voyage_id;
    meta.voyage_family_id = char(fam.family_id);
    meta.route_template_id = char(dag.template_id);
    meta.mission_template_id = char(dag.template_id);
    meta.split_group_id = char(fam.split_group_id);
    meta.scenario_stratum = row.stratum_id;
    meta.generation_seed = double(scn.random_seed);
    meta.model_baseline_ref = ...
        'ModelE physical v1.1 + control v1.0 (P2 bench, law=0)';
    meta.producer_script_sha = ctx.producer_sha;
    meta.fixture_kind = 'NONE';
    meta.ood_axis = 'NONE';
    meta.ood_def_version = 'DGN-11-v1';
    meta.physics_domain_ref = 'ModelE validated domain (T-412 v1.1)';
    meta.h_chart_m = double(wtr.h_chart_m);
    meta.speed_limit_zones = plan.speed_limit_zones;
    meta.bridge_zones = plan.bridge_zones;
    meta.voyage_duration_s = D;
    meta.scenario_id = voyage_id;
    meta.forecast_provenance = fprov;
    meta.base_draw_id = char(fam.base_draw_id);
    meta.intervention = ['traffic=' row.arms_traffic ...
        ';forecast=' row.arms_forecast];
    meta.forecast_quality_profile_id = row.arms_forecast;
    meta.stratum = row.stratum_id;
    meta.dataset_domain = 'FORMAL';

    art_dir = ctx.artifact_dir;
    out_file = fullfile(art_dir, [voyage_id '.mat']);
    exp_res = mission_v3.producer_export('export_artifact', stream, kf, ...
        meta, elig, out_file);

    row.disposition = char(elig.disposition);
    row.reason = char(elig.reason);
    row.detail = char(elig.detail);
    row.n_stream = double(exp_res.n_stream_samples);
    row.n_kf_rows = double(exp_res.n_kf_rows);
    row.artifact_rel = ['datasets_v3/mission_v3/' char(ctx.run_label) ...
        '/' voyage_id '.mat'];
    row.artifact_sha256 = local_sha256_file(out_file);
    row.payload_hash = local_payload_hash(stream, kf, elig);
end

function stream = local_fill_stream(ctx, ds, sel, n1, stream, voyage_id)
    sm = ctx.stream_map;
    fnames = fieldnames(sm);
    for j = 1:numel(fnames)
        f = fnames{j};
        m = sm.(f);
        src = char(m.source);
        if strncmp(src, 'logsout:', 8)
            nm = matlab.lang.makeValidName(src(9:end));
            if ~isfield(ds, nm)
                error('GATE:MISSION_V3:CONF:STREAM', ...
                    'voyage %s: logsout lacks %s (registry %s)', ...
                    voyage_id, nm, src);
            end
            col = ds.(nm);
            stream.(f) = col(sel);
        elseif strncmp(src, 'scenario:', 9)
            v = evalin('base', src(10:end));
            stream.(f) = repmat(double(v), n1, 1);
        else
            error('GATE:MISSION_V3:CONF:STREAM', ...
                'voyage %s: unsupported source %s', voyage_id, src);
        end
    end
end

%% ======================================================================
%  single candidate draw (one base_draw_index, one env evaluation)
%% ======================================================================
function samp = local_draw_one(ctx, bdi, tpl, reg, st)
    out = mission_v3.env_realize('sample', ctx.seed_root, bdi, tpl, reg, ...
        st, 'MaxAttempts', 1, 'ExhaustAction', 'return_ledger');
    samp = struct();
    samp.accepted = logical(out.accepted);
    samp.draw = out.ledger(1).draw;
    samp.disposition = out.ledger(1).disposition;
    samp.realization = [];
    if samp.accepted
        samp.realization = out.realization;
    end
end

function r = local_sampling_row(st, layer, depth_class, bdi, outcome, ...
        disp_s, ht, ev_count, ev_durs)
    token_id = '';
    env_disp = '';
    env_reason = '';
    if ~isempty(disp_s)
        env_disp = char(disp_s.disposition);
        env_reason = char(disp_s.reason);
        token_id = char(disp_s.token_id);
    end
    r = struct();
    r.stratum_id = st;
    r.layer = layer;
    r.depth_class = depth_class;
    r.base_draw_index = double(bdi);
    r.outcome = outcome;
    r.env_disposition = env_disp;
    r.env_reason = env_reason;
    r.token_id = token_id;
    r.ht_effective = double(ht);
    r.cruise_event_count = double(ev_count);
    r.cruise_event_durations_s = ev_durs;
end

function tf = local_in_band(v, band)
    if ~isnumeric(v) || ~isscalar(v) || isnan(v)
        tf = false;
        return;
    end
    ok_min = (v > band.min) || (band.min_inclusive && v == band.min);
    ok_max = (v < band.max) || (band.max_inclusive && v == band.max);
    tf = ok_min && ok_max;
end

function evs = local_cruise_events(rl, cruise_role)
    evs = rl.traffic([]);
    tv = rl.traffic;
    for i = 1:numel(tv)
        if strcmp(char(tv(i).role), cruise_role)
            evs(end+1) = tv(i); %#ok<AGROW>
        end
    end
end

function local_assert_follow(ctx, rl, st, bdi)
% GATE:MISSION_V3:CONF:FOLLOW_HOST - every cruise-segment event of an
% accepted TRAFFIC draw must carry a STEADY_FOLLOW plateau of at least
% steady_follow_min seconds (plateau = follow_frac * duration).  A
% violation means the amended environment implementation is not in
% effect: controlled stop (never resampled away).
    evs = local_cruise_events(rl, ctx.cruise_role);
    for i = 1:numel(evs)
        ev = evs(i);
        plateau = ctx.follow_frac * double(ev.duration_s);
        if plateau < ctx.steady_min_s - 1e-9
            error('GATE:MISSION_V3:CONF:FOLLOW_HOST', ...
                ['draw %s d%d: cruise event duration %.6f s gives a ', ...
                 'STEADY_FOLLOW plateau %.6f s < %.6f s (amended env ', ...
                 'implementation not in effect)'], st, bdi, ...
                double(ev.duration_s), plateau, ctx.steady_min_s);
        end
    end
end

%% ======================================================================
%  conditional walk for one stratum (shared by all layers)
%% ======================================================================
function [bdi, led, sa] = local_walk_stratum(ctx, entry, cond_spec, ...
        n_slots, led, opt)
% cond_spec: struct with fields kind ('none' | 'depth' | 'traffic') and,
% for depth, band + class label.
    st = entry.stratum_id;
    tpl = entry.template_id;
    reg = entry.regime_id;
    layer = entry.layer;
    si = entry.si;
    bdi = entry.draw_start;

    n_acc = 0;
    n_env_rej = 0;
    n_cond_rej = 0;
    n_attempts = 0;
    acc_ht = [];
    acc_ev_counts = [];
    acc_ev_min_plateau = [];

    while n_acc < n_slots
        n_attempts = n_attempts + 1;
        if ~strcmp(cond_spec.kind, 'none') ...
                && (n_attempts > ctx.max_class_attempts)
            error('GATE:MISSION_V3:CONF:ACCEPT_RATE', ...
                ['conditional class %s of stratum %s needs more than %d ', ...
                 'candidate attempts (delegation spec Escalate If; ', ...
                 'accepted %d of %d slots; controlled stop, no ', ...
                 'acceptance-rate tuning)'], cond_spec.label, st, ...
                ctx.max_class_attempts, n_acc, n_slots);
        end

        samp = local_draw_one(ctx, bdi, tpl, reg, st);
        if ~samp.accepted
            n_env_rej = n_env_rej + 1;
            led.sampling{end+1} = local_sampling_row(st, layer, ...
                cond_spec.label, bdi, 'ENV_REJECTED', samp.disposition, ...
                NaN, -1, []); %#ok<AGROW>
            local_flush(opt.ledger_path, led);
            bdi = bdi + 1;
            continue;
        end

        rl = samp.realization;
        pass = true;
        ev_count = -1;
        ev_durs = [];
        if strcmp(cond_spec.kind, 'depth')
            ht = double(rl.water.ht_effective);
            pass = local_in_band(ht, cond_spec.band);
            if ~pass
                n_cond_rej = n_cond_rej + 1;
                led.sampling{end+1} = local_sampling_row(st, layer, ...
                    cond_spec.label, bdi, 'COND_REJECTED', ...
                    samp.disposition, ht, -1, []); %#ok<AGROW>
                local_flush(opt.ledger_path, led);
                bdi = bdi + 1;
                continue;
            end
            acc_ht(end+1) = ht; %#ok<AGROW>
            srow = local_sampling_row(st, layer, cond_spec.label, bdi, ...
                'ACCEPTED', samp.disposition, ht, -1, []);
        elseif strcmp(cond_spec.kind, 'traffic')
            evs = local_cruise_events(rl, ctx.cruise_role);
            ev_count = numel(evs);
            if ev_count < 1
                pass = false;
            else
                local_assert_follow(ctx, rl, st, bdi);
                ev_durs = zeros(1, ev_count);
                for i = 1:ev_count
                    ev_durs(i) = double(evs(i).duration_s);
                end
                acc_ev_min_plateau(end+1) = ctx.follow_frac * min(ev_durs); %#ok<AGROW>
            end
            if ~pass
                n_cond_rej = n_cond_rej + 1;
                led.sampling{end+1} = local_sampling_row(st, layer, ...
                    cond_spec.label, bdi, 'COND_REJECTED', ...
                    samp.disposition, NaN, ev_count, []); %#ok<AGROW>
                local_flush(opt.ledger_path, led);
                bdi = bdi + 1;
                continue;
            end
            acc_ev_counts(end+1) = ev_count; %#ok<AGROW>
            srow = local_sampling_row(st, layer, cond_spec.label, bdi, ...
                'ACCEPTED', samp.disposition, NaN, ev_count, ev_durs);
        else
            srow = local_sampling_row(st, layer, cond_spec.label, bdi, ...
                'ACCEPTED', samp.disposition, NaN, -1, []);
        end
        led.sampling{end+1} = srow; %#ok<AGROW>

        % ---- accepted draw: expand and execute the layer members ----
        dag = samp.draw;
        fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
        member_ks = local_select_members(fam, layer);
        for jj = 1:numel(member_ks)
            k = member_ks(jj);
            [row, led] = local_member_with_retry(ctx, si, layer, ...
                cond_spec.label, bdi, dag, rl, fam, k, led, opt);
            led.voyages{end+1} = row; %#ok<AGROW>
            local_flush(opt.ledger_path, led);
        end
        n_acc = n_acc + 1;
        bdi = bdi + 1;
    end

    sa = struct();
    sa.stratum_id = st;
    sa.stratum_index = double(si);
    sa.layer = layer;
    sa.template_id = tpl;
    sa.regime_id = reg;
    sa.condition = cond_spec.label;
    sa.slots_planned = double(n_slots);
    sa.accepted_draws = double(n_acc);
    sa.env_rejected_draws = double(n_env_rej);
    sa.cond_rejected_draws = double(n_cond_rej);
    sa.attempts_total = double(n_attempts);
    if strcmp(cond_spec.kind, 'depth')
        sa.accepted_ht_effective = acc_ht;
    end
    if strcmp(cond_spec.kind, 'traffic')
        sa.accepted_cruise_event_counts = acc_ev_counts;
        sa.min_steady_plateau_s = local_min_or_nan(acc_ev_min_plateau);
    end
end

function member_ks = local_select_members(fam, layer)
% Layer member subsets (delegation spec Fixed Decision 3):
%   coverage -> the full 2x2 family (4 members);
%   depth    -> GOOD_ON only (traffic ON x forecast GOOD_V1, 1 member);
%   traffic  -> GOOD_ON + GOOD_OFF (forecast GOOD_V1 pair, 2 members).
    member_ks = [];
    n = double(fam.n_members);
    switch layer
        case 'coverage'
            member_ks = 1:n;
        case 'depth'
            for k = 1:n
                mem = fam.members(k);
                arms_v = mem.arms;
                if strcmp(char(arms_v.traffic), 'ON') ...
                        && strcmp(char(arms_v.forecast), 'GOOD_V1')
                    member_ks(end+1) = k; %#ok<AGROW>
                end
            end
            if numel(member_ks) ~= 1
                error('GATE:MISSION_V3:CONF:MEMBERS', ...
                    ['depth layer expects exactly one GOOD_ON member; ', ...
                     'found %d (intervention matrix drift)'], ...
                    numel(member_ks));
            end
        case 'traffic'
            for k = 1:n
                mem = fam.members(k);
                arms_v = mem.arms;
                if strcmp(char(arms_v.forecast), 'GOOD_V1')
                    member_ks(end+1) = k; %#ok<AGROW>
                end
            end
            if numel(member_ks) ~= 2
                error('GATE:MISSION_V3:CONF:MEMBERS', ...
                    ['traffic layer expects exactly two GOOD_V1 ', ...
                     'members (ON + OFF); found %d (matrix drift)'], ...
                    numel(member_ks));
            end
            m1 = fam.members(member_ks(1));
            m2 = fam.members(member_ks(2));
            a1 = m1.arms;
            a2 = m2.arms;
            if ~strcmp(char(m1.traffic_override), 'ON') ...
                    || ~strcmp(char(m2.traffic_override), 'OFF')
                error('GATE:MISSION_V3:CONF:MEMBERS', ...
                    'traffic layer GOOD_V1 pair is not one ON + one OFF');
            end
            if strcmp(char(a1.traffic), char(a2.traffic))
                error('GATE:MISSION_V3:CONF:MEMBERS', ...
                    'traffic layer pair arms collide');
            end
        otherwise
            error('GATE:MISSION_V3:CONF:ARG', ...
                'unknown layer: %s', layer);
    end
end

function [row, led] = local_member_with_retry(ctx, si, layer, ...
        depth_class, bdi, dag, rl, fam, k, led, opt)
    mem = fam.members(k);
    voyage_id = sprintf('CONF_S%d_D%04d_M%d', si, bdi, k);

    if logical(opt.resume)
        for j = 1:numel(led.voyages)
            r0 = led.voyages{j};
            if strcmp(char(r0.voyage_id), voyage_id) ...
                    && ~isempty(char(r0.payload_hash)) ...
                    && ~strcmp(char(r0.disposition), 'FAILED_GENERATION')
                art = fullfile(ctx.ROOT, char(r0.artifact_rel));
                if exist(art, 'file')
                    row = r0;
                    led.voyages(j) = [];
                    return;
                end
            end
        end
    end

    n_try = ctx.retry_max + 1;
    row = [];
    for att = 1:n_try
        t_m = tic;
        try
            row = mission_v3.producer_confirm('member', ctx, si, bdi, ...
                layer, depth_class, dag, rl, fam, k);
            row.member_attempt = double(att);
            row.member_wall_s = toc(t_m);
            row.retry_of = double(max(0, att - 1));
            break;
        catch ME
            local_reraise_hard(ME);
            if att >= n_try
                row = local_blank_row(si, layer, depth_class, ...
                    char(dag.stratum_id), char(dag.template_id), ...
                    char(dag.regime_id), bdi, fam, k, voyage_id);
                row.arms_traffic = char(mem.arms.traffic);
                row.arms_forecast = char(mem.arms.forecast);
                row.traffic_override = char(mem.traffic_override);
                row.member_attempt = double(att);
                row.member_wall_s = toc(t_m);
                row.retry_of = double(max(0, att - 1));
                row.disposition = 'FAILED_GENERATION';
                row.reason = 'none';
                row.detail = local_fail_detail(ME);
            else
                rr = mission_v3.eligibility('retry_record', voyage_id, ...
                    att, 'FAILED_GENERATION', att - 1);
                rr.detail = local_fail_detail(ME);
                rr.stratum_id = char(dag.stratum_id);
                rr.voyage_id = voyage_id;
                led.retry{end+1} = rr; %#ok<AGROW>
            end
        end
    end
end

%% ======================================================================
%  layer driver
%% ======================================================================
function entries = local_layer_entries(ctx, layer, filt)
    entries = {};
    for i = 1:numel(ctx.alloc)
        e = ctx.alloc(i);
        if strcmp(char(e.layer), layer)
            entries{end+1} = e; %#ok<AGROW>
        end
    end
    if ~isempty(filt)
        keep = {};
        wanted = local_cellstr_of(filt);
        for i = 1:numel(entries)
            e = entries{i};
            if ismember(char(e.stratum_id), wanted)
                keep{end+1} = e; %#ok<AGROW>
            end
        end
        if numel(keep) ~= numel(wanted)
            error('GATE:MISSION_V3:CONF:ARG', ...
                'StratumFilter entries not all in layer %s', layer);
        end
        entries = keep;
    end
    if isempty(entries)
        error('GATE:MISSION_V3:CONF:ARG', ...
            'layer %s has no strata to generate', layer);
    end
end

function [led, strata_audit] = local_execute_layer(ctx, layer, entries, ...
        led, opt)
% Conditional draw walk + member execution for one layer.  Depth fills
% strictly low-band first (first 12 accepted), then normal-band, sharing
% the draw cursor (frozen plan order).  strata_audit is a CELL of
% per-stratum summary structs (rows are heterogeneous across layers).
    strata_audit = {};
    draws_override = opt.draws_override;
    for ie = 1:numel(entries)
        entry = entries{ie};
        si = entry.si;
        n_planned = double(entry.n_draws);
        if ~isempty(draws_override)
            n_planned = double(draws_override);
        end
        switch layer
            case 'coverage'
                cond = struct('kind', 'none', 'label', 'none', ...
                    'band', []);
                [bdi_after, led, sa] = local_walk_stratum(ctx, entry, ...
                    cond, n_planned, led, opt);
                sa.draw_start_base = double(entry.draw_start);
                sa.draw_end_next = double(bdi_after);
                sa.expected_voyages = double(n_planned * 4);
            case 'depth'
                n_low = n_planned;
                n_normal = 0.0;
                if isempty(draws_override)
                    n_low = 12.0;
                    n_normal = double(entry.n_draws) - 12.0;
                else
                    if n_planned > 12
                        n_low = 12.0;
                        n_normal = n_planned - 12.0;
                    end
                end
                cond_lo = struct('kind', 'depth', 'label', 'depth_low', ...
                    'band', ctx.band_low);
                [bdi_after, led, sa_lo] = local_walk_stratum(ctx, entry, ...
                    cond_lo, n_low, led, opt);
                sa_lo.draw_start_base = double(entry.draw_start);
                sa_lo.draw_end_next = double(bdi_after);
                sa_lo.expected_voyages = double(n_low * 1);
                cond_hi = struct('kind', 'depth', 'label', ...
                    'depth_normal', 'band', ctx.band_normal);
                [bdi_after, led, sa_hi] = local_walk_stratum(ctx, entry, ...
                    cond_hi, n_normal, led, opt);
                sa_hi.draw_start_base = double(bdi_after) - n_normal;
                sa_hi.draw_end_next = double(bdi_after);
                sa_hi.expected_voyages = double(n_normal * 1);
                sa = local_join_depth(sa_lo, sa_hi, entry, n_planned);
            case 'traffic'
                cond = struct('kind', 'traffic', 'label', ...
                    'traffic_cruise_event', 'band', []);
                [bdi_after, led, sa] = local_walk_stratum(ctx, entry, ...
                    cond, n_planned, led, opt);
                sa.draw_start_base = double(entry.draw_start);
                sa.draw_end_next = double(bdi_after);
                sa.expected_voyages = double(n_planned * 2);
            otherwise
                error('GATE:MISSION_V3:CONF:ARG', ...
                    'unknown layer: %s', layer);
        end
        if isempty(strata_audit)
            strata_audit = {sa};
        else
            strata_audit{end+1} = sa; %#ok<AGROW>
        end
        fprintf(['[T429CONF] stratum %s (%s/%s): accepted %d/%d slots, ' ...
            'env_rej %d, cond_rej %d, attempts %d\n'], ...
            char(entry.stratum_id), layer, char(sa.condition), ...
            double(sa.accepted_draws), double(n_planned), ...
            double(sa.env_rejected_draws), double(sa.cond_rejected_draws), ...
            double(sa.attempts_total));
    end
end

function sa = local_join_depth(sa_lo, sa_hi, entry, n_planned)
% Depth stratum summary joining the two conditional class walks; the
% machine-checkable quota facts (12 in low band + 12 in normal band on
% the full plan) live here.
    sa = struct();
    sa.stratum_id = sa_lo.stratum_id;
    sa.stratum_index = sa_lo.stratum_index;
    sa.layer = 'depth';
    sa.template_id = sa_lo.template_id;
    sa.regime_id = sa_lo.regime_id;
    sa.condition = 'depth_low_then_normal';
    sa.slots_planned = double(n_planned);
    sa.accepted_draws = double(sa_lo.accepted_draws) ...
        + double(sa_hi.accepted_draws);
    sa.env_rejected_draws = double(sa_lo.env_rejected_draws) ...
        + double(sa_hi.env_rejected_draws);
    sa.cond_rejected_draws = double(sa_lo.cond_rejected_draws) ...
        + double(sa_hi.cond_rejected_draws);
    sa.attempts_total = double(sa_lo.attempts_total) ...
        + double(sa_hi.attempts_total);
    sa.draw_start_base = double(entry.draw_start);
    sa.draw_end_next = double(sa_hi.draw_end_next);
    sa.expected_voyages = double(n_planned * 1);
    sa.class_low = struct('label', sa_lo.condition, ...
        'slots_planned', double(sa_lo.slots_planned), ...
        'accepted_draws', double(sa_lo.accepted_draws), ...
        'attempts_total', double(sa_lo.attempts_total), ...
        'env_rejected_draws', double(sa_lo.env_rejected_draws), ...
        'cond_rejected_draws', double(sa_lo.cond_rejected_draws), ...
        'accepted_ht_effective', sa_lo.accepted_ht_effective);
    sa.class_normal = struct('label', sa_hi.condition, ...
        'slots_planned', double(sa_hi.slots_planned), ...
        'accepted_draws', double(sa_hi.accepted_draws), ...
        'attempts_total', double(sa_hi.attempts_total), ...
        'env_rejected_draws', double(sa_hi.env_rejected_draws), ...
        'cond_rejected_draws', double(sa_hi.cond_rejected_draws), ...
        'accepted_ht_effective', sa_hi.accepted_ht_effective);
end

function m = local_min_or_nan(v)
    if isempty(v)
        m = NaN;
    else
        m = double(min(v));
    end
end

%% ======================================================================
%  layer mode (pilot entry point)
%% ======================================================================
function out = local_layer_run(arg_cell)
    ov = local_parse_named(arg_cell);
    if ~isfield(ov, 'Layer')
        error('GATE:MISSION_V3:CONF:ARG', ...
            '''layer'' mode requires the Layer name-value');
    end
    layer = char(ov.Layer);
    if ~ismember(layer, CONF_LAYERS())
        error('GATE:MISSION_V3:CONF:ARG', ...
            'Layer must be coverage|depth|traffic: %s', layer);
    end
    ctx = local_make_ctx();
    if ~isfield(ov, 'ProvisionBase') || logical(ov.ProvisionBase)
        local_provision(ctx);
    end

    label = 't429conf_v2';
    if isfield(ov, 'RunLabel')
        label = char(ov.RunLabel);
    end
    ctx.run_label = label;
    run_dir = fullfile(ctx.ROOT, 'datasets_v3', 'mission_v3', label);
    ctx.artifact_dir = run_dir;
    if ~exist(run_dir, 'dir'), mkdir(run_dir); end
    if ~isfield(ov, 'AuditJsonPath') || ~isfield(ov, 'ManifestJsonPath')
        error('GATE:MISSION_V3:CONF:ARG', ...
            ['''layer'' mode requires AuditJsonPath and ', ...
             'ManifestJsonPath (pilot products stay inside the pilot ', ...
             'dataset dir)']);
    end
    audit_path = char(ov.AuditJsonPath);
    man_path = char(ov.ManifestJsonPath);
    ledger_path = fullfile(run_dir, '_run_ledger.json');

    opt = struct();
    opt.ledger_path = ledger_path;
    opt.resume = true;
    if isfield(ov, 'Resume')
        opt.resume = logical(ov.Resume);
    end
    opt.draws_override = [];
    if isfield(ov, 'DrawsOverride') && ~isempty(ov.DrawsOverride)
        opt.draws_override = double(ov.DrawsOverride);
    end
    filt = {};
    if isfield(ov, 'StratumFilter')
        filt = ov.StratumFilter;
    end

    entries = local_layer_entries(ctx, layer, filt);
    led = struct('voyages', {{}}, 'sampling', {{}}, 'retry', {{}});
    t_run = tic;
    [led, strata_audit] = local_execute_layer(ctx, layer, entries, led, opt);
    run_wall = toc(t_run);

    agg = local_overall(layer, strata_audit, led.voyages, ctx, run_wall);
    audit = local_audit(ctx, layer, label, strata_audit, agg, run_wall, ...
        opt);
    man = local_manifest(ctx, label, led, strata_audit, agg, run_wall, ...
        layer);
    local_write_json(audit_path, audit);
    local_write_json(man_path, man);
    local_write_json(ledger_path, struct('voyages', {led.voyages}, ...
        'sampling', {led.sampling}, 'retry', {led.retry}, ...
        'throughput', man.throughput, 'verdict', char(agg.verdict)));

    try
        close_system(ctx.MDL, 0);
    catch
        % model not loaded (ProvisionBase=false); nothing to close
    end

    out = struct();
    out.layer = layer;
    out.run_label = label;
    out.run_dir = run_dir;
    out.audit_path = audit_path;
    out.manifest_path = man_path;
    out.ledger_path = ledger_path;
    out.audit = audit;
    out.manifest = man;
    out.verdict = char(agg.verdict);
    verdict_error = true;
    if isfield(ov, 'VerdictError')
        verdict_error = logical(ov.VerdictError);
    end
    if verdict_error && strcmp(out.verdict, 'FAIL')
        error('GATE:MISSION_V3:CONF:INCOMPLETE', ...
            ['confirmation layer %s completeness FAIL (audit and ', ...
             'manifest written to %s and %s; controlled FAIL by ', ...
             'design)'], layer, audit_path, man_path);
    end
end

%% ======================================================================
%  one full pass (all three layers, frozen plan quotas only)
%% ======================================================================
function out = local_run(arg_cell)
    ov = local_parse_named(arg_cell);
    if isfield(ov, 'DrawsOverride')
        error('GATE:MISSION_V3:CONF:ARG', ...
            ['''run'' mode executes the frozen plan quotas only; ', ...
             'DrawsOverride is a pilot-only option']);
    end
    ctx = local_make_ctx();
    if ~isfield(ov, 'ProvisionBase') || logical(ov.ProvisionBase)
        local_provision(ctx);
    end

    label = 't429conf_v2';
    if isfield(ov, 'RunLabel')
        label = char(ov.RunLabel);
    end
    ctx.run_label = label;
    run_dir = fullfile(ctx.ROOT, 'datasets_v3', 'mission_v3', label);
    ctx.artifact_dir = run_dir;
    if ~exist(run_dir, 'dir'), mkdir(run_dir); end
    if ~isfield(ov, 'AuditJsonPath') || ~isfield(ov, 'ManifestJsonPath')
        error('GATE:MISSION_V3:CONF:ARG', ...
            '''run'' mode requires AuditJsonPath and ManifestJsonPath');
    end
    audit_path = char(ov.AuditJsonPath);
    man_path = char(ov.ManifestJsonPath);
    ledger_path = fullfile(run_dir, '_run_ledger.json');

    opt = struct();
    opt.ledger_path = ledger_path;
    opt.resume = true;
    if isfield(ov, 'Resume')
        opt.resume = logical(ov.Resume);
    end
    opt.draws_override = [];

    led = struct('voyages', {{}}, 'sampling', {{}}, 'retry', {{}});
    t_run = tic;
    strata_audit = {};
    layers_all = CONF_LAYERS();
    for li = 1:numel(layers_all)
        layer = layers_all{li};
        entries = local_layer_entries(ctx, layer, {});
        [led, sa_layer] = local_execute_layer(ctx, layer, entries, led, ...
            opt);
        for ie = 1:numel(sa_layer)
            strata_audit{end+1} = sa_layer{ie}; %#ok<AGROW>
        end
    end
    run_wall = toc(t_run);

    agg = local_overall('run', strata_audit, led.voyages, ctx, run_wall);
    audit = local_audit(ctx, 'run', label, strata_audit, agg, run_wall, ...
        opt);
    man = local_manifest(ctx, label, led, strata_audit, agg, run_wall, ...
        'run');
    local_write_json(audit_path, audit);
    local_write_json(man_path, man);
    local_write_json(ledger_path, struct('voyages', {led.voyages}, ...
        'sampling', {led.sampling}, 'retry', {led.retry}, ...
        'throughput', man.throughput, 'verdict', char(agg.verdict)));

    try
        close_system(ctx.MDL, 0);
    catch
        % model not loaded (ProvisionBase=false); nothing to close
    end

    out = struct();
    out.run_label = label;
    out.run_dir = run_dir;
    out.audit_path = audit_path;
    out.manifest_path = man_path;
    out.ledger_path = ledger_path;
    out.audit = audit;
    out.manifest = man;
    out.verdict = char(agg.verdict);
    verdict_error = true;
    if isfield(ov, 'VerdictError')
        verdict_error = logical(ov.VerdictError);
    end
    if verdict_error && strcmp(out.verdict, 'FAIL')
        error('GATE:MISSION_V3:CONF:INCOMPLETE', ...
            ['confirmation pass %s completeness FAIL (audit and ', ...
             'manifest written to %s and %s; controlled FAIL by ', ...
             'design)'], label, audit_path, man_path);
    end
end

%% ======================================================================
%  two-run comparison + final manifest/audit rewrite
%% ======================================================================
function out = local_compare(arg_cell)
    ledger1 = char(arg_cell{1});
    ledger2 = char(arg_cell{2});
    audit2_path = char(arg_cell{3});
    man2_path = char(arg_cell{4});
    man_out = char(arg_cell{5});
    audit_out = char(arg_cell{6});

    l1 = local_read_json_nodir(ledger1);
    l2 = local_read_json_nodir(ledger2);
    v1 = local_cell_of(l1.voyages);
    v2 = local_cell_of(l2.voyages);

    mism = {};
    n_cmp = 0;
    for j = 1:numel(v2)
        r2 = v2{j};
        vid = char(r2.voyage_id);
        hit = '';
        for i = 1:numel(v1)
            r1 = v1{i};
            if strcmp(char(r1.voyage_id), vid)
                hit = char(r1.payload_hash);
                break;
            end
        end
        if isempty(hit)
            mism{end+1} = [vid ':missing_in_run1']; %#ok<AGROW>
        elseif ~strcmp(hit, char(r2.payload_hash))
            mism{end+1} = [vid ':payload_hash_mismatch']; %#ok<AGROW>
        else
            n_cmp = n_cmp + 1;
        end
    end
    consistent = isempty(mism);

    audit = local_read_json_nodir(audit2_path);
    audit.two_run_consistency = struct('enabled', true, ...
        'first_ledger', 'datasets_v3/mission_v3/t429conf_v2/_run_ledger.json', ...
        'second_ledger', ['datasets_v3/mission_v3/t429conf_v2_rerun/', ...
        '_run_ledger.json'], 'voyages_compared', double(n_cmp), ...
        'mismatches', {mism}, 'payload_hash_consistent', ...
        logical(consistent));

    man = local_read_json_nodir(man2_path);
    man.two_run = audit.two_run_consistency;

    local_write_json(audit_out, audit);
    local_write_json(man_out, man);

    out = struct();
    out.consistent = consistent;
    out.voyages_compared = n_cmp;
    out.mismatches = {mism};
    if ~consistent
        error('GATE:MISSION_V3:CONF:TWO_RUN', ...
            'two-run payload hash comparison found %d mismatches', ...
            numel(mism));
    end
    ov2 = audit.overall;
    if strcmp(char(ov2.verdict), 'FAIL')
        error('GATE:MISSION_V3:CONF:INCOMPLETE', ...
            ['second confirmation pass completeness FAIL; final audit ', ...
             'and manifest written (controlled FAIL by design)']);
    end
end

%% ======================================================================
%  summaries, verdict, audit, manifest
%% ======================================================================
function agg = local_overall(scope, strata_audit, voy_c, ctx, run_wall)
    n_env_rej = 0;
    n_cond_rej = 0;
    n_acc_draws = 0;
    n_slots = 0;
    n_voy = 0;
    n_acc_v = 0;
    n_fail_v = 0;
    quota_ok = true;
    for j = 1:numel(strata_audit)
        s = strata_audit{j};
        n_env_rej = n_env_rej + double(s.env_rejected_draws);
        n_cond_rej = n_cond_rej + double(s.cond_rejected_draws);
        n_acc_draws = n_acc_draws + double(s.accepted_draws);
        n_slots = n_slots + double(s.slots_planned);
        n_acc_v = n_acc_v + local_voyages_of(voy_c, char(s.stratum_id), ...
            'ACCEPT');
        n_fail_v = n_fail_v + local_voyages_of(voy_c, ...
            char(s.stratum_id), 'FAILED_GENERATION');
        n_voy = n_voy + double(s.expected_voyages);
        if double(s.accepted_draws) ~= double(s.slots_planned)
            quota_ok = false;
        end
    end
    n_sim = 0;
    for j = 1:numel(voy_c)
        r = voy_c{j};
        if ~strcmp(char(r.disposition), 'FAILED_GENERATION')
            n_sim = n_sim + 1;
        end
    end
    agg = struct();
    agg.scope = scope;
    agg.env_rejected_draws = double(n_env_rej);
    agg.cond_rejected_draws = double(n_cond_rej);
    agg.attempts_total = double(n_env_rej + n_cond_rej + n_acc_draws);
    agg.accepted_draws = double(n_acc_draws);
    agg.slots_planned = double(n_slots);
    agg.voyages_planned = double(n_voy);
    agg.voyages_simulated = double(n_sim);
    agg.voyages_accepted = double(n_acc_v);
    agg.voyages_failed = double(n_fail_v);
    agg.run_wall_total_s = double(run_wall);
    quota_note = ['completeness semantics only: every planned slot ', ...
        'accepted its draw and expanded to the planned member set; no ', ...
        'T-429 result metric is computed or implied'];
    agg.quota_note = quota_note;
    verdict = 'PASS';
    if ~quota_ok || (n_fail_v > 0) || (n_sim ~= n_voy)
        verdict = 'FAIL';
    end
    if strcmp(scope, 'run') && (n_voy ~= ctx.expected_artifacts / ctx.passes)
        verdict = 'FAIL';
    end
    agg.verdict = verdict;
end

function n = local_voyages_of(voy_c, stratum_id, disposition)
    n = 0;
    for j = 1:numel(voy_c)
        r = voy_c{j};
        if strcmp(char(r.stratum_id), stratum_id) ...
                && strcmp(char(r.disposition), disposition)
            n = n + 1;
        end
    end
end

function audit = local_audit(ctx, scope, label, strata_audit, agg, ...
        run_wall, opt)
    am = ctx.amendment;
    am_sh = am.self_hash;
    audit = struct();
    audit.schema = 'mission_v3_t429conf_v2_audit_v1';
    audit.task = ['T429_CONFIRMATION_V2 conditional-sampling audit ', ...
        '(generation-side readiness only, no result metrics)'];
    audit.scope = scope;
    audit.run_label = label;
    audit.generated_at = local_utc_now();
    audit.amendment = struct('rel_path', ctx.amendment_rel, ...
        'amendment_id', char(am.amendment_id), ...
        'self_hash', char(am_sh.value));
    audit.seed_root = ctx.seed_root;
    audit.band_low = local_band_pub(ctx.band_low);
    audit.band_normal = local_band_pub(ctx.band_normal);
    audit.follow_guarantee = struct( ...
        'plateau_fraction', ctx.follow_frac, ...
        'steady_min_s', ctx.steady_min_s, ...
        'source', 'amendment fix3b_traffic_host.duration');
    audit.caps = struct('max_class_attempts', ...
        double(ctx.max_class_attempts), 'retry_max', ...
        double(ctx.retry_max), 'source', ['delegation spec Escalate If ', ...
        'clause; amendment retry_discipline']);
    audit.strata = strata_audit;
    audit.overall = agg;
    audit.resume = logical(opt.resume);
    audit.t429_self_check = local_t429_check(audit);
end

function b = local_band_pub(band)
    b = struct('min', double(band.min), 'max', double(band.max), ...
        'min_inclusive', logical(band.min_inclusive), ...
        'max_inclusive', logical(band.max_inclusive), ...
        'source', char(band.source));
end

function man = local_manifest(ctx, label, led, strata_audit, agg, ...
        run_wall, scope)
    am = ctx.amendment;
    am_sh = am.self_hash;
    cpe = am.confirmation_plan_exact;
    man = struct();
    man.schema = 'mission_v3_t429conf_v2_manifest_v1';
    man.task = ['T429_CONFIRMATION_V2 fresh confirmation corpus ', ...
        'manifest (generation-side readiness only; the corpus manifest ', ...
        'for freeze v2; no result metrics)'];
    man.run_label = label;
    man.scope = scope;
    man.generated_at = local_utc_now();
    man.amendment = struct('rel_path', ctx.amendment_rel, ...
        'amendment_id', char(am.amendment_id), ...
        'self_hash', char(am_sh.value), ...
        'status', 'FROZEN_PREREGISTRATION');
    man.seed_root = ctx.seed_root;
    man.passes = double(ctx.passes);
    man.expected_artifacts = double(ctx.expected_artifacts);
    man.expected_voyages = struct('coverage', ...
        double(cpe.expected_voyages.coverage), 'emergency', ...
        double(cpe.expected_voyages.emergency), 'depth', ...
        double(cpe.expected_voyages.depth), 'traffic', ...
        double(cpe.expected_voyages.traffic), 'total', ...
        double(cpe.expected_voyages.total));
    man.draw_allocation_plan = local_alloc_pub(ctx);
    man.band_low = local_band_pub(ctx.band_low);
    man.band_normal = local_band_pub(ctx.band_normal);
    man.follow_guarantee = struct('plateau_fraction', ctx.follow_frac, ...
        'steady_min_s', ctx.steady_min_s, ...
        'source', 'amendment fix3b_traffic_host.duration');
    man.frozen_chain = ctx.chain;
    man.stack = ctx.stack;
    man.registry_hashes = struct('environment_regime_params', ...
        ctx.hash_env_params, 'identifiability_contrast_registry', ...
        ctx.hash_contrast);

    layers = {};
    layers_all = CONF_LAYERS();
    for li = 1:numel(layers_all)
        layer = layers_all{li};
        lay = local_layer_stats(layer, strata_audit);
        layers{end+1} = lay; %#ok<AGROW>
    end
    man.layers = layers;

    n_sim = 0;
    sim_wall_total = 0;
    vrows = {};
    for j = 1:numel(led.voyages)
        r = led.voyages{j};
        d = char(r.disposition);
        if ~strcmp(d, 'FAILED_GENERATION')
            n_sim = n_sim + 1;
        end
        sw = 0;
        if isfield(r, 'sim_wall_s')
            sw = double(r.sim_wall_s);
        end
        if ~isnan(sw)
            sim_wall_total = sim_wall_total + sw;
        end
        v = struct();
        v.voyage_id = char(r.voyage_id);
        v.stratum_id = char(r.stratum_id);
        v.layer = char(r.layer);
        v.depth_class = char(r.depth_class);
        v.base_draw_index = double(r.base_draw_index);
        v.family_id = char(r.family_id);
        v.split_group_id = char(r.split_group_id);
        v.arms_traffic = char(r.arms_traffic);
        v.arms_forecast = char(r.arms_forecast);
        v.traffic_override = char(r.traffic_override);
        v.disposition = d;
        v.reason = char(r.reason);
        v.detail = char(r.detail);
        v.artifact_rel_path = char(r.artifact_rel);
        v.artifact_sha256 = char(r.artifact_sha256);
        v.payload_hash = char(r.payload_hash);
        v.duration_s = double(r.duration_s);
        v.sim_wall_s = sw;
        v.attempts = double(r.member_attempt);
        vrows{end+1} = v; %#ok<AGROW>
    end
    man.voyages = vrows;
    man.sampling_ledger = local_pack_cell(led.sampling);
    man.retry_ledger = local_pack_cell(led.retry);

    tp = struct();
    tp.voyages_simulated = double(n_sim);
    tp.sim_wall_total_s = double(sim_wall_total);
    tp.run_wall_total_s = double(run_wall);
    if run_wall > 0
        tp.voyages_per_hour = double(n_sim * 3600 / run_wall);
    else
        tp.voyages_per_hour = NaN;
    end
    if n_sim > 0
        tp.mean_sim_wall_s = double(sim_wall_total / n_sim);
    else
        tp.mean_sim_wall_s = NaN;
    end
    man.throughput = tp;
    man.overall = agg;
    man.t429_self_check = local_t429_check(man);
end

function lay = local_layer_stats(layer, strata_audit)
    lay = struct();
    lay.layer = layer;
    n_env = 0;
    n_cond = 0;
    n_acc = 0;
    n_att = 0;
    n_voy = 0;
    for j = 1:numel(strata_audit)
        s = strata_audit{j};
        if strcmp(char(s.layer), layer)
            n_env = n_env + double(s.env_rejected_draws);
            n_cond = n_cond + double(s.cond_rejected_draws);
            n_acc = n_acc + double(s.accepted_draws);
            n_att = n_att + double(s.attempts_total);
            n_voy = n_voy + double(s.expected_voyages);
        end
    end
    lay.attempts_total = double(n_att);
    lay.env_rejected_draws = double(n_env);
    lay.cond_rejected_draws = double(n_cond);
    lay.accepted_draws = double(n_acc);
    lay.expected_voyages = double(n_voy);
    if strcmp(layer, 'depth')
        ht_lo = [];
        ht_hi = [];
        for j = 1:numel(strata_audit)
            s = strata_audit(j);
            if ~strcmp(char(s.layer), 'depth'), continue; end
            if isfield(s, 'class_low')
                cl = s.class_low;
                ht_lo = [ht_lo, local_num_vec(cl.accepted_ht_effective)]; %#ok<AGROW>
                cn = s.class_normal;
                ht_hi = [ht_hi, local_num_vec(cn.accepted_ht_effective)]; %#ok<AGROW>
            end
        end
        lay.depth_quota = struct( ...
            'low_class_accepted_draws', double(numel(ht_lo)), ...
            'normal_class_accepted_draws', double(numel(ht_hi)), ...
            'note', ['frozen plan quota: 12 low-band + 12 normal-band ', ...
            'unique split groups on the full pass']);
    end
    if strcmp(layer, 'traffic')
        ev_counts = [];
        min_plateau = NaN;
        for j = 1:numel(strata_audit)
            s = strata_audit(j);
            if ~strcmp(char(s.layer), 'traffic'), continue; end
            if isfield(s, 'accepted_cruise_event_counts')
                ev_counts = [ev_counts, ...
                    local_num_vec(s.accepted_cruise_event_counts)]; %#ok<AGROW>
                mp = double(s.min_steady_plateau_s);
                if isnan(min_plateau) ...
                        || (~isnan(mp) && mp < min_plateau)
                    min_plateau = mp;
                end
            end
        end
        lay.traffic_quota = struct( ...
            'accepted_draws_with_cruise_event', double(numel(ev_counts)), ...
            'min_steady_plateau_s', double(min_plateau), ...
            'note', ['frozen plan quota: 12 accepted draws each with ', ...
            '>= 1 cruise event and the STEADY_FOLLOW plateau >= 180 s']);
    end
end

function v = local_num_vec(v)
    if isempty(v)
        v = [];
    end
    v = double(v(:)');
end

function ap = local_alloc_pub(ctx)
    ap = struct([]);
    for i = 1:numel(ctx.alloc)
        e = ctx.alloc(i);
        row = struct('stratum_id', char(e.stratum_id), ...
            'stratum_index', double(e.si), 'layer', char(e.layer), ...
            'n_draws', double(e.n_draws), ...
            'members_spec', char(e.members_spec), ...
            'draw_segment', sprintf('[%d, %d)', double(e.draw_start), ...
            double(e.draw_start) + 1000));
        if isempty(ap)
            ap = row;
        else
            ap(end+1) = row; %#ok<AGROW>
        end
    end
    ap = local_pack(ap);
end

%% ======================================================================
%  helpers (prequal producer precedent)
%% ======================================================================
function row = local_blank_row(si, layer, depth_class, st, tpl, reg, ...
        bdi, fam, k, voyage_id)
    mem = fam.members(k);
    arms_v = mem.arms;
    row = struct();
    row.voyage_id = voyage_id;
    row.layer = layer;
    row.depth_class = depth_class;
    row.stratum_id = st;
    row.stratum_index = double(si);
    row.template_id = tpl;
    row.regime_id = reg;
    row.base_draw_index = double(bdi);
    row.family_id = char(fam.family_id);
    row.split_group_id = char(fam.split_group_id);
    row.member_id = char(mem.member_id);
    row.arms_traffic = char(arms_v.traffic);
    row.arms_forecast = char(arms_v.forecast);
    row.traffic_override = char(mem.traffic_override);
    row.member_attempt = 1;
    row.retry_of = 0;
    row.disposition = '';
    row.reason = '';
    row.detail = '';
    row.duration_s = NaN;
    row.n_stream = 0;
    row.n_kf_rows = 0;
    row.sim_wall_s = NaN;
    row.member_wall_s = NaN;
    row.artifact_rel = '';
    row.artifact_sha256 = '';
    row.payload_hash = '';
end

function sel = local_grid1hz(t)
    tt = double(t(:)) * 100;
    tt_r = round(tt);
    sel = (abs(tt - tt_r) <= 1e-6) & (mod(tt_r, 100) == 0);
end

function sm = local_stream_map(ctx)
    sm = struct();
    schema = ctx.schema;
    fams = schema.capability_families;
    tstr = fams.truth_stream;
    fl = local_struct_cell(tstr.fields);
    sigreg = ctx.sigreg;
    sigs = local_struct_cell(sigreg.signals);
    for i = 1:numel(fl)
        f = fl{i};
        if ~isfield(f, 'required') || ~logical(f.required)
            continue;
        end
        nm = char(f.name);
        if strcmp(nm, 'time_s')
            continue;
        end
        tyn = char(f.type);
        if ~isempty(strfind(tyn, 'string|int'))
            % scalar id field (scenario_id); filled at member assembly
            continue;
        end
        src = '';
        for j = 1:numel(sigs)
            sg = sigs{j};
            if strcmp(char(sg.name), nm)
                src = char(sg.source_signal);
                break;
            end
        end
        if isempty(src)
            error('GATE:MISSION_V3:CONF:STREAM', ...
                'signal registry has no source for required field %s', nm);
        end
        row = struct('source', src);
        sm.(nm) = row;
    end
end

function h = local_payload_hash(stream, kf, elig)
    lines = {'MISSION_V3_CONFIRM_PAYLOAD_V1'};
    lines = local_ser('stream', stream, lines);
    lines = local_ser('kf', kf, lines);
    lines{end+1} = ['elig.disposition=' char(elig.disposition)]; %#ok<AGROW>
    lines{end+1} = ['elig.reason=' char(elig.reason)]; %#ok<AGROW>
    lines{end+1} = ['elig.token_id=' char(elig.token_id)]; %#ok<AGROW>
    h = local_sha256_text(strjoin(lines, newline));
end

function lines = local_ser(name, val, lines)
    if isstruct(val)
        fn = fieldnames(val);
        fnc = strjoin(fn.', ',');
        lines{end+1} = sprintf('%s struct n=%d fields=[%s]', name, ...
            numel(val), fnc); %#ok<AGROW>
        for i = 1:numel(val)
            el = val(i);
            for j = 1:numel(fn)
                f = fn{j};
                lines = local_ser([name '.' f], el.(f), lines);
            end
        end
    elseif iscell(val)
        lines{end+1} = sprintf('%s cell n=%d', name, numel(val)); %#ok<AGROW>
        for i = 1:numel(val)
            lines = local_ser(sprintf('%s{%d}', name, i), val{i}, ...
                lines);
        end
    elseif islogical(val) || isnumeric(val)
        v = double(val(:)');
        lines{end+1} = sprintf('%s num %dx%d', name, size(val, 1), ...
            size(val, 2)); %#ok<AGROW>
        lines{end+1} = sprintf('%.17g ', v); %#ok<AGROW>
    elseif ischar(val)
        lines{end+1} = sprintf('%s char %s', name, val); %#ok<AGROW>
    else
        error('GATE:MISSION_V3:CONF:SER', ...
            'unsupported payload class at %s', name);
    end
end

function ok = local_t429_check(obj)
    blacklist = {'sbd', 'ba', 'confidence', 'confusion', 'accuracy', ...
        'precision', 'recall', 'f1', 'auc', 'roc'};
    names = local_walk_names(obj);
    for i = 1:numel(names)
        nm = lower(char(names{i}));
        for j = 1:numel(blacklist)
            pat = ['(^|_)' blacklist{j} '($|_)'];
            if ~isempty(regexp(nm, pat, 'once'))
                error('GATE:MISSION_V3:CONF:T429', ...
                    'forbidden result-metric field name: %s', names{i});
            end
        end
    end
    ok = true;
end

function names = local_walk_names(obj)
    names = {};
    if isstruct(obj)
        fn = fieldnames(obj);
        for i = 1:numel(fn)
            names{end+1} = fn{i}; %#ok<AGROW>
        end
        for i = 1:numel(obj)
            el = obj(i);
            for j = 1:numel(fn)
                sub = local_walk_names(el.(fn{j}));
                names = [names, sub]; %#ok<AGROW>
            end
        end
    elseif iscell(obj)
        for i = 1:numel(obj)
            sub = local_walk_names(obj{i});
            names = [names, sub]; %#ok<AGROW>
        end
    end
end

function c = local_struct_cell(x)
% jsondecode returns arrays of JSON objects as struct arrays only when
% every element shares the same field set; otherwise a cell array of
% structs.  Normalize both shapes to a cell of scalar structs.
    c = {};
    if isstruct(x)
        for i = 1:numel(x)
            c{end+1} = x(i); %#ok<AGROW>
        end
    elseif iscell(x)
        for i = 1:numel(x)
            v = x{i};
            if isstruct(v)
                c{end+1} = v; %#ok<AGROW>
            end
        end
    end
end

function local_flush(ledger_path, led)
    try
        local_write_json(ledger_path, struct('voyages', {led.voyages}, ...
            'sampling', {led.sampling}, 'retry', {led.retry}));
    catch
        % incremental flush is best-effort; the final write is authoritative
    end
end

function c = local_cell_of(v)
    c = {};
    if isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    elseif iscell(v)
        c = v;
    end
end

function c = local_pack(v)
    c = {};
    if isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    end
end

function c = local_pack_cell(v)
    c = {};
    if iscell(v)
        c = v;
    elseif isstruct(v)
        for i = 1:numel(v)
            c{end+1} = v(i); %#ok<AGROW>
        end
    end
end

function c = local_cellstr_of(v)
    c = {};
    if iscell(v)
        for i = 1:numel(v)
            c{end+1} = char(v{i}); %#ok<AGROW>
        end
    elseif isstring(v)
        for i = 1:numel(v)
            c{end+1} = char(v(i)); %#ok<AGROW>
        end
    elseif ischar(v)
        c{1} = char(v);
    else
        error('GATE:MISSION_V3:CONF:ARG', 'expected a cellstr value');
    end
end

function ov = local_parse_named(arg_cell)
    ov = struct();
    k = 1;
    while k <= numel(arg_cell)
        nm = arg_cell{k};
        if ~(ischar(nm) || isstring(nm)) || k + 1 > numel(arg_cell)
            error('GATE:MISSION_V3:CONF:ARG', ...
                ['name-value arguments expected (got numel=%d, ', ...
                 'element %d class=%s)'], numel(arg_cell), k, class(nm));
        end
        ov.(char(nm)) = arg_cell{k+1};
        k = k + 2;
    end
end

function id = local_err_id(ME)
    if isfield(ME, 'identifier') && ~isempty(ME.identifier)
        id = char(ME.identifier);
    else
        id = 'UNKNOWN_ERROR';
    end
end

function d = local_fail_detail(ME)
    msg = char(ME.message);
    nl = strfind(msg, newline);
    if ~isempty(nl)
        msg = msg(1:nl(1)-1);
    end
    if numel(msg) > 160
        msg = [msg(1:160) '...'];
    end
    d = [local_err_id(ME) ': ' msg];
end

function local_reraise_hard(ME)
    id = local_err_id(ME);
    if ~isempty(id) && strncmp(id, 'GATE:', 5)
        rethrow(ME);
    end
end

function s = local_utc_now()
    s = char(datetime('now', 'TimeZone', 'UTC', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
end

function obj = local_read_json_nodir(path)
    txt = fileread(path);
    obj = jsondecode(txt);
end

function obj = local_read_json(p1, varargin)
    if nargin >= 3
        p = fullfile(p1, varargin{1}, varargin{2});
    else
        p = fullfile(p1, varargin{1});
    end
    txt = fileread(p);
    obj = jsondecode(txt);
end

function local_write_json(path, obj)
    local_t429_check(obj);
    txt = jsonencode(obj, 'PrettyPrint', true);
    tmp = [path '.tmp'];
    fid = fopen(tmp, 'w');
    if fid < 0
        error('GATE:MISSION_V3:CONF:IO', ...
            'cannot open %s for writing', tmp);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
    movefile(tmp, path, 'f');
end

function h = local_sha256_text(s)
    bytes = unicode2native(s, 'UTF-8');
    h = local_sha256_bytes(bytes);
end

function h = local_sha256_file(path)
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:MISSION_V3:CONF:IO', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    h = local_sha256_bytes(raw);
end

function h = local_sha256_bytes(bytes)
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(bytes(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end
