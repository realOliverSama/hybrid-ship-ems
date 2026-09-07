function res = predicate_eval(token, facts)
% fleet_v3.predicate_eval - canonical heldout-token predicate evaluation
% for the OOD REQUIRE face (T-018 Phase 4-D; D4 ruling: thresholds are
% consumed from the frozen config/mission_v3/heldout_token_registry.json,
% never hand-copied).
%
%   res = fleet_v3.predicate_eval(token, facts)
%
% token is ONE token entry of the frozen registry (a struct with
% token_id / predicate / support_band / ood_action as decoded from the
% registry JSON).  facts carries the realized generation-side metadata:
%   facts.segment_roles      cellstr, one entry per realized segment
%   facts.seg_u_adverse_abs  double vector, per-segment adverse sup
%                            (env_realize realization.segments
%                            (i).u_adverse_abs_sup)
%   facts.ht_effective       realized voyage H/T scalar
%                            ((h_chart + delta_wl) / draft)
%
% The predicate grammar is the registry's own two-clause form (the same
% grammar env_realize consumes on the IID EXCLUDE face):
%   clause 1: field 'segment.role', operator '=='      (role equality)
%   clause 2: field 'segment.u_adverse_abs' or 'voyage.ht_effective',
%             operator 'in_band' over the token's support_band
% The field whitelist is exactly these three fields; anything else fails
% closed (GATE:FLEETV3:PRED:FIELD), mirroring the env-layer guard.
%
% res.triggered  logical
% res.token_id   char (echoed from the registry entry)
% res.evidence   struct with the matching segment index (0 = voyage
%                scalar) and the evaluated numeric value
%
% Pure function; no file access (the caller passes the decoded entry);
% error ids: GATE:FLEETV3:PRED:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; thin body + local
% functions.

    if ~(isstruct(token) && isscalar(token))
        error('GATE:FLEETV3:PRED:ARG', ...
            'token must be a scalar registry token struct');
    end
    need_t = {'token_id', 'predicate', 'support_band'};
    for i = 1:numel(need_t)
        if ~isfield(token, need_t{i})
            error('GATE:FLEETV3:PRED:ARG', ...
                'token lacks the registry field %s', need_t{i});
        end
    end
    if ~(isstruct(facts) && isscalar(facts))
        error('GATE:FLEETV3:PRED:ARG', ...
            'facts must be a scalar struct');
    end
    need_f = {'segment_roles', 'seg_u_adverse_abs', 'ht_effective'};
    for i = 1:numel(need_f)
        if ~isfield(facts, need_f{i})
            error('GATE:FLEETV3:PRED:ARG', ...
                'facts lacks %s', need_f{i});
        end
    end
    roles = facts.segment_roles;
    if ~(iscell(roles) || isstring(roles) || ischar(roles))
        error('GATE:FLEETV3:PRED:ARG', ...
            'facts.segment_roles must be a cellstr');
    end
    if ischar(roles)
        roles = {roles};
    elseif isstring(roles)
        roles = cellstr(roles);
    end
    n_seg = numel(roles);
    su = facts.seg_u_adverse_abs;
    if ~isnumeric(su) || ~isvector(su) || (numel(su) ~= n_seg)
        error('GATE:FLEETV3:PRED:ARG', ...
            'facts.seg_u_adverse_abs must align with segment_roles');
    end
    ht = facts.ht_effective;
    if ~isnumeric(ht) || ~isscalar(ht) || ~isfinite(ht)
        error('GATE:FLEETV3:PRED:ARG', ...
            'facts.ht_effective must be a finite scalar');
    end

    pred = local_as_list(token.predicate, 'predicate');
    if numel(pred) ~= 2
        error('GATE:FLEETV3:PRED:GRAMMAR', ...
            'token %s: expected exactly two predicate clauses', ...
            char(token.token_id));
    end
    p1 = pred{1};
    p2 = pred{2};

    % clause 1 must be the segment-role equality (registry grammar)
    if ~strcmp(char(p1.field), 'segment.role') ...
            || ~strcmp(char(p1.operator), '==')
        error('GATE:FLEETV3:PRED:GRAMMAR', ...
            ['token %s: clause 1 must be the segment-role equality ', ...
             '(registry grammar)'], char(token.token_id));
    end
    role_v = char(p1.value);

    % clause 2: whitelisted numeric factor + registry band
    fld = char(p2.field);
    if ~strcmp(char(p2.operator), 'in_band')
        error('GATE:FLEETV3:PRED:GRAMMAR', ...
            'token %s: clause 2 operator must be in_band', ...
            char(token.token_id));
    end
    band = token.support_band;
    need_b = {'min', 'min_inclusive', 'max', 'max_inclusive'};
    for i = 1:numel(need_b)
        if ~isfield(band, need_b{i})
            error('GATE:FLEETV3:PRED:BAND', ...
                'token %s: support_band lacks %s', char(token.token_id), ...
                need_b{i});
        end
    end
    switch fld
        case 'segment.u_adverse_abs'
            per_seg = true;
        case 'voyage.ht_effective'
            per_seg = false;
        otherwise
            error('GATE:FLEETV3:PRED:FIELD', ...
                ['token %s: predicate field not in the whitelist ', ...
                 '(segment.role | segment.u_adverse_abs | ', ...
                 'voyage.ht_effective): %s'], char(token.token_id), fld);
    end

    res = struct();
    res.token_id = char(token.token_id);
    res.triggered = false;
    res.evidence = struct('segment_index', 0, 'value', NaN, ...
        'role', '');
    if per_seg
        for i = 1:n_seg
            if strcmp(char(roles{i}), role_v) ...
                    && local_in_band(double(su(i)), band)
                res.triggered = true;
                res.evidence.segment_index = i;
                res.evidence.value = double(su(i));
                res.evidence.role = char(roles{i});
                return;
            end
        end
    else
        if local_in_band(double(ht), band)
            for i = 1:n_seg
                if strcmp(char(roles{i}), role_v)
                    res.triggered = true;
                    res.evidence.segment_index = i;
                    res.evidence.value = double(ht);
                    res.evidence.role = char(roles{i});
                    return;
                end
            end
        end
    end
end

function tf = local_in_band(v, band)
    if ~isfinite(v) || isnan(v)
        tf = false;
        return;
    end
    ok_min = (v > double(band.min)) ...
        || (logical(band.min_inclusive) && v == double(band.min));
    ok_max = (v < double(band.max)) ...
        || (logical(band.max_inclusive) && v == double(band.max));
    tf = ok_min && ok_max;
end

function lst = local_as_list(v, what)
    if iscell(v)
        lst = v(:)';
    elseif isstruct(v)
        n = numel(v);
        lst = cell(1, n);
        for k = 1:n
            lst{k} = v(k);
        end
    else
        error('GATE:FLEETV3:PRED:ARG', ...
            'expected a JSON array for %s', what);
    end
end
