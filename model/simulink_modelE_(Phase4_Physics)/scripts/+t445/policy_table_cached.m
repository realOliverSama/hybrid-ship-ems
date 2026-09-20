function tbl = policy_table_cached(cfg, params)
% t445.policy_table_cached — artifacts/t445 本地缓存（不入库）
ROOT = t445.modele_root();
cdir = fullfile(ROOT, 'artifacts', 't445');
if ~exist(cdir, 'dir'), mkdir(cdir); end
p = fullfile(cdir, 'policy_table.mat');
if exist(p, 'file')
    S = load(p, 'tbl');
    if isfield(S, 'tbl') && isfield(S.tbl, 'config_sha256') ...
            && strcmp(S.tbl.config_sha256, cfg.config_sha256) ...
            && isfield(S.tbl, 'n_lambda') && S.tbl.n_lambda == 3020
        tbl = S.tbl;
        tbl.oob_query_count = 0;
        return;
    end
end
if nargin < 2 || isempty(params)
    params = ecms2.ecms_params();
end
tbl = t445.build_policy_table(cfg, params);
save(p, 'tbl', '-v7.3');
end
