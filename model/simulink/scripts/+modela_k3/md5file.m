function h = md5file(path)
% modela_k3.md5file — 文件内容 MD5（用于 parameter_file_hash 等）
    fis = java.io.FileInputStream(path);
    md = java.security.MessageDigest.getInstance('MD5');
    buf = zeros(1, 8192, 'int8');
    n = fis.read(buf);
    while n > 0
        md.update(buf(1:n));
        n = fis.read(buf);
    end
    fis.close();
    h = sprintf('%02x', typecast(md.digest(), 'uint8'));
end
