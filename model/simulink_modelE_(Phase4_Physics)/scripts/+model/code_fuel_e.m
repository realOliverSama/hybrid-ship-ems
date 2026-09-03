function c = code_fuel_e()
%CODE_FUEL_E 返回 FuelModelE MATLAB Function block 源码字符串（Model E，T-007）
%   c = model.code_fuel_e()
%   用途：builder 经 setMFCode 写入 10_Fuel_Consumption 的 per-genset 燃油块（FR-420）；
%   纯 MATLAB 单测（scripts/tests/test_fuel_model.m）以此字符串写盘直接调用。
%   依据：design.md §4.4 + OQ-6 方案 A（启动燃油只算一次、均摊 STARTING+SYNC 30 s）；
%   BSFC 五节点 PCHIP 事实源：config/ModelE_Physics_Parameters.m §5（数值冻结，禁止重议）；
%   禁止 P<P_min 外推；COOLDOWN 怠速油耗未计为 registered 局限。

lines = { ...
'function [fuel_rate_1_kg_s, fuel_rate_2_kg_s, fuel_rate_total_kg_s] = FuelModelE(...'
'    P1_kW, state1, P2_kW, state2, ...'
'    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s)'
'%#codegen'
'% FuelModelE：per-genset 燃油模型（Model E, FR-420, design 4.4, OQ-6 方案 A）'
'%   每机组独立计量，total = rate_1 + rate_2 [kg/s]：'
'%   ON(state=3)：rate = P * BSFC_pchip(clamp(P,[125,500])) / 3.6e6'
'%     BSFC LUT 只在 ON 态使用；P<P_min 的爬坡过渡段 clamp 到下节点(125)值查表——'
'%     不外推、非零；registered surrogate：过渡段怠速油耗未计（与 COOLDOWN 同类局限）；'
'%     正常下界不可达（由 Plant clamp 保证），此处为防御性 clamp。'
'%   STARTING/SYNC(state=1|2)：rate = C_start/(t_starting+t_sync)（0.6/30=0.02 kg/s，'
'%     30 s 积分恰为 C_start——启动燃油只算一次、均摊实现，禁止与 ON 态双算）。'
'%   OFF/COOLDOWN(state=0|4)：rate = 0（COOLDOWN 怠速油耗未计为 registered 局限）。'
'fuel_rate_1_kg_s = unit_fuel_rate(P1_kW, state1, DG_BSFC_P_kW, DG_BSFC_gkWh, ...'
'    DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);'
'fuel_rate_2_kg_s = unit_fuel_rate(P2_kW, state2, DG_BSFC_P_kW, DG_BSFC_gkWh, ...'
'    DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);'
'fuel_rate_total_kg_s = fuel_rate_1_kg_s + fuel_rate_2_kg_s;'
'end'
''
'function r = unit_fuel_rate(P_kW, st, P_bp, g_kWh, C_start_kg, t_starting_s, t_sync_s)'
'% 单机组瞬时燃油率 [kg/s]（state: 0=OFF 1=STARTING 2=SYNC 3=ON 4=COOLDOWN）'
'if st == 3'
'    % ON：防御性 clamp 到 BSFC 定义域节点 [125,500]（禁止 P<P_min 外推）'
'    P_c = min(max(P_kW, P_bp(1)), P_bp(end));'
'    bsfc_g_kWh = interp1(P_bp, g_kWh, P_c, ''pchip'');'
'    r = P_kW * bsfc_g_kWh / 3.6e6;   % kW × g/kWh = g/h → /3.6e6 → kg/s'
'elseif st == 1 || st == 2'
'    % STARTING/SYNC：启动燃油均摊（只算一次，方案 A event-level surrogate）'
'    r = C_start_kg / (t_starting_s + t_sync_s);'
'else'
'    % OFF / COOLDOWN：怠速油耗未计（registered 局限）'
'    r = 0;'
'end'
'end'};
c = strjoin(lines, newline);
end