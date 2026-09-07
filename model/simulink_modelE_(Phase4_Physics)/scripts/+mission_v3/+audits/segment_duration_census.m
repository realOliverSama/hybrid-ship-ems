function doc = segment_duration_census(varargin)
% mission_v3.audits.segment_duration_census - T-004 READ-ONLY segment
% duration census and T-429 slice-geometry freeze PROPOSAL (Phase 4-C
% mission v3; FR-547; DGN-12 R8; delegation spec
% experiments/mission_v3/T-004_delegation_spec.md).
%
% PURPOSE
%   Pure registry-arithmetic census: for every segment role touched by the
%   six REQUIRED contrasts in identifiability_contrast_registry.json,
%   compute the usable segment duration distribution
%       usable_s(role) = nominal_distance_m * stretch_low / v_role
%   (standby: duration-defined segment, usable = nominal_duration_s = 600 s;
%   distance = 0, speed conversion not applicable), then test the DGN-12
%   hard constraint
%       slice_duration + 2 * edge_buffer <= min usable segment duration
%   for each REQUIRED contrast at the starting evaluation candidate
%   geometry slice 120 s / edge buffer 30 s (both sides) / stride 60 s.
%   Output: machine-readable census JSON + human-readable MD report with a
%   freeze PROPOSAL. All proposed numbers are PROPOSALS ONLY - the final
%   slice geometry is decided by the main agent and frozen via T-007.
%
% READ-ONLY DISCIPLINE
%   - No Model E simulation, no T-429 metric computation.
%   - Registry / config / .specs files are never modified.
%   - No git operations. No absolute paths anywhere.
%
% USAGE
%   doc = mission_v3.audits.segment_duration_census();
%   doc = mission_v3.audits.segment_duration_census('OutPath', p);
%   doc = mission_v3.audits.segment_duration_census( ...
%           'SpeedOverride', struct('bridge', 4.0), ...   % sensitivity
%           'GeomOverride',  struct('slice_duration_s', 90));
%   Name-value: OutPath (char/string; default
%   results/mission_v3/segment_duration_census.json; when overrides are
%   applied the default basename gains a _sensitivity suffix so the
%   baseline artifact is never overwritten), SpeedOverride (struct,
%   role -> m/s), GeomOverride (struct, subset of slice_duration_s /
%   edge_buffer_s / stride_s).
%
% FAIL-CLOSED GUARDS (error out and escalate; never guess)
%   G1 stretch lower bound must exist (segment_stretch.params.low).
%   G2 every census role must have a positive candidate speed, or be a
%      duration-defined standby segment.
%   G3 exactly six REQUIRED contrasts must be present in the registry.
%   G4 every REQUIRED contrast must map to the delegation-spec host rules
%      (contrast_id and class_A/class_B values are asserted).
%
% ROOT resolution: 4 x fileparts from scripts/+mission_v3/+audits/ + pwd
% fallback (workspace AGENTS.md section 20.2; no absolute paths anywhere).

    args = parseArgs(varargin{:});

    % ---- ROOT resolution (mfilename + pwd fallback) ------------------------
    ROOT = fileparts(fileparts(fileparts(fileparts(mfilename('fullpath')))));
    if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end

    regDir = fullfile(ROOT, 'config', 'mission_v3');
    tplPath = fullfile(regDir, 'mission_template_registry.json');
    conPath = fullfile(regDir, 'identifiability_contrast_registry.json');
    dagPath = fullfile(regDir, 'conditional_dag_config.json');
    regTpl = readJson(tplPath);
    regCon = readJson(conPath);
    regDag = readJson(dagPath);

    % ---- fixed inputs ------------------------------------------------------
    [speedTbl, speedBasis] = defaultSpeedTable();
    [speedTbl, speedBasis, speedOvrApplied] = ...
        applySpeedOverride(speedTbl, speedBasis, args.SpeedOverride);

    stretchLow = NaN;
    stretchHigh = NaN;
    [stretchLow, stretchHigh] = getStretchBounds(regDag);

    geom = struct('slice_duration_s', 120, 'edge_buffer_s', 30, 'stride_s', 60);
    [geom, geomOvrApplied] = applyGeomOverride(geom, args.GeomOverride);
    requiredS = geom.slice_duration_s + 2 * geom.edge_buffer_s;
    ovrApplied = speedOvrApplied || geomOvrApplied;

    % ---- output paths ------------------------------------------------------
    if isempty(args.OutPath)
        baseName = 'segment_duration_census';
        if ovrApplied
            baseName = [baseName, '_sensitivity'];
        end
        outPath = fullfile(ROOT, 'results', 'mission_v3', [baseName, '.json']);
    else
        outPath = char(args.OutPath);
    end
    outDir = fileparts(outPath);
    if ~exist(outDir, 'dir')
        okDir = mkdir(outDir);
        if ~okDir
            error('T004:MKDIR_FAILED', ...
                'T004: cannot create output directory %s', outDir);
        end
    end
    outDir = fileparts(outPath);
    [~, outBase] = fileparts(outPath);
    mdPath = fullfile(outDir, [outBase, '.md']);

    % ---- census core -------------------------------------------------------
    rows = buildRows(regTpl, speedTbl, speedBasis, stretchLow);
    agg = aggregateTemplateRole(rows);
    roleMin = buildRoleMin(agg, speedTbl, speedBasis);
    cov = evalContrasts(regCon, agg, requiredS);
    wins = buildWindows(agg, geom, requiredS);
    prop = buildProposal(regCon, agg, speedTbl, speedBasis, stretchLow, ...
        geom, requiredS, cov);

    % ---- assemble document -------------------------------------------------
    genTime = datetime('now', 'TimeZone', 'UTC');
    genTime.Format = 'yyyy-MM-dd''T''HH:mm:ss''Z''';
    doc = struct();
    doc.schema = 'mission_v3_segment_duration_census_v1';
    doc.generated_at = char(genTime);
    doc.task_id = 'T-004';
    doc.route_mapping = 'T-429; FR-547; DGN-12 (R8); proposal consumed by T-007 freeze';
    doc.method = ['pure registry arithmetic census; no Model E simulation; ' ...
        'no T-429 metric execution; registry values untouched'];
    doc.read_only_statement = ['registries, Model E files and .specs are ' ...
        'read-only for this task; all proposed values are proposals for ' ...
        'main-agent adjudication (T-007 freeze)'];
    doc.guards_enforced = { ...
        'G1 stretch lower bound must exist (segment_stretch.params.low)', ...
        'G2 every role needs a positive candidate speed or duration-defined standby', ...
        'G3 exactly six REQUIRED contrasts expected', ...
        'G4 REQUIRED contrast ids and class values must match the delegation spec host rules', ...
        'hard-constraint comparison tolerance 1e-9 s'};
    doc.inputs = buildInputsDoc(tplPath, conPath, dagPath, regTpl, regCon, ...
        regDag, speedTbl, speedBasis, stretchLow, stretchHigh, geom, ...
        requiredS, args, speedOvrApplied, geomOvrApplied);
    doc.per_template_role = agg;
    doc.role_min_usable_s = roleMin.map;
    doc.role_min_detail = roleMin.rows;
    doc.contrast_coverage = cov;
    doc.window_arithmetic = wins;
    doc.proposal = prop;

    writeText(outPath, jsonencode(doc, 'PrettyPrint', true));
    writeText(mdPath, buildMd(doc));

    % ---- console summary ---------------------------------------------------
    fprintf('[T004] stretch_low=%.4f (source: %s)\n', stretchLow, ...
        'conditional_dag_config nodes.segment_stretch.params.low');
    fprintf('[T004] geometry candidate: slice=%g s buffer=%g s stride=%g s -> required %g s\n', ...
        geom.slice_duration_s, geom.edge_buffer_s, geom.stride_s, requiredS);
    nFail = 0;
    for k = 1:numel(cov.required)
        cr = cov.required{k};
        if ~cr.contrast_pass
            nFail = nFail + 1;
        end
        vStr = 'PASS';
        if ~cr.contrast_pass
            vStr = 'FAIL';
        end
        fprintf('[T004]   %-34s min_usable=%9.3f s  %s\n', ...
            cr.contrast_id, cr.contrast_min_usable_s, vStr);
    end
    fprintf('[T004] overall candidate-geometry verdict: %s (%d/6 contrasts violate)\n', ...
        prop.overall_verdict, nFail);
    fprintf('[T004] JSON: %s\n', outPath);
    fprintf('[T004] MD:   %s\n', mdPath);
