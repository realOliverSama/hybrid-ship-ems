function in = make_simin_zoh(in, blockPath)
% t445.make_simin_zoh — run-local Interpolate=off（真 ZOH），不落盘 .slx（DGN-F-02 §2.3）
%
% in: Simulink.SimulationInput 或模型名 char。B1/B2 必须走同一函数。
if nargin < 2 || isempty(blockPath)
    if isa(in, 'Simulink.SimulationInput')
        mdl = in.ModelName;
    else
        mdl = char(in);
    end
    blockPath = t445.oracle_src_block(mdl);
end
if ~isa(in, 'Simulink.SimulationInput')
    in = Simulink.SimulationInput(char(in));
end
in = in.setBlockParameter(blockPath, 'Interpolate', 'off');
end
