function res = export_ood(out_file, ood_extra)
% fleet_v3.export_ood - OOD meta extension for a bridged artifact
% (T-018 Phase 4-D; D5 OOD provenance face).
%
%   res = fleet_v3.export_ood(out_file, ood_extra)
%
% The frozen C-side producer_export face validates
% meta.dataset_domain against the frozen producer schema enum
% {FORMAL, PILOT} only, so the OOD record cannot pass through it
% directly.  Mechanism (the load-append option of the delegation
% ruling; producer_export itself is NOT modified):
%   1. member_exec exports the artifact through the untouched
%      producer_export face with the bridge domain FORMAL (fixture
%      NONE, full schema + FR-500 + FR-536 eligibility checks all
%      executed on the real content);
%   2. this function loads the artifact, applies the OOD meta
%      extension, rewrites the MAT (-v7, the frozen D-2 format value)
%      and re-verifies the {stream, kf, meta} round trip field by
%      field.
%
% Applied meta fields (D5 ood_metadata_fields + T-018 additions):
%   dataset_domain               'OOD'
%   ood_axis                     'A' | 'B' | 'C' (already bridged in)
%   ood_condition_id             'A1'..'C3'
%   ood_token_id                 canonical token id ('' when not Axis C)
%   ood_def_version              'DGN-11-v1' (already present)
%   physics_domain_ref           already present (schema family)
%   heldout_token_registry_sha   frozen registry self_hash (D4)
%   ood_judge_reason             fleet_v3.ood_judge reason text
%   propulsion_check             feasibility struct (700 kW guard)
%
% res echoes the exporter result shape (n_stream_samples / n_kf_rows /
% roundtrip_ok) so member_exec can consume either exporter uniformly.
%
% Error ids: GATE:FLEETV3:EXPORT:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths.

    if ~(isstruct(ood_extra) && isscalar(ood_extra))
        error('GATE:FLEETV3:EXPORT:ARG', ...
            'ood_extra must be a scalar struct');
    end
    need = {'ood_axis', 'ood_condition_id', 'ood_token_id', ...
        'heldout_token_registry_sha'};
    for i = 1:numel(need)
        if ~isfield(ood_extra, need{i})
            error('GATE:FLEETV3:EXPORT:ARG', ...
                'ood_extra lacks %s', need{i});
        end
    end

    s = load(out_file);
    if ~isfield(s, 'stream') || ~isfield(s, 'kf') ...
            || ~isfield(s, 'meta')
        error('GATE:FLEETV3:EXPORT:LAYOUT', ...
            'bridged artifact %s lacks {stream, kf, meta}', out_file);
    end
    m = s.meta;
    m.dataset_domain = 'OOD';
    m.ood_axis = char(ood_extra.ood_axis);
    m.ood_condition_id = char(ood_extra.ood_condition_id);
    m.ood_token_id = char(ood_extra.ood_token_id);
    m.heldout_token_registry_sha = ...
        char(ood_extra.heldout_token_registry_sha);
    if isfield(ood_extra, 'ood_judge_reason')
        m.ood_judge_reason = char(ood_extra.ood_judge_reason);
    end
    if isfield(ood_extra, 'propulsion')
        m.propulsion_check = ood_extra.propulsion;
    end

    stream = s.stream;
    kf = s.kf;
    meta = m;
    save(out_file, 'stream', 'kf', 'meta', '-v7');

    % self-check read-back (field-level, exporter precedent)
    winfo = whos('-file', out_file);
    names = sort({winfo.name});
    if ~isequal(names, sort({'stream', 'kf', 'meta'}))
        error('GATE:FLEETV3:EXPORT:ROUNDTRIP', ...
            'file %s does not hold exactly {stream, kf, meta}', out_file);
    end
    s2 = load(out_file);
    if ~isequal(s2.stream, stream) || ~isequal(s2.kf, kf) ...
            || ~isequal(s2.meta, meta)
        error('GATE:FLEETV3:EXPORT:ROUNDTRIP', ...
            'read-back of %s differs from the OOD-extended artifact', ...
            out_file);
    end
    if ~strcmp(char(s2.meta.dataset_domain), 'OOD')
        error('GATE:FLEETV3:EXPORT:ROUNDTRIP', ...
            'dataset_domain did not persist as OOD in %s', out_file);
    end

    res = struct();
    res.ok = true;
    res.file = char(out_file);
    res.dataset_domain = 'OOD';
    res.n_stream_samples = numel(stream.time_s);
    res.n_kf_rows = numel(kf.issue_time_s);
    res.roundtrip_ok = true;
end