end

% ========================================================================
% argument handling
% ========================================================================
function args = parseArgs(varargin)
    p = inputParser;
    p.addParameter('OutPath', '', @(x) ischar(x) || isstring(x));
    p.addParameter('SpeedOverride', struct(), @(x) isstruct(x));
    p.addParameter('GeomOverride', struct(), @(x) isstruct(x));
    p.parse(varargin{:});
    args = p.Results;
    args.OutPath = char(string(args.OutPath));
end

function [tbl, basis, applied] = applySpeedOverride(tbl, basis, ovr)
    applied = false;
    fn = fieldnames(ovr);
    for k = 1:numel(fn)
        f = fn{k};
        if ~isfield(tbl, f)
            error('T004:G2_OVERRIDE_UNKNOWN_ROLE', ...
                'T004 G2: SpeedOverride role %s is not a known census role', f);
        end
        v = double(ovr.(f));
        if ~isfinite(v) || v <= 0
            error('T004:G2_OVERRIDE_BAD_SPEED', ...
                'T004 G2: SpeedOverride for %s must be a positive finite speed', f);
        end
        tbl.(f) = v;
        basis.(f) = sprintf('%s [SpeedOverride applied: %.6f m/s]', basis.(f), v);
        applied = true;
    end
end

function [geom, applied] = applyGeomOverride(geom, ovr)
    applied = false;
    allowed = {'slice_duration_s', 'edge_buffer_s', 'stride_s'};
    fn = fieldnames(ovr);
    for k = 1:numel(fn)
        f = fn{k};
        if ~any(strcmp(allowed, f))
            error('T004:GEOM_OVERRIDE_UNKNOWN_FIELD', ...
                'T004: GeomOverride field %s not in {%s}', f, strjoin(allowed, ', '));
        end
        v = double(ovr.(f));
        if ~isfinite(v) || v <= 0
            error('T004:GEOM_OVERRIDE_BAD_VALUE', ...
                'T004: GeomOverride.%s must be a positive finite number', f);
        end
        geom.(f) = v;
        applied = true;
    end
end

% ========================================================================
% registry access helpers
% ========================================================================
function c = toList(x)
    if iscell(x)
        c = x;
        return;
    end
    c = cell(1, numel(x));
    for k = 1:numel(x)
        c{k} = x(k);
    end
end

function v = getf(s, name, default)
    if isstruct(s) && isfield(s, name)
        v = s.(name);
    else
        v = default;
    end
end

function j = readJson(fp)
    if exist(fp, 'file') ~= 2
        error('T004:REGISTRY_MISSING', 'T004: registry file not found: %s', fp);
    end
    j = jsondecode(fileread(fp));
end

function h = fileSha256(fp)
    h = '';
    try
        md = javaMethod('getInstance', 'java.security.MessageDigest', 'SHA-256');
        fis = javaObject('java.io.FileInputStream', fp);
        buf = zeros(1, 65536, 'int8');
        got = fis.read(buf);
        while got > 0
            md.update(buf, 0, got);
            got = fis.read(buf);
        end
        fis.close();
        dg = typecast(md.digest(), 'uint8');
        h = sprintf('%02x', double(dg));
    catch err
        h = ['unavailable: ', err.message];
    end
end

function [low, high] = getStretchBounds(dag)
    nodes = toList(dag.nodes);
    low = NaN;
    high = NaN;
    for i = 1:numel(nodes)
        nd = nodes{i};
        if strcmp(nd.node_id, 'segment_stretch')
            p = getf(nd, 'params', struct());
            low = double(getf(p, 'low', NaN));
            high = double(getf(p, 'high', NaN));
        end
    end
    if isnan(low) || low <= 0
        error('T004:G1_STRETCH_LOW_MISSING', ...
            'T004 G1: segment_stretch.params.low missing/invalid in conditional_dag_config.json');
    end
end

% ========================================================================
% candidate speed table (Fixed Decision 1 of the delegation spec)
% ========================================================================
function [tbl, basis] = defaultSpeedTable()
    tbl = struct();
    basis = struct();
    tbl.departure = 2.5;
    basis.departure = 'census proposal candidate (main agent)';
    tbl.harbor = 2.5;
    basis.harbor = 'census proposal candidate (main agent)';
    tbl.berth = 2.0;
    basis.berth = 'census proposal candidate (main agent)';
    tbl.bridge = 3.5;
    basis.bridge = 'census proposal candidate (main agent)';
    tbl.cruise = 5.5;
    basis.cruise = 'census proposal candidate (main agent)';
    tbl.patrol = 5.0;
    basis.patrol = 'census proposal candidate (main agent)';
    tbl.('return') = 5.5;
    basis.('return') = 'census proposal candidate (main agent)';
    tbl.rush = 7.522;
    basis.rush = ['FR-510/T-412 G2 value_kn=14.620 (read-only reference); ' ...
        'value fixed by delegation spec'];
    tbl.mission = 5.0;
    basis.mission = 'census proposal candidate (main agent)';
    tbl.work = 1.2;
    basis.work = 'low-speed work semantics (T-010 plan-layer candidate)';
    tbl.standby = NaN;
    basis.standby = ['duration-defined segment; usable = nominal_duration_s ' ...
        '(registry, 600 s); speed conversion not applicable (distance=0)'];
end

