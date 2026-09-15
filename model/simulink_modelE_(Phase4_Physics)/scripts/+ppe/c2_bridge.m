function bridge = c2_bridge(token)
% ppe.c2_bridge — MATLAB <-> python C2 inference file bridge (TG7-bound
% transport). The python server (ppe.c2_server) owns ALL scientific
% transforms (frozen aggregation, norms, keras); this side is mechanical.
%
% bridge = struct with function handles:
%   Pbar = bridge.request(voyage_id, t0, t_raw, cols)  % cols: struct of 11 H1
%   bridge.stop()
%
% Determinism: response provenance carries keras/norm SHAs; mismatch errors.
ROOT = ppe.ppe_root();
base = fullfile(ROOT, 'artifacts', 'ppe_bridge', token);
reqdir = fullfile(base, 'req');
respdir = fullfile(base, 'resp');
stopf = fullfile(base, 'STOP');
if ~exist(reqdir, 'dir'), mkdir(reqdir); end
if ~exist(respdir, 'dir'), mkdir(respdir); end
if exist(stopf, 'file'), delete(stopf); end

venv_py = fullfile(fileparts(ROOT), '..', '.venv_tf', 'bin', 'python');
if ~exist(venv_py, 'file')
    venv_py = fullfile(fileparts(ROOT), '.venv_tf', 'bin', 'python');
end
if ~exist(venv_py, 'file')
    error('PPE:BRIDGE:PY', 'cannot locate .venv_tf python');
end
pydir = fullfile(ROOT, 'python');
env = sprintf('PYTHONPATH="%s:%s" TF_CPP_MIN_LOG_LEVEL=3 ', pydir, ROOT);
logf = fullfile(base, 'server.log');
cmd = sprintf('%s"%s" -m ppe.c2_server --reqdir "%s" --respdir "%s" --stop-file "%s" > "%s" 2>&1 &', ...
    env, venv_py, reqdir, respdir, stopf, logf);
system(cmd);

bridge = struct();
bridge.request = @(vid, t0, t_raw, cols) local_request(reqdir, respdir, vid, t0, t_raw, cols);
bridge.stop = @() local_stop(stopf);
bridge.reqdir = reqdir;
bridge.respdir = respdir;
end

function Pbar = local_request(reqdir, respdir, vid, t0, t_raw, cols)
req = struct('voyage_id', char(vid), 't0', double(t0), 't', t_raw(:));
fn = fieldnames(cols);
for i = 1:numel(fn)
    req.(['H1_' fn{i}]) = cols.(fn{i})(:);
end
key = sprintf('%s_%.0f', char(vid), t0);
reqf = fullfile(reqdir, ['req_' key '.mat']);
respf = fullfile(respdir, ['resp_' key '.mat']);
save(reqf, '-struct', 'req', '-v7');
t_deadline = tic;
while true
    if exist(respf, 'file')
        pause(0.2);   % writer flush guard
        S = load(respf);
        break;
    end
    if toc(t_deadline) > 300
        error('PPE:BRIDGE:TIMEOUT', 'no response for %s', key);
    end
    pause(0.2);
end
if isfield(S, 'error')
    error('PPE:BRIDGE:REMOTE', 'server fail-closed for %s: %s', key, S.error);
end
Pbar = S.Pbar_q50(:);
prov = S.provenance;
if isstruct(prov)
    expected = 'd8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894';
    if isfield(prov, 'keras_sha256') && ~strcmp(prov.keras_sha256, expected)
        error('PPE:BRIDGE:IDENTITY', 'response keras SHA mismatch');
    end
end
if numel(Pbar) ~= 30 || any(~isfinite(Pbar))
    error('PPE:BRIDGE:SHAPE', 'Pbar contract broken for %s', key);
end
end

function local_stop(stopf)
fid = fopen(stopf, 'w');
if fid > 0, fwrite(fid, 'stop'); fclose(fid); end
end
