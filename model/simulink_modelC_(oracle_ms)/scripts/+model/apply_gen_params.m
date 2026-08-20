function eff = apply_gen_params(varargin)
% model.apply_gen_params — 发电机三参数扫描覆写机制（FR-002/FR-011）
% 覆写 base workspace 中发电机参数（不触发模型结构重建）：
%   Gen_P_max / Gen_Ramp_Up / Gen_Ramp_Down / ASSUMED_Gen_TimeConstant
% 调用形式（name-value 或 struct）：
%   eff = model.apply_gen_params('GP_kW', 640, 'R_gen', 40, 'tau_gen', 5);
%   eff = model.apply_gen_params(struct('GP_kW',640,'R_gen',40,'tau_gen',5));
% 可指定子集（未指定的沿用当前 base 值）；'GP_kW' 与 'lambda_P' 二选一。
% 输出 eff：生效值记录（供 runs.csv）：
%   eff.Gen_P_max / eff.Gen_Ramp_Up / eff.Gen_Ramp_Down / eff.tau_gen
%   eff.lambda_P = Gen_P_max / P_load_design_max
%   eff.MSFF_Horizon_s 等其余 MSFF 参数不做覆写（实验脚本直接 assignin）。
% 约定：P_load_design_max 为统一分母（FR-011），不随场景变化。
if nargin == 1 && isstruct(varargin{1})
    opts = varargin{1};
else
    opts = struct();
    for k = 1:2:numel(varargin)
        opts.(varargin{k}) = varargin{k+1};
    end
end

% 读取当前 base 值作为默认
getv = @(nm) evalin('base', nm);
P_load_design_max = getv('P_load_design_max');
GP  = getv('Gen_P_max');
Rup = getv('Gen_Ramp_Up');
Rdn = getv('Gen_Ramp_Down');
tau = getv('ASSUMED_Gen_TimeConstant');

if isfield(opts, 'GP_kW')
    GP = opts.GP_kW;
elseif isfield(opts, 'lambda_P')
    GP = opts.lambda_P * P_load_design_max;
end
if isfield(opts, 'R_gen')
    Rup = opts.R_gen;
    Rdn = opts.R_gen;
end
if isfield(opts, 'R_up'),  Rup = opts.R_up;  end
if isfield(opts, 'R_down'), Rdn = opts.R_down; end
if isfield(opts, 'tau_gen')
    tau = opts.tau_gen;
end

assignin('base', 'Gen_P_max', GP);
assignin('base', 'Gen_Ramp_Up', Rup);
assignin('base', 'Gen_Ramp_Down', Rdn);
assignin('base', 'ASSUMED_Gen_TimeConstant', tau);

eff = struct( ...
    'Gen_P_max', GP, 'Gen_Ramp_Up', Rup, 'Gen_Ramp_Down', Rdn, 'tau_gen', tau, ...
    'lambda_P', GP / P_load_design_max, ...
    'P_load_design_max', P_load_design_max);
fprintf('[apply_gen_params] Gen_P_max=%.0f kW (λ_P=%.2f), R=%.0f/%.0f kW/s, τ=%.1f s\n', ...
    GP, eff.lambda_P, Rup, Rdn, tau);
end