% ========================================================================
% per-template role census rows
% ========================================================================
function rows = buildRows(regTpl, speedTbl, speedBasis, stretchLow)
    tplList = toList(regTpl.mission_templates);
    rows = {};
    for i = 1:numel(tplList)
        tpl = tplList{i};
        tid = tpl.mission_template_id;
        seq = toList(tpl.segment_sequence);
        returnIdx = 0;
        for j = 1:numel(seq)
            sg = seq{j};
            if strcmp(sg.role, 'return')
                returnIdx = j;
                break;
            end
        end
        for j = 1:numel(seq)
            sg = seq{j};
            role = sg.role;
            notes = {};
            if strcmp(role, 'berth')
                direction = 'none';
                notes{end+1} = ['moored segment; excluded from direction host ' ...
                    'sets (non-berth rule, Fixed Decision 5)']; %#ok<AGROW>
            elseif strcmp(role, 'return')
                direction = 'downstream';
            elseif returnIdx > 0 && j > returnIdx
                direction = 'post_return_unmapped';
                notes{end+1} = ['segment positioned after return; direction ' ...
                    'rule undefined in delegation spec (none in current registry)']; %#ok<AGROW>
            else
                direction = 'upstream';
                if strcmp(role, 'standby')
                    notes{end+1} = ['stationary segment (distance=0, ' ...
                        'duration-defined); upstream per mechanical non-berth ' ...
                        'outbound rule - inclusion flagged for main agent']; %#ok<AGROW>
                end
            end
            presence = getf(sg, 'presence', '');
            if strcmp(presence, 'CONDITIONAL')
                notes{end+1} = ['presence CONDITIONAL (registry): segment may ' ...
                    'be absent in some voyages; census uses nominal geometry if present']; %#ok<AGROW>
            end
            dist = double(getf(sg, 'nominal_distance_m', NaN));
            dur = double(getf(sg, 'nominal_duration_s', NaN));
            if strcmp(role, 'standby')
                if isnan(dur) || ~isfinite(dur) || dur <= 0
                    error('T004:G2_STANDBY_NO_DURATION', ...
                        'T004 G2: standby segment in %s has no usable nominal_duration_s', tid);
                end
                speedUsed = NaN;
                usable = dur;
                notes{end+1} = ['duration-defined segment: usable = ' ...
                    'nominal_duration_s; stretch not applied (distance=0)']; %#ok<AGROW>
            else
                if ~isfield(speedTbl, role)
                    error('T004:G2_UNKNOWN_ROLE', ...
                        'T004 G2: role %s (template %s) has no candidate speed', role, tid);
                end
                speedUsed = speedTbl.(role);
                if ~isfinite(speedUsed) || speedUsed <= 0
                    error('T004:G2_BAD_SPEED', ...
                        'T004 G2: candidate speed for role %s is not positive finite', role);
                end
                if isnan(dist) || ~isfinite(dist) || dist < 0
                    error('T004:G2_BAD_DISTANCE', ...
                        'T004 G2: nominal_distance_m for role %s in %s invalid', role, tid);
                end
                usable = dist * stretchLow / speedUsed;
            end
            row = struct();
            row.mission_template_id = tid;
            row.role = role;
            row.seq_index = j;
            row.direction = direction;
            row.nominal_distance_m = dist;
            row.nominal_duration_s = dur;
            row.candidate_speed_mps = speedUsed;
            row.speed_basis = speedBasis.(role);
            row.usable_s = round(usable, 6);
            row.usable_s_exact = usable;
            row.notes = notes;
            rows{end+1} = row; %#ok<AGROW>
        end
    end
end

function agg = aggregateTemplateRole(rows)
% one row per (template, role); usable_s = min over occurrences
% (delegation spec Fixed Decision 3)
    keys = {};
    buckets = {};
    for k = 1:numel(rows)
        r = rows{k};
        key = [r.mission_template_id, '||', r.role];
        idx = find(strcmp(keys, key), 1);
        if isempty(idx)
            g = r;
            g.seq_indices = r.seq_index;
            g.distances_m = r.nominal_distance_m;
            g.durations_s = r.nominal_duration_s;
            g = rmfield(g, 'seq_index');
            keys{end+1} = key; %#ok<AGROW>
            buckets{end+1} = g; %#ok<AGROW>
        else
            g = buckets{idx};
            g.seq_indices(end+1) = r.seq_index;
            g.distances_m(end+1) = r.nominal_distance_m;
            if isfinite(r.nominal_duration_s)
                g.durations_s(end+1) = r.nominal_duration_s;
            end
            if r.usable_s_exact < g.usable_s_exact
                keep = {'candidate_speed_mps', 'speed_basis', 'usable_s', ...
                    'usable_s_exact', 'nominal_distance_m', 'direction'};
                for f = 1:numel(keep)
                    g.(keep{f}) = r.(keep{f});
                end
            end
            buckets{idx} = g;
        end
    end
    for k = 1:numel(buckets)
        b = buckets{k};
        if isempty(b.durations_s)
            b.durations_s = [];
        end
        buckets{k} = b;
    end
    agg = buckets;
end

function rm = buildRoleMin(agg, speedTbl, speedBasis)
    map = struct();
    detail = {};
    roles = {};
    for k = 1:numel(agg)
        if ~any(strcmp(roles, agg{k}.role))
            roles{end+1} = agg{k}.role; %#ok<AGROW>
        end
    end
    for r = 1:numel(roles)
        role = roles{r};
        minU = Inf;
        minT = {};
        allT = {};
        for k = 1:numel(agg)
            if strcmp(agg{k}.role, role)
                allT{end+1} = agg{k}.mission_template_id; %#ok<AGROW>
                if agg{k}.usable_s_exact < minU
                    minU = agg{k}.usable_s_exact;
                    minT = {agg{k}.mission_template_id};
                elseif abs(agg{k}.usable_s_exact - minU) <= 1e-9
                    minT{end+1} = agg{k}.mission_template_id; %#ok<AGROW>
                end
            end
        end
        map.(role) = round(minU, 6);
        d = struct();
        d.role = role;
        d.min_usable_s = round(minU, 6);
        d.achieved_in_templates = minT;
        d.present_in_templates = allT;
        d.candidate_speed_mps = speedTbl.(role);
        d.speed_basis = speedBasis.(role);
        detail{end+1} = d; %#ok<AGROW>
    end
    rm = struct();
    rm.map = map;
    rm.rows = detail;
end

% ========================================================================
% contrast coverage / hard-constraint evaluation
% ========================================================================
function cov = evalContrasts(regCon, agg, requiredS)
    cList = toList(regCon.contrasts);
    covRows = {};
    excl = {};
    nReq = 0;
    for i = 1:numel(cList)
        c = cList{i};
        if strcmpi(c.gate_role, 'REQUIRED')
            nReq = nReq + 1;
            covRows{end+1} = evalOneContrast(c, agg, requiredS); %#ok<AGROW>
        else
            e = struct();
            e.contrast_id = c.contrast_id;
            e.gate_role = c.gate_role;
            e.note = 'excluded from hard-constraint obligations (gate_role not REQUIRED)';
            excl{end+1} = e; %#ok<AGROW>
        end
    end
    if nReq ~= 6
        error('T004:G3_REQUIRED_COUNT', ...
            'T004 G3: expected exactly 6 REQUIRED contrasts, found %d', nReq);
    end
    cov = struct();
    cov.hard_constraint_required_s = requiredS;
    cov.required = covRows;
    cov.excluded = excl;
