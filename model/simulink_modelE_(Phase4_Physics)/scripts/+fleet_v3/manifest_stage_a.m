function res = manifest_stage_a(plan, out_iid, out_ood)
% fleet_v3.manifest_stage_a - Stage A summary manifest for the 73
% pipeline (T-018 Phase 4-D; D5 readiness face, no result metrics).
%
%   res = manifest_stage_a(plan, out_iid, out_ood)
%
% Contents: per-corpus voyage/window/disposition counts, rejection
% rates, OOD smoke hit statistics (in-band hit rate, REQUIRE
% reachability, STRESS_SET removal count, C3 feasibility - FR-545
% evidence), the frozen provenance hashes (fleet_v3.plan_load), and
% throughput.  Written to results/dataset_v3/qualification/
% stage_a_manifest.json (stage_a) or results/dataset_v3/
% qualification_smoke/smoke_manifest.json (smoke); the JSON passes a
% T429-style result-metric field-name blacklist self-check before every
% write (C-side precedent).
%
% A T429-style blacklist check runs before the write; any hit raises
% GATE:FLEETV3:MANIFEST:T429.
%
% Error ids: GATE:FLEETV3:MANIFEST:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions.

    if ~(isstruct(plan) && isscalar(plan))
        error('GATE:FLEETV3:MANIFEST:ARG', 'plan must be a scalar struct');
    end

    man = struct();
    man.schema = 'fleet_v3_stage_a_manifest_v1';
    man.task = 'T-018 Phase 4-D Stage A fleet qualification manifest';
    man.run_mode = char(plan.mode);
    man.generated_at = local_utc_now();
    man.disposable = logical(plan.stage_a_disposable);

    man.activation = struct('rel_path', plan.activation_rel, ...
        'sha256', char(plan.provenance.activation_sha256), ...
        'status', char(plan.activation.status));
    man.provenance = struct( ...
        'activation_sha256', char(plan.provenance.activation_sha256), ...
        'ood_override_sha256', ...
        char(plan.provenance.ood_override_sha256), ...
        'heldout_token_registry_sha', ...
        char(plan.provenance.heldout_registry_sha), ...
        'ood_def_version', char(plan.ood.def_version), ...
        'seed_root', char(plan.iid.seed), ...
        'note', ['D5: the OOD configuration carries its own frozen ', ...
        'provenance (override config serialization + canonical token ', ...
        'registry self_hash + activation record file hash)']);

    man.qualification_criteria = plan.qual;
    man.iid_gates = struct('per_stratum_max', plan.iid.thr_stratum, ...
        'overall_max', plan.iid.thr_overall, ...
        'retry_max', plan.iid.retry_max);

    % ---- IID corpus summary ----
    iid_sum = struct();
    iid_sum.corpus = 'IID';
    iid_sum.seed = out_iid.seed;
    iid_sum.draw_base = out_iid.draw_base;
    iid_sum.draws = out_iid.draws;
    iid_sum.verdict = char(out_iid.verdict);
    iid_sum.rejection_rate = out_iid.rejection_rate;
    iid_sum.strata = local_pack(out_iid.strata);
    iid_sum.voyages = local_voyage_rows(out_iid.voyages);
    iid_sum.n_voyages = double(numel(out_iid.voyages));
    iid_sum.n_accepted = local_count_disp(out_iid.voyages, 'ACCEPT');
    iid_sum.n_rejected = local_count_disp(out_iid.voyages, 'REJECTED');
    iid_sum.n_failed = local_count_disp(out_iid.voyages, ...
        'FAILED_GENERATION');
    man.iid = iid_sum;

    % ---- OOD corpus summary (hit statistics; FR-545 evidence) ----
    ood_sum = struct();
    ood_sum.corpus = 'OOD';
    ood_sum.seed = out_ood.seed;
    ood_sum.draw_base = out_ood.draw_base;
    ood_sum.def_version = char(plan.ood.def_version);
    ood_sum.conditions = local_pack(out_ood.conditions);
    ood_sum.voyages = local_voyage_rows(out_ood.voyages);
    ood_sum.n_voyages = double(numel(out_ood.voyages));
    ood_sum.n_accepted = local_count_disp(out_ood.voyages, 'ACCEPT');
    ood_sum.n_rejected = local_count_disp(out_ood.voyages, 'REJECTED');
    ood_sum.n_failed = local_count_disp(out_ood.voyages, ...
        'FAILED_GENERATION');
    ood_sum.n_stress_set_removed = out_ood.n_stress_removed;
    ood_sum.stress_rows = local_pack_cell(out_ood.stress_rows);
    ood_sum.c3_feasibility = local_c3_evidence(out_ood);
    man.ood = ood_sum;

    % ---- throughput ----
    tp = struct();
    n_sim = iid_sum.n_voyages - iid_sum.n_failed + ood_sum.n_voyages ...
        - ood_sum.n_failed;
    wall = out_iid.run_wall_s + out_ood.run_wall_s;
    tp.voyages_simulated = double(n_sim);
    tp.run_wall_total_s = double(wall);
    if wall > 0
        tp.voyages_per_hour = double(n_sim * 3600 / wall);
    else
        tp.voyages_per_hour = NaN;
    end
    man.throughput = tp;

    man.t429_self_check = true;
    local_t429_check(man);

    if strcmp(char(plan.mode), 'smoke')
        res_dir = fullfile(plan.modele_root, 'results', 'dataset_v3', ...
            'qualification_smoke');
        out_path = fullfile(res_dir, 'smoke_manifest.json');
    else
        res_dir = fullfile(plan.modele_root, 'results', 'dataset_v3', ...
            'qualification');
        out_path = fullfile(res_dir, 'stage_a_manifest.json');
    end
    if ~exist(res_dir, 'dir')
        mkdir(res_dir);
    end
    txt = jsonencode(local_jsonable(man), 'PrettyPrint', true);
    tmp = [out_path '.tmp'];
    fid = fopen(tmp, 'w');
    if fid < 0
        error('GATE:FLEETV3:MANIFEST:IO', ...
            'cannot open %s for writing', tmp);
    end
    fwrite(fid, txt, 'char');
    fclose(fid);
    movefile(tmp, out_path, 'f');

    res = struct();
    res.manifest_path = out_path;
    res.manifest = man;
    res.iid_verdict = iid_sum.verdict;
    res.ood_conditions_accepted = double(numel(out_ood.conditions));
