function [il, ip, ir, oob] = policy_lookup(tbl, lambda, Pbar, SOC)
% t445.policy_lookup — λ 最近邻 + P̄ 最近邻 + SOC 三区。越域只计数，不外推新点。
oob = 0;
lam = lambda;
if lam < tbl.lambda_table_min - 1e-12 || lam > tbl.lambda_table_max + 1e-12
    oob = 1;
end
if Pbar < -1e-12 || Pbar > tbl.pbar_grid(end) + 1e-12
    oob = 1;
end
il = round((lam - tbl.lambda_table_min) / tbl.lambda_step) + 1;
il = min(max(il, 1), tbl.n_lambda);
ip = round(Pbar / tbl.pbar_step) + 1;
ip = min(max(ip, 1), tbl.n_pbar);
if SOC <= 0.105
    ir = 2;
elseif SOC >= 0.945
    ir = 3;
else
    ir = 1;
end
end