end

function row = evalOneContrast(c, agg, requiredS)
    cid = c.contrast_id;
    note = '';
    switch cid
        case 'SEG_BRIDGE_VS_HARBOR'
            assertClasses(c, 'bridge', 'harbor');
            hA = rowsWithRole(agg, 'bridge');
            hB = rowsWithRole(agg, 'harbor');
            note = 'class hosts = all census rows with the matching segment role';
        case 'SEG_BERTH_VS_DEPARTURE'
            assertClasses(c, 'berth', 'departure');
            hA = rowsWithRole(agg, 'berth');
            hB = rowsWithRole(agg, 'departure');
            note = 'class hosts = all census rows with the matching segment role';
        case 'DIR_UPSTREAM_VS_DOWNSTREAM'
            assertClasses(c, 'upstream', 'downstream');
            hA = rowsWithDirection(agg, 'upstream');
            hB = rowsWithDirection(agg, 'downstream');
            note = ['hosts = all non-berth segments carrying the class ' ...
                'direction, listed per template (Fixed Decision 5)'];
        case 'DEPTH_LOW_ID_VS_NORMAL_ID'
            assertClasses(c, 'low_id', 'normal_id');
            hA = longestUsablePerTemplate(agg);
            hB = hA;
            note = ['voyage-level depth classes share segment hosts; host ' ...
                'record = longest usable role per template (Fixed Decision 5)'];
        case 'SEG_EMERGENCY_SPRINT_VS_RETURN_CRUISE'
            % D6 host rebinding (Day-1 Gate Ruling 2026-09-05 section D):
            % class_B host = RETURN steady return-transit segment.
            assertClasses(c, 'rush', 'return');
            hA = rowsWithRole(agg, 'rush');
            hB = rowsWithRole(agg, 'return');
            note = ['class hosts = all census rows with the matching ' ...
                'segment role (class_B host rebound from cruise to ' ...
                'return per D6; cruise host rejected - EMERGENCY_RESPONSE ' ...
                'has no cruise segment and mission role carries ' ...
                'mission-load nuisance)'];
        case 'SEG_TRAFFIC_FOLLOW_VS_FREE_CRUISE'
            assertClasses(c, 'traffic_follow', 'free_cruise');
            hA = rowsWithRole(agg, 'cruise');
            hB = hA;
            note = ['traffic events are injected on cruise segments; both ' ...
                'traffic classes host on cruise (Fixed Decision 5)'];
        otherwise
            error('T004:G4_UNMAPPED_CONTRAST', ...
                'T004 G4: REQUIRED contrast %s has no host mapping in the delegation spec', cid);
    end
    minA = classMinUsable(hA);
    minB = classMinUsable(hB);
    row = struct();
    row.contrast_id = cid;
    row.factor_axis = c.factor_axis;
    row.class_A = c.class_A;
    row.class_B = c.class_B;
    row.hard_constraint_required_s = requiredS;
    row.class_A_hosts = hA;
    row.class_A_min_usable_s = round(minA, 6);
    row.class_A_pass = (minA >= requiredS - 1e-9);
    row.class_B_hosts = hB;
    row.class_B_min_usable_s = round(minB, 6);
    row.class_B_pass = (minB >= requiredS - 1e-9);
    row.contrast_min_usable_s = round(min(minA, minB), 6);
    row.contrast_pass = (min(minA, minB) >= requiredS - 1e-9);
    row.violating_hosts = [classViolating(hA, requiredS, 'A'), ...
        classViolating(hB, requiredS, 'B')];
    row.note = note;
end

function assertClasses(c, aExp, bExp)
    if ~strcmp(c.class_A, aExp) || ~strcmp(c.class_B, bExp)
        error('T004:G4_CLASS_DRIFT', ...
            'T004 G4: contrast %s has class_A/class_B = %s/%s, delegation spec expects %s/%s', ...
            c.contrast_id, c.class_A, c.class_B, aExp, bExp);
    end
end

function out = rowsWithRole(agg, role)
    out = {};
    for k = 1:numel(agg)
        if strcmp(agg{k}.role, role)
            out{end+1} = hostRec(agg{k}); %#ok<AGROW>
        end
    end
end

function out = rowsWithDirection(agg, direction)
    out = {};
    for k = 1:numel(agg)
        if strcmp(agg{k}.direction, direction)
            out{end+1} = hostRec(agg{k}); %#ok<AGROW>
        end
    end
end

function out = longestUsablePerTemplate(agg)
    tids = {};
    out = {};
    for k = 1:numel(agg)
        tid = agg{k}.mission_template_id;
        idx = find(strcmp(tids, tid), 1);
        if isempty(idx)
            tids{end+1} = tid; %#ok<AGROW>
            out{end+1} = hostRec(agg{k}); %#ok<AGROW>
        else
            cur = out{idx};
            if agg{k}.usable_s_exact > cur.usable_s
                out{idx} = hostRec(agg{k});
            end
        end
    end
end

function hr = hostRec(r)
    hr = struct();
    hr.mission_template_id = r.mission_template_id;
    hr.role = r.role;
    hr.usable_s = r.usable_s;
end

function mn = classMinUsable(hosts)
    if isempty(hosts)
        error('T004:G4_EMPTY_HOST_SET', ...
            'T004 G4: a REQUIRED contrast class maps to an empty host set (escalate)');
    end
    mn = Inf;
    for k = 1:numel(hosts)
        mn = min(mn, hosts{k}.usable_s);
    end
end

function vv = classViolating(hosts, requiredS, cls)
    vv = {};
    for k = 1:numel(hosts)
        h = hosts{k};
        if h.usable_s < requiredS - 1e-9
            v = struct();
            v.class = cls;
            v.mission_template_id = h.mission_template_id;
            v.role = h.role;
            v.usable_s = h.usable_s;
            v.deficit_s = round(requiredS - h.usable_s, 6);
            vv{end+1} = v; %#ok<AGROW>
        end
    end
end

% ========================================================================
% window arithmetic
% ========================================================================
function wins = buildWindows(agg, geom, requiredS)
    wins = {};
    for k = 1:numel(agg)
        r = agg{k};
        eligible = (r.usable_s_exact >= requiredS - 1e-9);
        n = 0;
        if eligible
            span = r.usable_s_exact - 2 * geom.edge_buffer_s - geom.slice_duration_s;
            n = max(0, floor(span / geom.stride_s) + 1);
        end
        w = struct();
        w.mission_template_id = r.mission_template_id;
        w.role = r.role;
        w.usable_s = r.usable_s;
        w.hard_constraint_required_s = requiredS;
        w.eligible = eligible;
        w.n_slices_at_stride = n;
        wins{end+1} = w; %#ok<AGROW>
    end
