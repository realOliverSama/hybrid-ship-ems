function [skip, rec, reason] = formal_pack_skip_v2(ev, fz2, voyage_id, arm)
% ppe.formal_pack_skip_v2 — epoch-2 幂等跳过判定（恢复授权 §3，纯函数）。
%
% 只有当磁盘上的终端记录满足【全部】epoch-2 身份要求时才允许 skip=true：
%   - formal_epoch == 2
%   - run_id == 期望值（由 arm|voyage_id 确定性派生）
%   - voyage_id / arm 与请求一致
%   - freeze_root_sha256 == 当前 epoch-2 冻结字节根
%   - cstar_impl_sha256 == 当前 C* 复合身份
%   - checker_v2.sha256 == 冻结绑定的 checker v2 身份
%   - terminal_status 为 COMPLETE*
%   - trajectory_sha256 非空，且轨迹工件存在且字节哈希一致
%   - audit_sha256 非空，且审计工件存在且字节哈希一致
%
% 任一不满足 → skip=false + reason（pack 将先隔离陈旧证据再原子重跑）。
% epoch-1 记录位于不同命名空间且从不被本函数读取：对 epoch-2 完成判定
% 不可见（Q-E2E-10）。失配记录绝不静默计为完成（Q-E2E-11/12）。

skip = false;
rec = [];
reason = 'absent';
if ~exist(ev, 'file')
    return;
end
try
    rec = jsondecode(fileread(ev));
catch
    rec = [];
    reason = 'unparsable';
    return;
end
if ~isstruct(rec)
    reason = 'not-a-struct';
    return;
end

req = {@(r) isfield(r, 'formal_epoch') && r.formal_epoch == 2, ...
    @(r) isfield(r, 'run_id') && strcmp(char(r.run_id), local_run_id(arm, voyage_id)), ...
    @(r) isfield(r, 'voyage_id') && strcmp(char(r.voyage_id), char(voyage_id)), ...
    @(r) isfield(r, 'arm') && strcmp(char(r.arm), char(arm)), ...
    @(r) isfield(r, 'freeze_root_sha256') ...
        && strcmp(char(r.freeze_root_sha256), fz2.freeze_root_sha256), ...
    @(r) isfield(r, 'cstar_impl_sha256') ...
        && strcmp(char(r.cstar_impl_sha256), fz2.cstar_composite_sha256), ...
    @(r) isfield(r, 'checker_v2') && isstruct(r.checker_v2) ...
        && isfield(r.checker_v2, 'sha256') ...
        && strcmp(char(r.checker_v2.sha256), fz2.checker_v2_sha256), ...
    @(r) isfield(r, 'terminal_status') ...
        && strncmp(char(r.terminal_status), 'COMPLETE', 8), ...
    @(r) isfield(r, 'trajectory_sha256') && ~isempty(char(r.trajectory_sha256)), ...
    @(r) isfield(r, 'audit_sha256') && ~isempty(char(r.audit_sha256)), ...
    @(r) isfield(r, 'controller_trace_sha256') ...
        && ~isempty(char(r.controller_trace_sha256))};
names = {'formal_epoch', 'run_id', 'voyage_id', 'arm', 'freeze_root', ...
    'cstar_composite', 'checker_v2_identity', 'terminal_status', ...
    'trajectory_field', 'audit_field', 'controller_trace_field'};
for i = 1:numel(req)
    if ~req{i}(rec)
        reason = sprintf('identity-mismatch:%s', names{i});
        return;
    end
end

% ---- 工件存在 + 字节哈希一致（缺失/损坏 = 不算完成）----
vdir = fileparts(ev);
tpath = fullfile(vdir, [char(arm) '.traj.v1.mat']);
apath = fullfile(vdir, [char(arm) '.audit.v1.json']);
cpath = fullfile(vdir, [char(arm) '.ctrace.v1.json']);
if ~exist(tpath, 'file')
    reason = 'artifact-missing:trajectory';
    return;
end
if ~strcmp(model.sha256_file(tpath), char(rec.trajectory_sha256))
    reason = 'artifact-hash-mismatch:trajectory';
    return;
end
if ~exist(apath, 'file')
    reason = 'artifact-missing:audit';
    return;
end
if ~strcmp(model.sha256_file(apath), char(rec.audit_sha256))
    reason = 'artifact-hash-mismatch:audit';
    return;
end
if ~exist(cpath, 'file')
    reason = 'artifact-missing:controller_trace';
    return;
end
if ~strcmp(model.sha256_file(cpath), char(rec.controller_trace_sha256))
    reason = 'artifact-hash-mismatch:controller_trace';
    return;
end

skip = true;
reason = 'complete-identity-match';
end

function rid = local_run_id(arm, vid)
md = java.security.MessageDigest.getInstance('SHA-256');
raw = uint8(sprintf('ppe|%s|%s', char(arm), char(vid)));
d = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
rid = h(1:16);
end
