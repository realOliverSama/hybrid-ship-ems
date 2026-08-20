function Rm = minff_reach_standalone(P_ff, P_hat, n, P_base, P_gen_now, P_gen_ramp, Pb, Pmax, Msp, Rup, Rdn, tau, dt)
% model.minff_reach_standalone - min_ff 内可达性递推的独立复算（T-004 AC3/AC4 对照）
    P_tgt = min(P_base + P_ff, Pmax);
    P_cmd = min(max(P_tgt, Msp), Pmax);
    rr = P_gen_ramp; yy = P_gen_now;
    Rm = -inf;
    for i = 1:n
        Tn = 5 * i; yi = 0;
        while yi < Tn
            dtk = min(dt, Tn - yi);
            drl = P_cmd - rr; drl = min(max(drl, -Rdn*dtk), Rup*dtk);
            rn = rr + drl;
            e = exp(-dtk/tau); s = (rn-rr)/dtk;
            yn = yy*e + rr*(1-e) + s*(dtk - tau*(1-e));
            rr = rn; yy = yn; yi = yi + dtk;
        end
        P_reach = min(max(yy, Msp), Pmax);
        Rv = P_hat(i) - P_reach - Pb;
        if Rv > Rm, Rm = Rv; end
    end
end