end

% ========================================================================
% freeze proposal
% ========================================================================
function prop = buildProposal(regCon, agg, speedTbl, speedBasis, stretchLow, ...
    geom, requiredS, cov)

    perContrast = {};
    for k = 1:numel(cov.required)
        cr = cov.required{k};
        p = struct();
        p.contrast_id = cr.contrast_id;
        p.contrast_pass = cr.contrast_pass;
        p.contrast_min_usable_s = cr.contrast_min_usable_s;
        perContrast{end+1} = p; %#ok<AGROW>
    end
    overall = 'PASS';
    for k = 1:numel(perContrast)
        if ~perContrast{k}.contrast_pass
            overall = 'FAIL';
        end
    end

    % violating-host detail with remediation thresholds (generic arithmetic)
    vDetail = {};
    for k = 1:numel(cov.required)
        cr = cov.required{k};
        for m = 1:numel(cr.violating_hosts)
            v = cr.violating_hosts{m};
            rowK = [];
            for j = 1:numel(agg)
                if strcmp(agg{j}.mission_template_id, v.mission_template_id) ...
                        && strcmp(agg{j}.role, v.role)
                    rowK = agg{j};
                    break;
                end
            end
            if isempty(rowK)
                continue;
            end
            v.contrast_id = cr.contrast_id;
            v.max_plan_speed_for_pass_mps = round( ...
                rowK.distances_m(1) * stretchLow / requiredS, 6);
            v.required_distance_at_candidate_speed_m = round( ...
                requiredS * rowK.candidate_speed_mps / stretchLow, 3);
            vDetail{end+1} = v; %#ok<AGROW>
        end
    end

    % remediation R1 / R2: tighter global geometries, re-evaluated with the
    % same engine (stride candidates = slice/2, mirroring 120/60)
    r1Geom = struct('slice_duration_s', 90, 'edge_buffer_s', 30, 'stride_s', 45);
    r2Geom = struct('slice_duration_s', 60, 'edge_buffer_s', 15, 'stride_s', 30);
    r1 = remediationRow(regCon, agg, r1Geom, ...
        'geometry-only global revision (slice 90 / buffer 30 / stride 45)', {});
    r1.id = 'R1';
    r2 = remediationRow(regCon, agg, r2Geom, ...
        'geometry-only global revision (slice 60 / buffer 15 / stride 30)', ...
        {'fragile_margin_on_bridge'});
    r2.id = 'R2';

    % bridge/departure census values (for remediation arithmetic; generic)
    bridgeU = NaN;
    bridgeDist = NaN;
    bridgeV = NaN;
    departureDist = NaN;
    departureV = NaN;
    for k = 1:numel(agg)
        if strcmp(agg{k}.role, 'bridge')
            bridgeU = agg{k}.usable_s_exact;
            bridgeDist = agg{k}.distances_m(1);
            bridgeV = agg{k}.candidate_speed_mps;
        end
        if strcmp(agg{k}.role, 'departure')
            departureDist = agg{k}.distances_m(1);
            departureV = agg{k}.candidate_speed_mps;
        end
    end
    r1.fails_note = failsNote(r1);
    r2.fails_note = failsNote(r2);
    r2Req = r2Geom.slice_duration_s + 2 * r2Geom.edge_buffer_s;
    r2Thresh = bridgeDist * stretchLow / r2Req;
    r2.arithmetic{end+1} = sprintf(['bridge margin at R2 = usable %.6f s - ' ...
        'required %g s = %.6f s; in speed terms v_threshold = %g*%.4g/%g = ' ...
        '%.6f m/s vs candidate %.6f m/s (+%.2f%%) - margin consumed by ' ...
        'candidate-speed uncertainty'], bridgeU, r2Req, bridgeU - r2Req, ...
        bridgeDist, stretchLow, r2Req, r2Thresh, bridgeV, ...
        100 * (r2Thresh / bridgeV - 1));

    % R3: keep candidate geometry; template/plan-layer revisions (T-010)
    r3 = struct();
    r3.id = 'R3';
    r3.title = sprintf( ...
        'keep candidate %g/%g/%g; T-010-level template/plan revisions for bridge and departure', ...
        geom.slice_duration_s, geom.edge_buffer_s, geom.stride_s);
    r3.geometry_candidate = geom;
    r3.hard_constraint_required_s = requiredS;
    r3.per_contrast_pass = perContrast;
    r3.flags = {'requires_template_revision', 'requires_main_agent_decision'};
    r3.arithmetic = { ...
        sprintf(['R3a bridge: usable >= %g s at v=%.6f m/s requires ' ...
        'nominal_distance >= %g*%.6f/0.8 = %.1f m (registry %g m; +%0.1f m); ' ...
        'OR at %g m requires v <= %g*0.8/%g = %.6f m/s (not a plausible ' ...
        'bridge-transit plan vs candidate %.6f m/s)'], ...
        requiredS, bridgeV, requiredS, bridgeV, ...
        requiredS * bridgeV / stretchLow, bridgeDist, ...
        requiredS * bridgeV / stretchLow - bridgeDist, bridgeDist, ...
        bridgeDist, requiredS, bridgeDist * stretchLow / requiredS, bridgeV), ...
        sprintf(['R3b departure: at %g m requires v <= %g*0.8/%g = %.6f m/s ' ...
        '(candidate %.6f m/s; %0.2f%% slower plan); OR at v=%.6f requires ' ...
        'nominal_distance >= %g*%.6f/0.8 = %.1f m (registry %g m)'], ...
        departureDist, departureDist, requiredS, ...
        departureDist * stretchLow / requiredS, departureV, ...
        100 * (1 - departureDist * stretchLow / requiredS / departureV), ...
        departureV, requiredS, departureV, ...
        requiredS * departureV / stretchLow, departureDist), ...
        sprintf(['R3c all other hosts already pass at %g/%g/%g (see ' ...
        'contrast_coverage)'], geom.slice_duration_s, geom.edge_buffer_s, ...
        geom.stride_s)};
    r3.fails_note = ['candidate geometry itself still fails for ' ...
        'bridge/departure hosts until the registry/plan revisions land'];
    r3.verdict = 'NOT_EVALUABLE_AS_IS (conditional on revisions)';

    % R4: per-contrast geometry split
    r4 = struct();
    r4.id = 'R4';
    r4.title = 'per-contrast (or per-axis) geometry split';
    r4.geometry_candidate = geom;
    r4.hard_constraint_required_s = requiredS;
    r4.flags = {'requires_design_interpretation', 'possible_spec_amendment'};
    r4.arithmetic = { ...
        ['FR-547 defines the main analysis unit as SEGMENT_PURE_DIAGNOSTIC_' ...
        'SLICE with FIXED duration / FIXED edge buffer; whether one global ' ...
        'geometry or per-axis geometries is allowed is a design ' ...
        'interpretation question reserved to the main agent']};
    r4.fails_note = '';
    r4.verdict = 'NOT_DECIDABLE_IN_THIS_TASK';

    remediations = {r1, r2, r3, r4};

    % main-agent adjudication items (thresholds computed from census values)
    bridgeVmax = bridgeDist * stretchLow / requiredS;
    departureVmax = departureDist * stretchLow / requiredS;
    departureDmin = requiredS * departureV / stretchLow;
    decisions = { ...
        sprintf(['D1 (escalation per delegation spec Escalate-If): bridge ' ...
        'host cannot satisfy the %g s hard constraint under any plausible ' ...
        'bridge-transit speed at registry nominal %g m (v_threshold = ' ...
        '%.6f m/s). Choose between R2 (tighter global geometry, fragile ' ...
        'margin), R3a (template-level bridge distance/speed revision at ' ...
        'T-010), or R4 (per-contrast geometry; design interpretation ' ...
        'needed).'], requiredS, bridgeDist, bridgeVmax), ...
        sprintf(['D2 departure host fails at %.6f s vs %g s but is fixable ' ...
        'by R1 (slice 90: 150 <= %.6f), or a modest departure plan-speed ' ...
        'reduction (<= %.6f m/s), or distance increase (>= %.1f m).'], ...
        departureDist * stretchLow / departureV, requiredS, ...
        departureDist * stretchLow / departureV, departureVmax, departureDmin), ...
        ['D3 standby is included as a DIR upstream host per the mechanical ' ...
        'non-berth outbound rule, but it is a stationary segment; if the ' ...
        'main agent excludes it from direction hosts, no numeric verdict ' ...
        'changes (its 600 s is not the set minimum).'], ...
        ['D4 standby usable duration applies no stretch (duration-defined, ' ...
        'distance=0). If T-007 prefers a conservative 0.8x duration ' ...
        'stretch, usable = 480 s - still >= 180 s; no verdict change.'], ...
        ['D5 berth exists only in ROUTINE_PATROL; SEG_BERTH_VS_DEPARTURE ' ...
        'declares same-template pairing. This census covers geometry ' ...
        'only; cohort feasibility belongs to later tasks.'], ...
        ['D6 cruise does not appear in the EMERGENCY_RESPONSE segment ' ...
        'sequence while SEG_EMERGENCY_SPRINT_VS_CRUISE cohort predicate ' ...
        'pins template == EMERGENCY_RESPONSE; class_B host rows therefore ' ...
        'live in ROUTINE_PATROL / WORK_MISSION (usable 581.818182 s, ' ...
        'geometrically feasible). The cross-template pairing semantics ' ...
        'belong to the main agent; the registry is read-only here.'], ...
        ['D7 the candidate speed table itself is a proposal (Fixed ' ...
        'Decision 1); rush = 7.522 m/s is quoted as-is from the ' ...
        'delegation spec (FR-510/T-412 G2 value_kn=14.620, read-only ' ...
        'reference). Use SpeedOverride to run sensitivity variants.']};

    prop = struct();
    prop.candidate_geometry = geom;
    prop.hard_constraint = 'slice_duration_s + 2 * edge_buffer_s <= min usable segment duration';
    prop.hard_constraint_required_s = requiredS;
    prop.candidate_geometry_source = ['design.md DGN-12 starting evaluation ' ...
        'candidate (240/60/120 withdrawn: required >= 360 s pure segment); ' ...
        'final values frozen via T-007'];
    prop.overall_verdict = overall;
    prop.per_contrast_verdict = perContrast;
    prop.violating_host_detail = vDetail;
    prop.remediation_candidates = remediations;
    prop.freeze_note = ['census proposes only; nothing is frozen here. ' ...
        'Final geometry values are decided by the main agent and frozen ' ...
        'via T-007 (tasks.md item 7).'];
    prop.main_agent_decision_items = decisions;
    prop.speed_table_basis = speedBasis;