end

%% ========================== local helpers ===========================

function rows = local_voyage_rows(voy_c)
    rows = {};
    for j = 1:numel(voy_c)
        r = voy_c{j};
        v = struct();
        v.voyage_id = char(r.voyage_id);
        v.base_draw_index = double(r.base_draw_index);
        v.family_id = char(r.family_id);
        v.split_group_id = char(r.split_group_id);
        if isfield(r, 'stratum_id')
            v.stratum_id = char(r.stratum_id);
        end
        v.disposition = char(r.disposition);
        v.reason = char(r.reason);
        v.artifact_rel_path = char(r.artifact_rel);
        v.artifact_sha256 = char(r.artifact_sha256);
        v.payload_hash = char(r.payload_hash);
        v.duration_s = double(r.duration_s);
        v.attempts = double(r.member_attempt);
        if isfield(r, 'ood_axis')
            v.ood_axis = char(r.ood_axis);
            v.ood_condition_id = char(r.ood_condition_id);
            v.ood_token_id = char(r.ood_token_id);
        end
        rows{end+1} = v; %#ok<AGROW>
    end
end

function n = local_count_disp(voy_c, prefix)
    n = 0;
    for j = 1:numel(voy_c)
        d = char(voy_c{j}.disposition);
        if strncmp(d, prefix, numel(prefix))
            n = n + 1;
        end
    end
end

function e = local_c3_evidence(out_ood)
% FR-545: rush segments at low H/T must show nonzero feasible support.
    e = struct('condition', 'C3', 'accepted', false, ...
        'attempts_used', NaN, 'propulsion_feasible', false, ...
        'sup_abs', NaN, 'ht_effective', NaN);
    conds = out_ood.conditions;
    for i = 1:numel(conds)
        if strcmp(char(conds(i).condition_id), 'C3')
            e.accepted = logical(conds(i).accepted);
            e.attempts_used = double(conds(i).attempts_used);
            e.sup_abs = double(conds(i).sup_abs);
            e.ht_effective = double(conds(i).ht_effective);
            e.propulsion_feasible = e.accepted;
        end
    end
end

function local_t429_check(obj)
    blacklist = {'sbd', 'ba', 'confidence', 'confusion', 'accuracy', ...
        'precision', 'recall', 'f1', 'auc', 'roc'};
    names = local_walk_names(obj);
    for i = 1:numel(names)
        nm = lower(char(names{i}));
        for j = 1:numel(blacklist)
            pat = ['(^|_)' blacklist{j} '($|_)'];
            if ~isempty(regexp(nm, pat, 'once'))
                error('GATE:FLEETV3:MANIFEST:T429', ...
                    'forbidden result-metric field name: %s', names{i});
            end
        end
    end
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

function s = local_jsonable(s)
% Normalize for jsonencode: empty struct arrays encode cleanly as {}
% (empty cells); everything else (uniform struct arrays, cells, plain
% values) is already valid jsonencode input.
    if isstruct(s)
        fn = fieldnames(s);
        for i = 1:numel(s)
            el = s(i);
            for j = 1:numel(fn)
                v = el.(fn{j});
                if isstruct(v) && isempty(v)
                    el.(fn{j}) = {};
                else
                    el.(fn{j}) = local_jsonable(v);
                end
            end
            tmp(i) = el; %#ok<AGROW>
        end
        if isempty(s)
            s = {};
        else
            s = tmp;
        end
    elseif iscell(s)
        for i = 1:numel(s)
            s{i} = local_jsonable(s{i});
        end
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

function s = local_utc_now()
    s = char(datetime('now', 'TimeZone', 'UTC', ...
        'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
end
