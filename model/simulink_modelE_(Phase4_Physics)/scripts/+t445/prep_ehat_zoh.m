function data = prep_ehat_zoh(t0, E300_kWh, ehat)
% t445.prep_ehat_zoh — From Workspace 真 ZOH 节点表（仅 t0 网格，HoldingFinalValue 收尾）
%
% 输出 N×3：[t0, E300_true, e_hat]，供 ECMS_EHat_Data。
% 块 SampleTime 0.01 + Interpolate=off => 任意查询时刻返回最近左侧样本。
t0 = t0(:);
E300_kWh = E300_kWh(:);
ehat = ehat(:);
assert(numel(t0) == numel(E300_kWh) && numel(t0) == numel(ehat), ...
    'T445:ZOHDATA:LEN', 't0/E300/ehat length mismatch');
assert(issorted(t0), 'T445:ZOHDATA:T0', 't0 must be nondecreasing');
data = [t0, E300_kWh, ehat];
end
