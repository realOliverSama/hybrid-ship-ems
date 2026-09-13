%% test_h2_surface.m - fleet_v3 H2/Dataset export-surface invariant suite
% (T432_ACTIVATION_AMENDMENT_02, Final Ruling section 3): the formal
% export surface derives from the producer schema while the pilot
% surface derives from the registry full set - the dual-fact-source
% defect.  This suite pins the fail-closed invariant that fuses the two
% fact sources:
%   P  real frozen schema + registry -> ctx_build-equivalent stream_map
%      passes the assertion; ctx_build wiring executes it end-to-end
%   N-A tmp schema copy with one H2 required field (SOC) removed ->
%      GATE:FLEETV3:CTX:H2SURFACE (message names the column)
%   N-B stream_map missing one H2 column -> must fail closed
%   N-C stream_map carrying one column outside the schema required
%      Dataset face -> must fail closed (export-surface drift)
% Failures raise GATE:FLEETV3:TEST:H2SURFACE.
% Discipline: ASCII-only; no absolute paths (tempname artifacts).

ra_root = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
if isempty(ra_root) || ~exist(fullfile(ra_root, 'config'), 'dir')
    ra_root = pwd;
end
addpath(fullfile(ra_root, 'scripts'));

ra_schema = jsondecode(fileread(fullfile(ra_root, 'config', ...
    'dataset_v3_producer_schema.json')));
ra_sigreg = jsondecode(fileread(fullfile(ra_root, 'config', ...
    'dataset_v3_signal_registry.json')));

% ---- P: real frozen schema + registry -> stream_map passes ------------
ra_sm = local_stream_map_like(ra_schema, ra_sigreg);
fleet_v3.h2_surface_assert(ra_sm, ra_schema, ra_sigreg);  % must not throw

ra_h2_names = local_registry_h2(ra_sigreg);
assert(numel(ra_h2_names) == 10, ...
    'GATE:FLEETV3:TEST:H2SURFACE', ...
    'registry INPUT_H2 must declare 10 columns (FR-461)');
assert(all(ismember(ra_h2_names, fieldnames(ra_sm))), ...
    'GATE:FLEETV3:TEST:H2SURFACE', ...
    'derived stream_map must realize every registry INPUT_H2 column');

% ---- P: ctx_build wiring executes the invariant end-to-end ------------
ra_act = fullfile(fileparts(fileparts(ra_root)), 'docs', 'phase4', ...
    'd_dataset_v3', 't432_activation_record.json');
ra_plan = fleet_v3.plan_load(ra_act, 'stage_a');
ra_ctx = fleet_v3.ctx_build(ra_plan, 'ProvisionBase', false);
assert(~ra_ctx.model_ready, 'GATE:FLEETV3:TEST:H2SURFACE', ...
    'ProvisionBase=false must not load the model');
assert(all(ismember(ra_h2_names, fieldnames(ra_ctx.stream_map))), ...
    'GATE:FLEETV3:TEST:H2SURFACE', ...
    'ctx_build stream_map lost an INPUT_H2 column (AMENDMENT_02)');

% ---- N-A: schema copy missing one H2 required field (SOC) -------------
ra_tmp_json = [tempname, '.json'];
ra_cleanup = onCleanup(@() local_safe_delete(ra_tmp_json));
ra_bad_schema = ra_schema;
ra_bad_fams = ra_bad_schema.capability_families;
ra_bad_ts = ra_bad_fams.truth_stream;
ra_keep = ~strcmp(local_field_names(ra_bad_ts.fields), 'SOC');
ra_bad_ts.fields = ra_bad_ts.fields(ra_keep);
ra_bad_fams.truth_stream = ra_bad_ts;
ra_bad_schema.capability_families = ra_bad_fams;
fid = fopen(ra_tmp_json, 'w');
if fid < 0
    error('GATE:FLEETV3:TEST:H2SURFACE', ...
        'cannot write tmp schema fixture');
