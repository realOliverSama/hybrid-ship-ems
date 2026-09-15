function varargout = checkpoint_mgr(cmd, varargin)
% ppe.checkpoint_mgr — D8 segmented checkpoint identity.
%
% Identity binds: arm; voyage_id; t0 / segment index; study config SHA;
% code commit SHA; Model-E .slx SHA; C* implementation SHA; ForecastWindow
% provenance/hash; hard-feasibility config SHA; corpus identity SHA;
% canonical T_eval identity; Predictor-v3 model/config/norm SHAs for C2 or
% explicit 'N/A' for C0/C1.
%
% Usage:
%   id  = ppe.checkpoint_mgr('identity', ctx)        % ctx: struct of fields
%   ppe.checkpoint_mgr('save', path, op, id, extra)
%   rec = ppe.checkpoint_mgr('load', path, expected_id)  % refuses mismatch
switch lower(cmd)
    case 'identity'
        varargout{1} = local_identity(varargin{1});
    case 'save'
        local_save(varargin{:});
    case 'load'
        varargout{1} = local_load(varargin{:});
    otherwise
        error('PPE:CKPT:CMD', 'unknown cmd %s', cmd);
end
end

function id = local_identity(ctx)
req = {'arm', 'voyage_id', 'segment_t0', 'segment_index', ...
    'study_config_sha256', 'code_commit_sha', 'slx_sha256', ...
    'cstar_impl_sha256', 'forecast_window_sha256', 'hf_config_sha256', ...
    'corpus_identity_sha256', 'teval_identity_sha256', ...
    'predictor_identity'};
for i = 1:numel(req)
    if ~isfield(ctx, req{i})
        error('PPE:CKPT:FIELD', 'missing identity field %s', req{i});
    end
end
id = struct();
id.schema = 'ppe_cstar_checkpoint_identity_v1';
for i = 1:numel(req)
    id.(req{i}) = ctx.(req{i});
end
% predictor identity: struct of SHAs for C2 or the string 'N/A'
if isstruct(id.predictor_identity)
    pf = id.predictor_identity;
    preq = {'keras_sha256', 'selected_config_sha256', ...
        'formal_norm_v3_sha256', 'predictor_norm_ext_v1_sha256'};
    for j = 1:numel(preq)
        if ~isfield(pf, preq{j})
            error('PPE:CKPT:PRED', 'missing predictor field %s', preq{j});
        end
    end
elseif ~(ischar(id.predictor_identity) && strcmp(id.predictor_identity, 'N/A'))
    error('PPE:CKPT:PRED', 'predictor_identity must be struct or ''N/A''');
end
id.identity_sha256 = local_hash(id);
end

function h = local_hash(id)
if isfield(id, 'identity_sha256')
    id = rmfield(id, 'identity_sha256');
end
s = jsonencode(id);
md = java.security.MessageDigest.getInstance('SHA-256');
d = typecast(md.digest(typecast(uint8(s), 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
end

function local_save(path, op, id, extra)
d = fileparts(path);
if ~exist(d, 'dir'), mkdir(d); end
payload = struct('identity', id, 'op', op, 'extra', extra);
save(path, 'payload', '-v7.3');
end

function rec = local_load(path, expected_id)
S = load(path, 'payload');
rec = S.payload;
if nargin >= 2 && ~isempty(expected_id)
    a = rec.identity;
    b = expected_id;
    if ~strcmp(a.identity_sha256, b.identity_sha256)
        error('PPE:CKPT:MISMATCH', ...
            'checkpoint identity refused: stored=%s expected=%s', ...
            a.identity_sha256, b.identity_sha256);
    end
end
end
