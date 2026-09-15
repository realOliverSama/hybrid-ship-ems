function winfun = winfun_plan(plan_file, arm, opts)
% ppe.winfun_plan — formal ForecastWindow provider factory (DGN-PPE-02/12).
%
% Follows a python-generated per-voyage update plan (hash-bound at freeze).
% The plan decides the ACTION at each t0; this function only executes:
%   warmup_c0 -> C0 persistence window from this arm's own logs (all arms)
%   anchor    -> arm source: C0 persistence / C1 stored windows file /
%                C2 bridge inference
%   tail      -> C0 persistence update (legal always) / C1,C2 hold (ZOH)
%
% The C* runner never branches on source; all source policy lives here.
%
% opts fields:
%   T_eval          canonical voyage horizon (from frozen T_eval table)
%   c1_windows_file .mat with anchors (Nx1) and Y (Nx30) from the frozen
%                   canonical store (exported by python at formal prep)
%   c2_bridge       ppe.c2_bridge handle (C2 only)
%   water_depth_m   scenario constant (H1 channel; Model-E unlogged constant)
%   ctrl_mode       0=AUTO / 1=MANUAL compile-time constant
%
% Returned handle: [Pbar, prov, use_previous] = winfun(t0, ds, p0)
if nargin < 3, opts = struct(); end
doc = jsondecode(fileread(plan_file));
% accept both the exported wrapper document (schema/plan_sha256/plan) and a
% bare plan array; the update-plan array is what carries t0/action
if isstruct(doc) && isfield(doc, 'plan')
    plan = doc.plan;
else
    plan = doc;
end
T_eval = opts.T_eval;
K0 = 30;

    function [Pbar, prov, use_prev] = impl(t0, ds, p0)
        act = plan_action(plan, t0);
        use_prev = false;
        Pbar = [];
        prov = struct('source_id', '', 't0', t0);
        switch act
            case 'tail'
                if strcmp(arm, 'C0')
                    [Pbar, prov] = local_c0(t0, T_eval, ds, p0, K0);
                    return;
                end
                use_prev = true;
                prov.source_id = 'ZOH_HOLD';
                return;
            case {'warmup_c0', 'anchor'}
                % below: source selection inside the anchor branch
            otherwise
                error('PPE:WINFUN:ACTION', 'unknown action %s', act);
        end

        if strcmp(act, 'warmup_c0') || strcmp(arm, 'C0')
            [Pbar, prov] = local_c0(t0, T_eval, ds, p0, K0);
            return;
        end
        switch arm
            case 'C1'
                S = load(opts.c1_windows_file);   % anchors, Y
                iw = find(abs(S.anchors - t0) < 1e-9, 1, 'first');
                if isempty(iw)
                    error('PPE:WINFUN:C1MISS', ...
                        'C1 stored window missing at t0=%g — FAIL CLOSED', t0);
                end
                y = S.Y(iw, :).';
                K = min(K0, max(1, ceil((T_eval - t0) / 10 - 1e-12)));
                Pbar = y(1:K);
                prov.source_id = 'C1_TRUTH';
            case 'C2'
                if ~isfield(opts, 'c2_bridge') || isempty(opts.c2_bridge)
                    error('PPE:WINFUN:BRIDGE', 'C2 requires live bridge');
                end
                [t_raw, cols] = local_history(ds, p0, t0, opts);
                Pbar = opts.c2_bridge.request(opts.voyage_id, t0, t_raw, cols);
                K = min(K0, max(1, ceil((T_eval - t0) / 10 - 1e-12)));
                Pbar = Pbar(1:K);
                prov.source_id = 'C2_REATT_Q50';
            otherwise
                error('PPE:WINFUN:ARM', 'arm %s illegal', arm);
        end
    end

winfun = @impl;
end

function a = plan_action(plan, t0)
% plan: decoded JSON array of structs with numeric t0 and char action
idx = find(abs([plan.t0] - t0) < 1e-9, 1, 'first');
if isempty(idx)
    error('PPE:WINFUN:PLAN', 't0=%g absent from update plan', t0);
end
a = plan(idx).action;
end

function [Pbar, prov] = local_c0(t0, T_eval, ds, p0, K0)
% FR-821: mean of most recently completed [t0-10, t0) bin; t0<10 -> p0.
if t0 >= 10 - 1e-9
    if isempty(ds) || ~isfield(ds, 'P_load_total_kW')
        error('PPE:WINFUN:HIST', 'C0 requires arm-local history at t0=%g', t0);
    end
    mask = ds.t > t0 - 10 - 1e-9 & ds.t <= t0 + 1e-9;
    if ~any(mask)
        error('PPE:WINFUN:BIN', 'no samples in completed bin at t0=%g', t0);
    end
    pp = mean(ds.P_load_total_kW(mask));
else
    if isempty(p0)
        error('PPE:WINFUN:P0', 'p0 probe missing at t0=%g', t0);
    end
    pp = p0;
end
K = min(K0, max(1, ceil((T_eval - t0) / 10 - 1e-12)));
Pbar = pp * ones(K, 1);
prov = struct('source_id', 'C0_PERSISTENCE', 't0', t0, 'p_persist', pp);
end

function [t_raw, cols] = local_history(ds, p0, t0, opts)
% Arm-local raw logs over (t0-300, t0] for the C2 bridge. water_depth_m and
% ctrl_mode are compile-time/scenario constants (registry notes); supplied
% from opts, not from logs.
if isempty(ds)
    error('PPE:WINFUN:H1', 'C2 anchor at t0=%g without history', t0);
end
mask = ds.t > t0 - 300 - 1e-9 & ds.t <= t0 + 1e-9;
t_raw = ds.t(mask);
chans = {'P_load_total_kW', 'v_ship_mps', 'acceleration_mps2', 'n_prop_rpm', ...
    'P_hotel_kW', 'v_current_mps', 'P_prop_demand_kW', 'v_ref_mps', ...
    'manual_power_cmd_pct'};
cols = struct();
for i = 1:numel(chans)
    nm = chans{i};
    if ~isfield(ds, nm)
        error('PPE:WINFUN:CH', 'ds missing channel %s', nm);
    end
    cols.(nm) = ds.(nm)(mask);
end
cols.water_depth_m = opts.water_depth_m * ones(size(t_raw));
cols.ctrl_mode = opts.ctrl_mode * ones(size(t_raw));
end
