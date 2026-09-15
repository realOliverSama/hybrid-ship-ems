function incident_log(row)
% t445.incident_log — 追加 incidents.jsonl（FR-777，禁删）
ROOT = t445.modele_root();
p = fullfile(ROOT, 'results', 't445_oracle_gate', 'incidents.jsonl');
if ~isfield(row, 'utc')
    row.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'));
end
fid = fopen(p, 'a');
assert(fid > 0, 'T445:INCIDENT:WRITE');
fprintf(fid, '%s\n', jsonencode(row));
fclose(fid);
end
