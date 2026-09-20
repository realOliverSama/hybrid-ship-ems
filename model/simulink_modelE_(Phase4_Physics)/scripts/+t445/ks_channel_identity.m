function r = ks_channel_identity()
% t445.ks_channel_identity — AC-618: |K_soc·K_S·ê − Δλ| < 1e-12 for B2 K_S=1/30
cfg = phase4f_t445_config();
Ksoc = cfg.lambda_law.K_soc;
KS = cfg.lambda_law.K_S_B2_num / cfg.lambda_law.K_S_B2_den;
ehat = [-0.10; -0.05; 0; 0.05; 0.10];
dlam = Ksoc * KS * ehat;
resid = max(abs(dlam - ehat));
assert(resid < 1e-12, 'T445:KS:ID', 'K_S channel residual %g', resid);
r = struct('pass', true, 'max_residual', resid, 'K_S', KS);
end
