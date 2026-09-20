function varargout = simstate_mgr(cmd, varargin)
% t445.simstate_mgr — SimState/OperatingPoint 七元组身份（DGN-F-10）
switch lower(cmd)
    case 'identity'
        varargout{1} = local_identity(varargin{:});
    case 'checksum'
        varargout{1} = local_checksum();
    case 'save'
        local_save(varargin{:});
    case 'load'
        varargout{1} = local_load(varargin{:});
    otherwise
        error('T445:SIMSTATE:CMD', 'unknown cmd %s', cmd);
end
end

function id = local_identity(voyage_id, arm, cfg, stream_sha, segment_t0, seed)
if nargin < 6 || isempty(seed)
    seed = 'phase4f-t445-toy-smoke-v1';
end
cs = local_checksum();
id = struct();
id.voyage_id = char(voyage_id);
id.arm = char(arm);
id.config_hash = cfg.config_sha256;
id.seed = char(seed);
id.frozen_stream_sha256 = char(stream_sha);
id.model_checksum = cs.combined;
id.slx_sha256 = cs.slx;
id.builder_sha256 = cs.builder;
id.segment_t0 = segment_t0;
end

function cs = local_checksum()
ROOT = t445.modele_root();
slx = fullfile(ROOT, 'model', 'HybridShip_ModelE.slx');
bld = fullfile(ROOT, 'scripts', '+model', 'build_model_e.m');
cs = struct();
cs.slx = model.sha256_file(slx);
cs.builder = model.sha256_file(bld);
md = java.security.MessageDigest.getInstance('SHA-256');
raw = uint8([cs.slx, '|', cs.builder]);
digest = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
cs.combined = lower(reshape(dec2hex(digest, 2)', 1, []));
end

function local_save(path, op, identity, extra)
d = fileparts(path);
if ~exist(d, 'dir'), mkdir(d); end
payload = struct('identity', identity, 'op', op, 'extra', extra, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z')));
save(path, 'payload', '-v7.3');
end

function rec = local_load(path)
S = load(path, 'payload');
rec = S.payload;
end
