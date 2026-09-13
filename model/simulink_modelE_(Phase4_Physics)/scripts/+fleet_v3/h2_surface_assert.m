function h2_surface_assert(sm, schema, sigreg)
% fleet_v3.h2_surface_assert - fail-closed H2 / Dataset export-surface
% invariant (T432_ACTIVATION_AMENDMENT_02, Final Ruling section 3).
%
%   fleet_v3.h2_surface_assert(sm, schema, sigreg)
%
%   sm      realized stream_map struct (ctx_build local_stream_map
%           output: exactly one field per realized stream column)
%   schema  jsondecode of config/dataset_v3_producer_schema.json
%   sigreg  jsondecode of config/dataset_v3_signal_registry.json
%
% Enforced ruling equivalences (dual-fact-source defect guard: the
% formal export surface derives from the schema, the pilot surface from
% the registry - this invariant pins the two together):
%
%   (1) formal_export_fields == schema_required_dataset_fields
%       fieldnames(sm) must EQUAL the schema truth_stream required set
%       minus time_s minus scalar string|int id fields (exactly the
%       ctx_build local_stream_map derivation) - no extra column and
%       no missing column.
%   (2) schema_H2_fields == registry_H2_fields_marked_for_dataset_
%       retention: every registry INPUT_H2 name must be BOTH a
%       schema truth_stream required field AND a realized stream_map
%       column (one missing H2 column fails).
%
% Violation -> error('GATE:FLEETV3:CTX:H2SURFACE', ...) with the
% missing/extra column names listed (machine-readable).
%
% Pure function: no side effects, no file I/O, no model dependency -
% unit tests call it directly on parsed JSON fixtures.
%
% Discipline: ASCII-only source.

    if ~(isstruct(sm) && isscalar(sm))
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'stream_map must be a scalar struct');
    end
    if ~(isstruct(schema) && isscalar(schema))
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'schema must be a parsed producer schema struct');
    end
    if ~(isstruct(sigreg) && isscalar(sigreg))
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'sigreg must be a parsed signal registry struct');
    end

    % ---- fact extraction -------------------------------------------------
    h2_reg = local_h2_names(sigreg);        % registry INPUT_H2 names
    [req_names, req_types, req_required] = local_truth_fields(schema);
    req_names = req_names(req_required);
    req_types = req_types(req_required);

    sm_names = fieldnames(sm);

    % ---- assertion 1: registry H2 retention obligations all realized ----
    miss_in_schema = local_setdiff(h2_reg, req_names);
    miss_in_sm = local_setdiff(h2_reg, sm_names);
    if ~isempty(miss_in_schema) || ~isempty(miss_in_sm)
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            ['H2 dataset-retention surface broken (AMENDMENT_02): ' ...
            'registry INPUT_H2 (%d cols) missing from schema ' ...
            'truth_stream required: [%s]; missing from realized ' ...
            'stream_map: [%s]'], numel(h2_reg), ...
            local_join(miss_in_schema), local_join(miss_in_sm));
    end

    % ---- assertion 2: export surface == schema required Dataset face ----
    % The schema required Dataset face mirrors the ctx_build
    % local_stream_map derivation: drop time_s (time base, realized
    % separately) and scalar string|int id fields (member assembly).
    keep = true(numel(req_names), 1);
    keep(strcmp(req_names, 'time_s')) = false;
    keep(~cellfun(@isempty, strfind(req_types, 'string|int'))) = false;
    exp_face = req_names(keep);

    sm_face = sm_names;
    sm_face(strcmp(sm_face, 'time_s')) = [];  % defensive (ctx_build
    % never realizes time_s into the stream_map; drop if present)

    extra = local_setdiff(sm_face, exp_face);
    missing = local_setdiff(exp_face, sm_face);
    if ~isempty(extra) || ~isempty(missing)
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            ['formal export surface drifted from schema required ' ...
            'Dataset face (AMENDMENT_02): stream_map extra cols: ' ...
            '[%s]; stream_map missing cols: [%s] (schema required ' ...
            'Dataset face has %d cols)'], ...
            local_join(extra), local_join(missing), numel(exp_face));
    end
end

function names = local_h2_names(sigreg)
% registry INPUT_H2 name set (the FR-461 Dataset retention obligation).
    names = {};
    sigs = sigreg.signals;
    if isstruct(sigs)
        for i = 1:numel(sigs)
            if strcmp(char(sigs(i).role), 'INPUT_H2')
                names{end + 1} = char(sigs(i).name); %#ok<AGROW>
            end
        end
    elseif iscell(sigs)
        for i = 1:numel(sigs)
            if strcmp(char(sigs{i}.role), 'INPUT_H2')
                names{end + 1} = char(sigs{i}.name); %#ok<AGROW>
            end
        end
    else
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'sigreg.signals must be an array of signal objects');
    end
    if isempty(names)
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'signal registry declares zero INPUT_H2 columns');
    end
end

function [names, types, required] = local_truth_fields(schema)
% schema truth_stream field parallel arrays (name/type/required).
    fams = schema.capability_families;
    if ~isfield(fams, 'truth_stream')
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'producer schema has no truth_stream capability family');
    end
    fl = fams.truth_stream.fields;
    names = {};
    types = {};
    required = logical([]);
    if isstruct(fl)
        for i = 1:numel(fl)
            names{end + 1} = char(fl(i).name); %#ok<AGROW>
            if isfield(fl, 'type')
                types{end + 1} = char(fl(i).type); %#ok<AGROW>
            else
                types{end + 1} = ''; %#ok<AGROW>
            end
            if isfield(fl, 'required') && ~isempty(fl(i).required)
                required(end + 1) = logical(fl(i).required); %#ok<AGROW>
            else
                required(end + 1) = false; %#ok<AGROW>
            end
        end
    elseif iscell(fl)
        for i = 1:numel(fl)
            names{end + 1} = char(fl{i}.name); %#ok<AGROW>
            types{end + 1} = char(fl{i}.type); %#ok<AGROW>
            required(end + 1) = logical(fl{i}.required); %#ok<AGROW>
        end
    else
        error('GATE:FLEETV3:CTX:H2SURFACE', ...
            'truth_stream.fields must be an array of field objects');
    end
end

function d = local_setdiff(a, b)
% order-stable set difference a \ b for cellstr
    d = a(~ismember(a, b));
end

function s = local_join(list)
% cellstr -> 'a, b, c' (empty -> '')
    if isempty(list)
        s = '';
    else
        s = strjoin(list, ', ');
    end
end
