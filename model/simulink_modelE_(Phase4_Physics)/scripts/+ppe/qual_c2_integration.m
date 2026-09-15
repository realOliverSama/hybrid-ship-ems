function qual_c2_integration()
% ppe.qual_c2_integration — NONFORMAL C2 bridge integration smoke (QUALIFICATION).
%
% Exercises the exact C2 path used by run_formal_voyage WITHOUT touching the
% formal corpus or producing formal outputs: stale-traffic cleanup, server
% launch on a dedicated qualification token, one bridge request with
% synthetic H1 channels (300 s @ 1 Hz), provenance keras-sha fail-closed
% check, and bridge.stop() lifecycle.
ROOT = ppe.ppe_root();
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
TOKEN = 'qual-c2-integration';
bdir = fullfile(ROOT, 'artifacts', 'ppe_bridge', TOKEN);
% stale-traffic cleanup (same logic as run_formal_voyage C2 branch)
for sub = {'req', 'resp'}
    d = fullfile(bdir, sub{1});
    if exist(d, 'dir')
        fl = dir(fullfile(d, [sub{1} '_*.mat']));
        for j = 1:numel(fl), delete(fullfile(fl(j).folder, fl(j).name)); end
    end
end
br = ppe.c2_bridge(TOKEN);
cleanup_br = onCleanup(@() br.stop());
t_raw = (0:0.5:300).';   % 601 samples, causal (<= t0)
cols = struct();
chans = {'P_load_total_kW', 'v_ship_mps', 'acceleration_mps2', ...
    'n_prop_rpm', 'P_hotel_kW', 'v_current_mps', 'P_prop_demand_kW', ...
    'v_ref_mps', 'manual_power_cmd_pct'};
vals = [420, 3.2, 0.01, 120, 100, 0.3, 380, 3.2, 0];
for i = 1:numel(chans)
    cols.(chans{i}) = vals(i) + 0.1 * sin(2 * pi * t_raw / 600);
end
cols.water_depth_m = 20 * ones(size(t_raw));
cols.ctrl_mode = zeros(size(t_raw));
t0 = tic;
% real formal voyage id: the server pulls F channels from the canonical
% frozen store and fail-closed rejects any other id (synthetic ids were
% verified to be rejected before this test was finalized)
Pbar = br.request('FLTA_S2_D01004_M1', 300, t_raw, cols);
dt_req = toc(t0);
r = struct();
r.schema = 'ppe_c2_integration_qual_v1';
r.purpose = 'QUALIFICATION';
r.token = TOKEN;
r.n_request = 1;
r.Pbar_n = numel(Pbar);
r.Pbar_all_finite = all(isfinite(Pbar));
r.Pbar_head = Pbar(1);
r.request_wall_s = dt_req;
r.pass = numel(Pbar) == 30 && all(isfinite(Pbar));
r.utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
clear cleanup_br;   % triggers bridge.stop() (STOP file)
qdir = fullfile(ROOT, 'results', 'ppe_practical', 'qualification');
fid = fopen(fullfile(qdir, 'c2_integration_qual.json'), 'w');
fwrite(fid, jsonencode(r, 'PrettyPrint', true));
fclose(fid);
fprintf('[c2-integration] pass=%d Pbar_n=%d wall=%.2fs\n', ...
    r.pass, r.Pbar_n, dt_req);
if ~r.pass
    error('PPE:C2INT:FAIL', 'C2 integration qualification FAIL');
end
end
