function out = rebuild_voyage(plan, ctx, vrow)
% ppe.rebuild_voyage — deterministic rebuild of one frozen Dataset-v3
% formal voyage scenario from its pass2-ledger coordinates.
%
% Replicates the scenario-construction half of fleet_v3.member_exec
% verbatim (env_realize sample at the frozen base_draw_index with
% MaxAttempts=1 -> dag/rl -> family_expand -> member k -> plan_compose ->
% scenario_compile). NO simulation here. Deterministic: same coordinates
% -> same scn (proven by ppe.qual_replay_parity).
%
% vrow fields: voyage_id, stratum_id, base_draw_index, member_id,
%   traffic_override ('ON'/'OFF').
%
% out: scn, comp, dag, rl, mem, k, voyage_id.
if ~isstruct(vrow) || ~isfield(vrow, 'voyage_id')
    error('PPE:REBUILD:ARG', 'vrow must carry pass2-ledger coordinates');
end
parts = strsplit(char(vrow.stratum_id), '__');
if numel(parts) ~= 2
    error('PPE:REBUILD:STRATUM', 'bad stratum %s', char(vrow.stratum_id));
end
tpl = strrep(char(parts{1}), 'STRAT_', '');
reg = char(parts{2});
st = char(vrow.stratum_id);
bdi = double(vrow.base_draw_index);
mk = strsplit(char(vrow.member_id), '#M');
k = str2double(mk{end});
seed = char(plan.iid.seed);

s = mission_v3.env_realize('sample', seed, bdi, tpl, reg, st, ...
    'MaxAttempts', 1);
led = s.ledger;
acc = led(s.n_attempts);
dag = acc.draw;
rl = s.realization;
fam = mission_v3.family_expand('expand', dag, 'Realization', rl);
mem = fam.members(k);

use_rl = rl;
if isfield(mem, 'traffic_override') && strcmp(char(mem.traffic_override), 'OFF')
    use_rl.traffic = rl.traffic([]);
end
if isfield(vrow, 'traffic_override') ...
        && strcmp(char(vrow.traffic_override), 'OFF')
    use_rl.traffic = rl.traffic([]);
end

plan_cmp = mission_v3.plan_compose('compose', use_rl, 'OODMode', false);
comp = mission_v3.scenario_compile('compile', plan_cmp, use_rl);

% T-011 caller contract (verbatim from member_exec)
p_base = dag.p_hotel_base_kW;
d_hotel = dag.delta_p_hotel_unplanned;
expect_hotel = double(p_base) + double(d_hotel);
if abs(double(comp.scn.hotel_kW) - expect_hotel) > 1e-9
    error('PPE:REBUILD:HOTEL_GUARD', ...
        'voyage %s: scn.hotel_kW != p_hotel_base + delta', ...
        char(vrow.voyage_id));
end

out = struct();
out.scn = comp.scn;
out.comp = comp;
out.dag = dag;
out.rl = rl;
out.mem = mem;
out.k = k;
out.voyage_id = char(vrow.voyage_id);
end
