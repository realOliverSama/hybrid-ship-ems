function M = audit_matrix(ds, cfg)
% t445.audit_matrix — 12 类信号实例化（DGN-F-12）；合成/真 logsout 共用
if nargin < 2 || isempty(cfg)
    cfg = phase4f_t445_config();
end
spec = cfg.audit_capability_matrix;
if iscell(spec), spec = [spec{:}]; end
need = { ...
    't', 'SOC', 'fuel_total_kg', 'P_load_total_kW', 'P_gen_actual_kW', ...
    'P_bat_actual_kW', 'V_dc_V', 'V_bat_V', 'I_bat_A', ...
    'P_bat_dis_avail_kW', 'P_bat_chg_avail_kW', 'state_DG1', 'state_DG2', ...
    'v_ref_mps', 'v_ship_mps'};
M = struct();
M.schema = 't445_audit_capability_instance_v1';
M.n = numel(spec);
M.rows = spec;
missing = {};
for i = 1:numel(need)
    if ~isfield(ds, need{i})
        missing{end+1} = need{i}; %#ok<AGROW>
    end
end
M.missing_fields = missing;
unplanned = {};
for i = 1:numel(spec)
    st = spec(i).status;
    if ~any(strcmp(st, {'AVAILABLE_AND_CHECKED', 'NOT_INSTRUMENTED', 'NOT_APPLICABLE'}))
        unplanned{end+1} = spec(i).signal; %#ok<AGROW>
    end
    if strcmp(st, 'NOT_INSTRUMENTED') && isfield(spec(i), 'hard') && spec(i).hard
        unplanned{end+1} = spec(i).signal; %#ok<AGROW>
    end
end
M.unplanned_not_instrumented = unique(unplanned);
M.closed = isempty(unplanned) && all(strcmp({spec.status}, 'AVAILABLE_AND_CHECKED'));
end
