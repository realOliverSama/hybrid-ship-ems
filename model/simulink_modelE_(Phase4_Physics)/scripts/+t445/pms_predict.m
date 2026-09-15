function s = pms_predict(Pg_star_kW, varargin)
% t445.pms_predict — 由聚合 P_g* 预测冻结 PMS 均分（不选动作，只估成本）
% DG1 恒在线；N=2 当 P_g* > 单机额定或超过启动门槛。
p = inputParser;
p.addParameter('P_min', 125, @isnumeric);
p.addParameter('P_rated', 500, @isnumeric);
p.addParameter('P_start_th', 425, @isnumeric);
p.addParameter('N_prev', 1, @isnumeric);
p.parse(varargin{:});
o = p.Results;
Pg = max(0, Pg_star_kW);
% 窗长 300 s ≤ min_up：窗内一旦两机则保持两机（不停机）
if o.N_prev >= 2
    N = 2;
elseif Pg > o.P_rated + 1e-9 || Pg > o.P_start_th + 1e-9
    N = 2;
else
    N = 1;
end
P = zeros(1, 2);
if N < 1
    s = struct('N', 0, 'P_DG', P, 'start_event', 0);
    return;
end
Praw = Pg / N;
Pi = min(max(Praw, o.P_min), o.P_rated);
P(1:N) = Pi;
s = struct();
s.N = N;
s.P_DG = P;
s.start_event = double(N > o.N_prev);
end
