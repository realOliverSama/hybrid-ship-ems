function r = fuel_model_e_unit(P_kW, st, varargin)
% t445.fuel_model_e_unit — FuelModelE 单机瞬时油耗 [kg/s]（与 code_fuel_e 同语义）
% st: 0=OFF 1=STARTING 2=SYNC 3=ON 4=COOLDOWN
p = inputParser;
p.addParameter('P_bp', [125 250 375 400 500], @isnumeric);
p.addParameter('g_kWh', [600 547 447 427 453], @isnumeric);
p.addParameter('C_start_kg', 0.6, @isnumeric);
p.addParameter('t_start_s', 25, @isnumeric);
p.addParameter('t_sync_s', 5, @isnumeric);
p.parse(varargin{:});
o = p.Results;
if st == 3
    P_c = min(max(P_kW, o.P_bp(1)), o.P_bp(end));
    bsfc = interp1(o.P_bp, o.g_kWh, P_c, 'pchip');
    r = P_kW * bsfc / 3.6e6;
elseif st == 1 || st == 2
    r = o.C_start_kg / (o.t_start_s + o.t_sync_s);
else
    r = 0;
end
end
