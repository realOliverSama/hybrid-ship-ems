function hex = sha256_doubles(x)
% t445.sha256_doubles — 列向量 double 的 SHA-256（流身份，非科学哈希算法变体）
x = x(:);
raw = typecast(x, 'uint8');
md = java.security.MessageDigest.getInstance('SHA-256');
digest = typecast(md.digest(typecast(raw, 'int8')), 'uint8');
hex = lower(reshape(dec2hex(digest, 2)', 1, []));
end