end

function r = remediationRow(regCon, agg, geom, title, flags)
    requiredS = geom.slice_duration_s + 2 * geom.edge_buffer_s;
    cov2 = evalContrasts(regCon, agg, requiredS);
    per = {};
    for k = 1:numel(cov2.required)
        cr = cov2.required{k};
        p = struct();
        p.contrast_id = cr.contrast_id;
        p.contrast_pass = cr.contrast_pass;
        p.contrast_min_usable_s = cr.contrast_min_usable_s;
        per{end+1} = p; %#ok<AGROW>
    end
    allPass = true;
    for k = 1:numel(per)
        if ~per{k}.contrast_pass
            allPass = false;
        end
    end
    r = struct();
    r.id = '';
    r.title = title;
    r.geometry_candidate = geom;
    r.hard_constraint_required_s = requiredS;
    r.per_contrast_pass = per;
    r.flags = flags;
    r.arithmetic = {sprintf('required = %g + 2*%g = %g s', ...
        geom.slice_duration_s, geom.edge_buffer_s, requiredS)};
    if allPass
        r.verdict = 'ALL_SIX_CONTRASTS_PASS (check flags for freeze risk)';
    else
        r.verdict = 'STILL_FAILS for the contrasts listed in fails_note';
    end
end

function s = failsNote(r)
    parts = {};
    for k = 1:numel(r.per_contrast_pass)
        if ~r.per_contrast_pass{k}.contrast_pass
            parts{end+1} = r.per_contrast_pass{k}.contrast_id; %#ok<AGROW>
        end
    end
    if isempty(parts)
        s = '';
    else
        s = ['still failing: ', strjoin(parts, ', ')];
    end
end

% ========================================================================
% inputs documentation
% ========================================================================
function inp = buildInputsDoc(tplPath, conPath, dagPath, regTpl, regCon, ...
    regDag, speedTbl, speedBasis, stretchLow, stretchHigh, geom, ...
    requiredS, args, speedOvrApplied, geomOvrApplied)

    inp = struct();

    sr = struct();
    sr.mission_template_registry = registryRef( ...
        'config/mission_v3/mission_template_registry.json', tplPath, regTpl);
    sr.identifiability_contrast_registry = registryRef( ...
        'config/mission_v3/identifiability_contrast_registry.json', conPath, regCon);
    sr.conditional_dag_config = registryRef( ...
        'config/mission_v3/conditional_dag_config.json', dagPath, regDag);
    inp.source_registries = sr;

    inp.candidate_speed_table_mps = speedTbl;
    inp.speed_basis = speedBasis;

    sf = struct();
    sf.low = stretchLow;
    sf.high = stretchHigh;
    sf.source_path_relative_to_model_root = ...
        'config/mission_v3/conditional_dag_config.json';
    sf.source_pointer = 'nodes[node_id=segment_stretch].params.low';
    inp.stretch_factor = sf;

    gc = geom;
    gc.hard_constraint = ...
        'slice_duration_s + 2 * edge_buffer_s <= min usable segment duration';
    gc.hard_constraint_required_s = requiredS;
    inp.geometry_candidate = gc;

    inp.usable_duration_formula = ...
        'usable_s(role) = nominal_distance_m * stretch_low / v_role; standby: usable_s = nominal_duration_s';

    ov = struct();
    ov.speed_override_applied = speedOvrApplied;
    ov.geometry_override_applied = geomOvrApplied;
    fnS = fieldnames(args.SpeedOverride);
    fnG = fieldnames(args.GeomOverride);
    if isempty(fnS)
        ov.speed_override = [];
    else
        ov.speed_override = args.SpeedOverride;
    end
    if isempty(fnG)
        ov.geometry_override = [];
    else
        ov.geometry_override = args.GeomOverride;
    end
    inp.overrides_applied = ov;
