function h = cstar_impl_sha()
% ppe.cstar_impl_sha — canonical SHA-256 over the C* implementation file set
% (D8/TG7 binding). Same algorithm on the python freeze side.
ROOT = ppe.ppe_root();
files = ppe.cstar_impl_files();
md = java.security.MessageDigest.getInstance('SHA-256');
for i = 1:numel(files)
    fs = model.sha256_file(fullfile(ROOT, files{i}));
    md.update(typecast(uint8([files{i}, ':', fs, sprintf('\n')]), 'int8'));
end
d = typecast(md.digest(), 'uint8');
h = lower(reshape(dec2hex(d, 2)', 1, []));
end