end
fwrite(fid, jsonencode(ra_bad_schema));
fclose(fid);
ra_bad_schema = jsondecode(fileread(ra_tmp_json));
ra_threw = false;
try
    fleet_v3.h2_surface_assert(ra_sm, ra_bad_schema, ra_sigreg);
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:CTX:H2SURFACE') ...
        && ~isempty(strfind(char(ME.message), 'SOC'));
end
assert(ra_threw, 'GATE:FLEETV3:TEST:H2SURFACE', ...
    'schema missing H2 required field did not fail closed');

% ---- N-B: stream_map missing one H2 column ----------------------------
ra_sm_missing = rmfield(ra_sm, 'SOC');
ra_threw = false;
try
    fleet_v3.h2_surface_assert(ra_sm_missing, ra_schema, ra_sigreg);
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:CTX:H2SURFACE') ...
        && ~isempty(strfind(char(ME.message), 'SOC'));
end
assert(ra_threw, 'GATE:FLEETV3:TEST:H2SURFACE', ...
    'stream_map missing an H2 column did not fail closed');

% ---- N-C: stream_map with one column outside the schema face ----------
ra_sm_extra = ra_sm;
ra_sm_extra.extra_col_not_in_schema = struct('source', 'logsout:bogus');
ra_threw = false;
try
    fleet_v3.h2_surface_assert(ra_sm_extra, ra_schema, ra_sigreg);
catch ME
    ra_threw = strcmp(char(ME.identifier), 'GATE:FLEETV3:CTX:H2SURFACE') ...
        && ~isempty(strfind(char(ME.message), 'extra_col_not_in_schema'));
end
assert(ra_threw, 'GATE:FLEETV3:TEST:H2SURFACE', ...
    'stream_map export-surface drift (extra col) did not fail closed');

fprintf(['test_h2_surface: PASS (real-schema positive + ctx_build ' ...
    'wiring + schema/sm missing/extra negatives)\n']);

% ---- local helpers ----------------------------------------------------
function names = local_registry_h2(sigreg)
% registry INPUT_H2 names (jsondecode yields struct array or cell of
% structs depending on key homogeneity - handle both)
    sigs = sigreg.signals;
    names = {};
    for i = 1:numel(sigs)
        if isstruct(sigs)
            ra_e = sigs(i);
        else
            ra_e = sigs{i};
        end
        if strcmp(char(ra_e.role), 'INPUT_H2')
            names{end + 1} = char(ra_e.name); %#ok<AGROW>
        end
    end
end

function names = local_field_names(fl)
% field-name cellstr from a struct array or cell of structs
    names = cell(1, numel(fl));
    for i = 1:numel(fl)
        if isstruct(fl)
            names{i} = char(fl(i).name);
        else
            names{i} = char(fl{i}.name);
        end
    end
end

function sm = local_stream_map_like(schema, sigreg)
% Mirrors the ctx_build local_stream_map derivation (schema required
% truth_stream -> registry source; time_s and scalar string|int id
% fields realized separately and excluded here).
    sm = struct();
    fl = local_fields_list(schema.capability_families.truth_stream.fields);
    sigs = local_fields_list(sigreg.signals);
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
            continue;   % scalar id field (scenario_id), member assembly
        end
        src = '';
        for j = 1:numel(sigs)
            if strcmp(char(sigs{j}.name), nm)
                src = char(sigs{j}.source_signal);
                break;
            end
        end
        if isempty(src)
            error('GATE:FLEETV3:TEST:H2SURFACE', ...
                'fixture derivation: registry has no source for %s', nm);
        end
        sm.(nm) = struct('source', src);
    end
end

function fl = local_fields_list(v)
    if isstruct(v)
        n = numel(v);
        fl = cell(1, n);
        for i = 1:n
            fl{i} = v(i);
        end
    elseif iscell(v)
        fl = v(:)';
    else
        error('GATE:FLEETV3:TEST:H2SURFACE', ...
            'expected a JSON array of field objects');
    end
end

function local_safe_delete(p)
    try
        delete(p);
    catch
        % tmp artifact leftover is harmless
    end
end
