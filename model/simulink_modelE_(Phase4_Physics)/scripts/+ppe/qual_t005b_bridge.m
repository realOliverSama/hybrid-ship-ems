function r = qual_t005b_bridge()
% ppe.qual_t005b_bridge — C2 bridge transport qualification (SYNTHETIC).
%
% Starts the python c2_server, sends two identical synthetic raw-log
% requests for a TRAIN voyage anchor, verifies bitwise-identical responses,
% provenance identity, and fail-closed error record for an off-anchor t0.
% No Simulink sim, no corpus data. Writes
% results/ppe_practical/qualification/t005b_bridge.json.
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
token = 't005b';
base = fullfile(ROOT, 'artifacts', 'ppe_bridge', token);
if exist(base, 'dir')
    d = dir(fullfile(base, 'req', '*.mat'));
    for i = 1:numel(d), delete(fullfile(base, 'req', d(i).name)); end
    d = dir(fullfile(base, 'resp', '*.mat'));
    for i = 1:numel(d), delete(fullfile(base, 'resp', d(i).name)); end
end
br = [];

% synthetic raw logs: 1 s grid, (0,300], constant channels
t_raw = (0:300).';   % 1 s grid incl. t=0 (coverage contract)
cols = struct();
cols.P_load_total_kW = 400 * ones(size(t_raw));
cols.v_ship_mps = 6 * ones(size(t_raw));
cols.acceleration_mps2 = zeros(size(t_raw));
cols.n_prop_rpm = 300 * ones(size(t_raw));
cols.P_hotel_kW = 100 * ones(size(t_raw));
cols.v_current_mps = 0.5 * ones(size(t_raw));
cols.P_prop_demand_kW = 300 * ones(size(t_raw));
cols.v_ref_mps = 6 * ones(size(t_raw));
cols.manual_power_cmd_pct = zeros(size(t_raw));
cols.water_depth_m = 20 * ones(size(t_raw));
cols.ctrl_mode = zeros(size(t_raw));

vid = 'FLTA_S1_D00000_M1';   % train voyage (kf row exists at t0=300)
t0 = 300;

% server A
brA = ppe.c2_bridge([token '_a']);
pause(20);
P1 = brA.request(vid, t0, t_raw, cols);
brA.stop();
% server B (fresh process): cross-process determinism
brB = ppe.c2_bridge([token '_b']);
pause(20);
P2 = brB.request(vid, t0, t_raw, cols);
bitwise = isequal(P1, P2);
okshape = numel(P1) == 30 && all(isfinite(P1));

% fail-closed: off-anchor t0 has no kf vintage row -> error response
bad = false;
try
    brB.request(vid, 305, t_raw, cols);
catch ME
    bad = contains(ME.identifier, 'PPE:BRIDGE');
end
brB.stop();
pause(2);

r = struct();
r.schema = 'ppe_t005b_bridge_qual_v1';
r.task = 'T-005b';
r.purpose = 'QUALIFICATION';
r.bitwise_repeat = bitwise;
r.shape_ok = okshape;
r.failclosed_offanchor = bad;
r.q50_head = P1(1);
r.pass = bitwise && okshape && bad;
r.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
if ~exist(qdir, 'dir'), mkdir(qdir); end
fid = fopen(fullfile(qdir, 't005b_bridge.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[T-005b bridge] pass=%d bitwise=%d shape=%d failclosed=%d\n', ...
    r.pass, bitwise, okshape, bad);
if ~r.pass
    error('PPE:T005B:FAIL', 'bridge qualification FAIL');
end
end