end

function ref = registryRef(pathRel, pathAbs, reg)
    sh = getf(reg, 'self_hash', struct());
    ref = struct();
    ref.path_relative_to_model_root = pathRel;
    ref.self_hash_algorithm = getf(sh, 'algorithm', '');
    ref.self_hash_value = getf(sh, 'value', '');
    ref.file_sha256 = fileSha256(pathAbs);
end

% ========================================================================
% markdown report
% ========================================================================
function s = buildMd(doc)
    L = {};
    L = addLine(L, '# T-004 Segment Duration Census and T-429 Slice Geometry Freeze Proposal');
    L = addLine(L, '');
    L = addLine(L, sprintf('Generated: %s (UTC)', doc.generated_at));
    L = addLine(L, 'Method: pure registry arithmetic census; no Model E simulation; no T-429 metric execution.');
    L = addLine(L, 'Authority: .specs/phase4-c-mission-v3/{requirements,design,tasks}.md (FR-547, DGN-12 R8);');
    L = addLine(L, 'delegation spec experiments/mission_v3/T-004_delegation_spec.md.');
    L = addLine(L, 'All values below are PROPOSALS for main-agent adjudication; the freeze happens via T-007.');
    L = addLine(L, '');

    inp = doc.inputs;
    L = addLine(L, '## 1. Inputs');
    L = addLine(L, '');
    L = addLine(L, '| registry | declared self_hash (first 16) | computed file sha256 (first 16) |');
    L = addLine(L, '|---|---|---|');
    sr = inp.source_registries;
    regs = {sr.mission_template_registry, sr.identifiability_contrast_registry, sr.conditional_dag_config};
    regNames = {'mission_template_registry.json', 'identifiability_contrast_registry.json', 'conditional_dag_config.json'};
    for k = 1:3
        rg = regs{k};
        L = addLine(L, sprintf('| %s | %s... | %s... |', regNames{k}, ...
            abbrev(rg.self_hash_value, 16), abbrev(rg.file_sha256, 16)));
    end
    L = addLine(L, '');
    sf = inp.stretch_factor;
    L = addLine(L, sprintf(['- Stretch factor lower bound: %.4f (source: %s, %s); ', ...
        'high = %.4f'], sf.low, sf.source_path_relative_to_model_root, ...
        sf.source_pointer, sf.high));
    gc = inp.geometry_candidate;
    L = addLine(L, sprintf(['- Geometry candidate: slice %g s / edge buffer %g s (both sides) / ', ...
        'stride %g s; hard constraint requires %g s'], gc.slice_duration_s, ...
        gc.edge_buffer_s, gc.stride_s, gc.hard_constraint_required_s));
    L = addLine(L, sprintf('- Usable duration formula: %s', inp.usable_duration_formula));
    L = addLine(L, '- Candidate speed table (m/s, PROPOSAL inputs, not frozen):');
    L = addLine(L, '');
    L = addLine(L, '| role | speed m/s | basis |');
    L = addLine(L, '|---|---|---|');
    st = inp.candidate_speed_table_mps;
    sb = inp.speed_basis;
    fn = fieldnames(st);
    for k = 1:numel(fn)
        v = st.(fn{k});
        if isnan(v)
            vStr = 'n/a (duration-defined)';
        else
            vStr = sprintf('%.3f', v);
        end
        L = addLine(L, sprintf('| %s | %s | %s |', fn{k}, vStr, sb.(fn{k})));
    end
    L = addLine(L, '');

    L = addLine(L, '## 2. Per-template role census (usable duration at stretch low)');
    L = addLine(L, '');
    L = addLine(L, 'Template legend: RP = ROUTINE_PATROL, ER = EMERGENCY_RESPONSE, WM = WORK_MISSION.');
    L = addLine(L, '');
    L = addLine(L, '| template | role | seq | dist m | dur s | speed m/s | usable s | direction |');
    L = addLine(L, '|---|---|---|---|---|---|---|---|');
    for k = 1:numel(doc.per_template_role)
        r = doc.per_template_role{k};
        L = addLine(L, sprintf('| %s | %s | %s | %s | %s | %s | %.3f | %s |', ...
            tAbbr(r.mission_template_id), r.role, numsToStr(r.seq_indices), ...
            numsToStr(r.distances_m), numsToStr(r.durations_s), ...
            numToStr(r.candidate_speed_mps), r.usable_s, r.direction));
    end
    L = addLine(L, '');

    L = addLine(L, '## 3. Role minimum usable duration (cross-template)');
    L = addLine(L, '');
    L = addLine(L, '| role | min usable s | achieved in | present in |');
    L = addLine(L, '|---|---|---|---|');
    for k = 1:numel(doc.role_min_detail)
        d = doc.role_min_detail{k};
        L = addLine(L, sprintf('| %s | %.3f | %s | %s |', d.role, d.min_usable_s, ...
            joinCell(d.achieved_in_templates, ', '), joinCell(d.present_in_templates, ', ')));
    end
    L = addLine(L, '');

    L = addLine(L, '## 4. Required contrast coverage and hard-constraint verdict (candidate geometry)');
    L = addLine(L, '');
    ccv = doc.contrast_coverage;
    L = addLine(L, sprintf('Hard constraint: slice + 2 x buffer = %g s must be <= min usable duration per contrast class.', ...
        ccv.hard_constraint_required_s));
    L = addLine(L, '');
    L = addLine(L, '| contrast | class | hosts (role: templates) | min usable s | required s | verdict |');
    L = addLine(L, '|---|---|---|---|---|---|');
    for k = 1:numel(ccv.required)
        c = ccv.required{k};
        L = addLine(L, sprintf('| %s | A (%s) | %s | %.3f | %g | %s |', ...
            c.contrast_id, c.class_A, hostsLabel(c.class_A_hosts), ...
            c.class_A_min_usable_s, c.hard_constraint_required_s, ...
            passStr(c.class_A_pass)));
        L = addLine(L, sprintf('| %s | B (%s) | %s | %.3f | %g | %s |', ...
            c.contrast_id, c.class_B, hostsLabel(c.class_B_hosts), ...
            c.class_B_min_usable_s, c.hard_constraint_required_s, ...
            passStr(c.class_B_pass)));
    end
    L = addLine(L, '');
    if ~isempty(ccv.excluded)
        L = addLine(L, 'Excluded (not REQUIRED, no constraint obligation):');
        for k = 1:numel(ccv.excluded)
            e = ccv.excluded{k};
            L = addLine(L, sprintf('- %s (%s)', e.contrast_id, e.gate_role));
        end
        L = addLine(L, '');
    end

    L = addLine(L, '## 5. Window arithmetic at candidate geometry');
    L = addLine(L, '');
    L = addLine(L, 'Slices per host at stride: n = floor((usable - 2*buffer - slice) / stride) + 1, 0 if ineligible.');
    L = addLine(L, '');
    L = addLine(L, '| template | role | usable s | eligible | slices |');
    L = addLine(L, '|---|---|---|---|---|');
    for k = 1:numel(doc.window_arithmetic)
        w = doc.window_arithmetic{k};
        L = addLine(L, sprintf('| %s | %s | %.3f | %s | %d |', ...
            tAbbr(w.mission_template_id), w.role, w.usable_s, ...
            boolStr(w.eligible), w.n_slices_at_stride));
    end
    L = addLine(L, '');

    L = addLine(L, '## 6. Proposal');
    L = addLine(L, '');
    pr = doc.proposal;
    L = addLine(L, sprintf('Candidate geometry %g/%g/%g overall verdict: **%s**.', ...
        pr.candidate_geometry.slice_duration_s, pr.candidate_geometry.edge_buffer_s, ...
        pr.candidate_geometry.stride_s, pr.overall_verdict));
    L = addLine(L, '');
    if ~isempty(pr.violating_host_detail)
        L = addLine(L, '### Violating hosts (candidate geometry)');
        L = addLine(L, '');
        L = addLine(L, '| contrast | class | template | role | usable s | deficit s | v max for pass (m/s) | dist needed at candidate speed (m) |');
        L = addLine(L, '|---|---|---|---|---|---|---|---|');
        for k = 1:numel(pr.violating_host_detail)
            v = pr.violating_host_detail{k};
            L = addLine(L, sprintf('| %s | %s | %s | %s | %.3f | %.3f | %.6f | %.1f |', ...
                v.contrast_id, v.class, tAbbr(v.mission_template_id), v.role, ...
                v.usable_s, v.deficit_s, v.max_plan_speed_for_pass_mps, ...
                v.required_distance_at_candidate_speed_m));
        end
        L = addLine(L, '');
    end
    L = addLine(L, '### Remediation candidates');
    L = addLine(L, '');
    for k = 1:numel(pr.remediation_candidates)
        r = pr.remediation_candidates{k};
        L = addLine(L, sprintf('#### %s %s', r.id, r.title));
        L = addLine(L, '');
        gg = r.geometry_candidate;
        L = addLine(L, sprintf('- Geometry: slice %g s / buffer %g s / stride %g s (required %g s)', ...
            gg.slice_duration_s, gg.edge_buffer_s, gg.stride_s, ...
            r.hard_constraint_required_s));
        L = addLine(L, sprintf('- Verdict: %s', r.verdict));
        if ~isempty(r.fails_note)
            L = addLine(L, sprintf('- Fails: %s', r.fails_note));
        end
        if ~isempty(r.flags)
            L = addLine(L, sprintf('- Flags: %s', joinCell(r.flags, '; ')));
        end
        L = addLine(L, '- Arithmetic:');
        for m = 1:numel(r.arithmetic)
            L = addLine(L, sprintf('  - %s', r.arithmetic{m}));
        end
        L = addLine(L, '');
    end
    L = addLine(L, '### Freeze note');
    L = addLine(L, '');
    L = addLine(L, pr.freeze_note);
    L = addLine(L, '');

    L = addLine(L, '## 7. Items for main-agent adjudication');
    L = addLine(L, '');
    for k = 1:numel(pr.main_agent_decision_items)
        L = addLine(L, sprintf('- %s', pr.main_agent_decision_items{k}));
    end
    L = addLine(L, '');

    L = addLine(L, '## 8. Read-only statement');
    L = addLine(L, '');
    L = addLine(L, doc.read_only_statement);
    L = addLine(L, 'Guards enforced (fail-closed, error ids T004:G*):');
    for k = 1:numel(doc.guards_enforced)
        L = addLine(L, sprintf('- %s', doc.guards_enforced{k}));
    end
    L = addLine(L, '');
    s = strjoin(L, newline);
