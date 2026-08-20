function H = minff_unit()
% model.minff_unit — EMS_MinFF 单元验证辅助函数集（T-005 U1~U6）
% 返回 handle 结构体，供 20c_unittest_minff.m（纯脚本，eval(fileread) 运行）调用。
% 所有函数与 build_model_cms.m 中 CODE_MIN_FF 内联逻辑逐行一致（注释标注出处）。
    H = struct();
    H.fSOC          = @fSOC;
    H.fVdc          = @fVdc;
    H.replica_minff = @replica_minff;
    H.reach_node    = @reach_node;
    H.reach_at      = @reach_at;
    H.analytic_reach = @analytic_reach;
end

function f = fSOC(SOC, SOCK)
% 与 CODE_MIN_FF 内联 f_SOC 完全一致（build_model_cms.m CODE_MIN_FF）
if SOC <= SOCK(1)
    f = 0;
elseif SOC >= SOCK(2)
    f = 1;
else
    f = (SOC - SOCK(1)) / (SOCK(2) - SOCK(1));
end
end

function f = fVdc(Vdc, VDCK)
% 与 CODE_MIN_FF 内联 f_V 完全一致
if Vdc <= VDCK(1)
    f = 0;
elseif Vdc >= VDCK(2)
    f = 1;
else
    f = (Vdc - VDCK(1)) / (VDCK(2) - VDCK(1));
end
end

function [ff, R0] = replica_minff(P_hat, n, P_base, P_gen_now, P_gen_ramp, SOC, Vdc, ...
    SOCK, VDCK, BatLim, Pmax, Msp, Rup, Rdn, tau, dt, fold)
% 与 CODE_MIN_FF 决策块逐行一致（T-004 实现，含折叠/折点/二分/防抖前原始值）
if fold
    pk = max(P_hat(1:n));
    P_hat = repmat(pk, 1, 12);
end
kn = SOCK;
if SOC <= kn(1), fS = 0; elseif SOC >= kn(2), fS = 1; else, fS = (SOC-kn(1))/(kn(2)-kn(1)); end
kn = VDCK;
if Vdc <= kn(1), fV = 0; elseif Vdc >= kn(2), fV = 1; else, fV = (Vdc-kn(1))/(kn(2)-kn(1)); end
Pb = BatLim * fS * fV;
R0 = model.minff_reach_standalone(0, P_hat, n, P_base, P_gen_now, P_gen_ramp, Pb, ...
    Pmax, Msp, Rup, Rdn, tau, dt);
if R0 <= 0
    ff = 0;
else
    lo = 0; hi = Pmax;
    for it = 1:20
        mid = 0.5*(lo+hi);
        Rm = model.minff_reach_standalone(mid, P_hat, n, P_base, P_gen_now, P_gen_ramp, Pb, ...
            Pmax, Msp, Rup, Rdn, tau, dt);
        if Rm > 0, lo = mid; else, hi = mid; end
    end
    ff = 0.5*(lo+hi);
end
end

function P = reach_node(P_ff, P_hat, n, P_base, P_gen_now, P_gen_ramp, Pb, ...
    Pmax, Msp, Rup, Rdn, tau, dt, nodeIdx)
% 复算 min_ff 内 minff_reach 到指定节点的可达功率（与模型逐行一致）
P_tgt = min(P_base + P_ff, Pmax);
P_cmd = min(max(P_tgt, Msp), Pmax);
rr = P_gen_ramp; yy = P_gen_now;
Tn = 5 * nodeIdx;
yi = 0;
while yi < Tn
    dtk = min(dt, Tn - yi);
    drl = P_cmd - rr; drl = min(max(drl, -Rdn*dtk), Rup*dtk);
    rn = rr + drl;
    e = exp(-dtk/tau); s = (rn-rr)/dtk;
    yn = yy*e + rr*(1-e) + s*(dtk - tau*(1-e));
    rr = rn; yy = yn; yi = yi + dtk;
end
P = min(max(yy, Msp), Pmax);
end

function P = reach_at(t, P_ff, P_base, P_gen_now, P_gen_ramp, Pb, ...
    Pmax, Msp, Rup, Rdn, tau, dt)
% 递推到任意时刻 t 的可达功率（细网格，供 U1 轨迹对照）
P_tgt = min(P_base + P_ff, Pmax);
P_cmd = min(max(P_tgt, Msp), Pmax);
rr = P_gen_ramp; yy = P_gen_now; yi = 0;
while yi < t
    dtk = min(dt, t - yi);
    drl = P_cmd - rr; drl = min(max(drl, -Rdn*dtk), Rup*dtk);
    rn = rr + drl;
    e = exp(-dtk/tau); s = (rn-rr)/dtk;
    yn = yy*e + rr*(1-e) + s*(dtk - tau*(1-e));
    rr = rn; yy = yn; yi = yi + dtk;
end
P = min(max(yy, Msp), Pmax);
end

function P = analytic_reach(t, P_cmd, rr0, yy0, Rup, Rdn, tau, Msp, Pmax)
% 解析解：限幅(已施加) -> 爬坡(斜率 R) -> 一阶惯性(τ) -> 限幅
%   u(t)=rr0+sgn*R*t  (t<=t_cross)，u(t)=P_cmd (t>t_cross)
%   y(t)=yy0*e^{-t/τ}+u0*(1-e^{-t/τ})+s*(t-τ(1-e^{-t/τ}))（线性输入精确解）
d = P_cmd - rr0;
if abs(d) < 1e-12
    t_cross = 0; sgn = 0;
else
    sgn = sign(d);
    R = (d > 0) * Rup + (d < 0) * Rdn;
    t_cross = abs(d) / R;
end
P = zeros(size(t));
for k = 1:numel(t)
    tk = t(k);
    if tk <= t_cross
        if sgn == 0
            yk = yy0*exp(-tk/tau) + P_cmd*(1-exp(-tk/tau));
        else
            R = (d > 0)*Rup + (d < 0)*Rdn;
            yk = yy0*exp(-tk/tau) + rr0*(1-exp(-tk/tau)) + sgn*R*(tk - tau*(1-exp(-tk/tau)));
        end
    else
        R = (d > 0)*Rup + (d < 0)*Rdn;
        yc = yy0*exp(-t_cross/tau) + rr0*(1-exp(-t_cross/tau)) + sgn*R*(t_cross - tau*(1-exp(-t_cross/tau)));
        yk = yc*exp(-(tk-t_cross)/tau) + P_cmd*(1-exp(-(tk-t_cross)/tau));
    end
    P(k) = min(max(yk, Msp), Pmax);
end
end
