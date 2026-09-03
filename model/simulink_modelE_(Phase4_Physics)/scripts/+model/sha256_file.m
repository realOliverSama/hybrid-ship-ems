function hex = sha256_file(path)
% model.sha256_file — 文件 SHA-256（hex 字符串），用于 config manifest / 零改动证据链
if nargin < 1 || isempty(path) || ~exist(path, 'file')
    error('sha256_file:InvalidPath', '文件不存在: %s', string(path));
end
md = java.security.MessageDigest.getInstance('SHA-256');
fid = fopen(path, 'r');
raw = fread(fid, inf, 'uint8=>uint8');
fclose(fid);
jbytes = typecast(raw, 'int8');      % 位保持转换（int8() 会饱和，禁用）
digest = typecast(md.digest(jbytes), 'uint8');
hex = lower(reshape(dec2hex(digest, 2)', 1, []));
end