end

function L = addLine(L, line)
    L{end+1} = line; %#ok<AGROW>
end

function s = abbrev(str, n)
    if numel(str) > n
        s = [str(1:n), '...'];
    else
        s = str;
    end
end

function a = tAbbr(t)
    switch t
        case 'ROUTINE_PATROL'
            a = 'RP';
        case 'EMERGENCY_RESPONSE'
            a = 'ER';
        case 'WORK_MISSION'
            a = 'WM';
        otherwise
            a = t;
    end
end

function s = hostsLabel(hosts)
% group host records by role -> "role (T1, T2); role2 (T3)"
    roles = {};
    tlists = {};
    for k = 1:numel(hosts)
        h = hosts{k};
        idx = find(strcmp(roles, h.role), 1);
        if isempty(idx)
            roles{end+1} = h.role; %#ok<AGROW>
            tlists{end+1} = tAbbr(h.mission_template_id); %#ok<AGROW>
        else
            tlists{idx} = [tlists{idx}, ', ', tAbbr(h.mission_template_id)]; %#ok<AGROW>
        end
    end
    parts = cell(1, numel(roles));
    for k = 1:numel(roles)
        parts{k} = sprintf('%s (%s)', roles{k}, tlists{k});
    end
    s = strjoin(parts, '; ');
end

function s = numsToStr(v)
    if isempty(v)
        s = '-';
        return;
    end
    parts = cell(1, numel(v));
    for k = 1:numel(v)
        parts{k} = numToStr(v(k));
    end
    s = strjoin(parts, ', ');
end

function s = numToStr(v)
    if isnan(v)
        s = '-';
    elseif v == floor(v)
        s = sprintf('%d', round(v));
    else
        s = sprintf('%g', v);
    end
end

function s = boolStr(b)
    if b
        s = 'yes';
    else
        s = 'no';
    end
end

function s = passStr(p)
    if p
        s = 'PASS';
    else
        s = 'FAIL';
    end
end

function s = joinCell(c, sep)
    if isempty(c)
        s = '';
        return;
    end
    s = strjoin(c, sep);
end

function writeText(fp, s)
    fid = fopen(fp, 'w');
    if fid == -1
        error('T004:WRITE_FAILED', 'T004: cannot open %s for writing', fp);
    end
    fwrite(fid, s, 'char');
    fclose(fid);
end
