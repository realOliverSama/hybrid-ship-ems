function ctx = ctx_build(plan, varargin)
% fleet_v3.ctx_build - shared generation context for the 73 Stage A
% pipeline (T-018 Phase 4-D).  Mirrors the C-side producer_prequal ctx
% provision (config scripts into the base workspace, Model E loaded,
% frozen schema/registry maps, lead grid) but is plan-driven and carries
% the fleet_v3 producer identity.
%
%   ctx = fleet_v3.ctx_build(plan)
%   ctx = fleet_v3.ctx_build(plan, 'ProvisionBase', false)
%       (unit-test / dry-inspection mode; model_ready stays false and
%        the Model E system is NOT loaded)
%
% ctx fields: ROOT, MDL, plan, leads, stream_map, producer_sha,
%   model_ready, artifact label dirs resolved by the gen callers.
%
% producer_sha is the SHA-256 of member_exec.m - the script that
% actually assembles and writes the artifacts (meta.producer_script_sha,
% schema field "导出脚本 SHA256").
%
% Error ids: GATE:FLEETV3:CTX:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths; ROOT from plan (the
% caller already resolved it via mfilename + pwd fallback).

    if ~(isstruct(plan) && isscalar(plan) && isfield(plan, 'modele_root'))
        error('GATE:FLEETV3:CTX:ARG', ...
            'plan must be a fleet_v3.plan_load output');
    end
    ov = struct('provision', true);
    if nargin >= 2
        if ~(ischar(varargin{1}) || isstring(varargin{1}))
            error('GATE:FLEETV3:CTX:ARG', 'expected name-value pairs');
        end
        for k = 1:2:(numel(varargin) - 1)
            if strcmpi(varargin{k}, 'ProvisionBase')
                ov.provision = logical(varargin{k + 1});
            else
                error('GATE:FLEETV3:CTX:ARG', ...
                    'unknown option: %s', char(varargin{k}));
            end
        end
    end

    ROOT = plan.modele_root;
    ctx = struct();
    ctx.ROOT = ROOT;
    ctx.MDL = 'HybridShip_ModelE';
    ctx.plan = plan;
    addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

    w0 = mission_v3.eligibility('windows', 0);
    ctx.leads = (1:double(w0.n_lead_bins)) * double(w0.lead_bin_s);

    schema = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_producer_schema.json');
    sigreg = local_read_json(fullfile(ROOT, 'config'), ...
        'dataset_v3_signal_registry.json');
    ctx.stream_map = local_stream_map(schema, sigreg);

    ctx.producer_sha = local_sha256_file( ...
        fullfile(ROOT, 'scripts', '+fleet_v3', 'member_exec.m'));
    ctx.model_ready = false;
    if ov.provision
        ctx = local_provision(ctx);
    end
end

function ctx = local_provision(ctx)
% Same provision contract as the C producer chain (T-020 precedent):
% frozen config scripts evaluated into the base workspace, then the
% Model E system loaded read-only (runtime injection only through the
% scenario_compile whitelist).  Returns ctx by value (model_ready flag).
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

function sm = local_stream_map(schema, sigreg)
% Required truth_stream fields -> realization sources (T-020
% local_stream_map precedent, derived from the frozen machine schema +
% signal registry, never hard-coded).
    sm = struct();
    fams = schema.capability_families;
    tstr = fams.truth_stream;
    fl = local_fields_list(tstr.fields);
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
            error('GATE:FLEETV3:CTX:STREAM', ...
                'signal registry has no source for required field %s', nm);
        end
        row = struct('source', src);
        sm.(nm) = row;
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
        error('GATE:FLEETV3:CTX:SCHEMA', ...
            'expected a JSON array of field objects');
    end
end

function obj = local_read_json(p1, varargin)
    if nargin >= 3
        p = fullfile(p1, varargin{1}, varargin{2});
    else
        p = fullfile(p1, varargin{1});
    end
    if ~exist(p, 'file')
        error('GATE:FLEETV3:CTX:IO', 'frozen file not found: %s', p);
    end
    obj = jsondecode(fileread(p));
    if ~isstruct(obj)
        error('GATE:FLEETV3:CTX:IO', ...
            'jsondecode did not return a struct for %s', p);
    end
end

function h = local_sha256_file(path)
    fid = fopen(path, 'r');
    if fid < 0
        error('GATE:FLEETV3:CTX:IO', ...
            'cannot open file for hashing: %s', path);
    end
    raw = fread(fid, inf, 'uint8=>uint8');
    fclose(fid);
    md = java.security.MessageDigest.getInstance('SHA-256');
    jbytes = typecast(raw(:), 'int8');
    dg = typecast(md.digest(jbytes), 'uint8');
    h = lower(reshape(dec2hex(dg, 2)', 1, []));
end
