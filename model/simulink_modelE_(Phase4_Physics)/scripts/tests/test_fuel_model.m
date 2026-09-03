% test_fuel_model.m — T-007 链 B：per-genset 燃油模型（FuelModelE）纯 MATLAB 单测
%   方式：把 +model/code_fuel_e() 返回的 block 源码字符串写入 tempdir 直接调用（无 Simulink）。
%   依据：design.md §4.4（FR-420）+ OQ-6 方案 A（启动燃油只算一次、均摊 STARTING+SYNC 30 s）；
%   BSFC 五节点 PCHIP 事实源：config/ModelE_Physics_Parameters.m §5（对照 run 读取，不重议数值）。
%   运行（worktree 根）：
%   matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_fuel_model.m')"

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));

td = tempdir;
write_text(fullfile(td, 'FuelModelE.m'), model.code_fuel_e());
addpath(td);
rehash;

%% F1: BSFC 节点值精确（P=125/250/375/400/500 → 600/547/447/427/453 g/kWh）
for i = 1:numel(DG_BSFC_P_kW)
    [r1, r2, ~] = FuelModelE(DG_BSFC_P_kW(i), 3, 0, 0, ...
        DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
    assert(r2 == 0, 'F1: 备用机组 OFF 应 rate=0');
    expected = DG_BSFC_P_kW(i) * DG_BSFC_gkWh(i) / 3.6e6;
    assert(abs(r1 - expected) < 1e-12, ...
        'F1: P=%g 节点 rate=%.12g 期望 %.12g', DG_BSFC_P_kW(i), r1, expected);
    bsfc_back = r1 * 3.6e6 / DG_BSFC_P_kW(i);
    assert(abs(bsfc_back - DG_BSFC_gkWh(i)) < 1e-9, ...
        'F1: P=%g 反算 BSFC=%.6f 期望 %g g/kWh', DG_BSFC_P_kW(i), bsfc_back, DG_BSFC_gkWh(i));
end
fprintf('F1 PASS: 节点值精确 %s g/kWh @ %s kW\n', mat2str(DG_BSFC_gkWh), mat2str(DG_BSFC_P_kW));

%% F2: PCHIP 形状（P=200/450 与 MATLAB interp1 参考逐位一致）
for Pq = [200 450]
    ref_bsfc = interp1(DG_BSFC_P_kW, DG_BSFC_gkWh, Pq, 'pchip');
    [r1, ~, ~] = FuelModelE(Pq, 3, 0, 0, ...
        DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
    expected = Pq * ref_bsfc / 3.6e6;
    assert(isequal(r1, expected), ...
        'F2: P=%g 与 interp1 pchip 参考非逐位一致（r=%.17g, ref=%.17g）', Pq, r1, expected);
    fprintf('F2: P=%g kW → BSFC=%.6f g/kWh（pchip 内插）\n', Pq, ref_bsfc);
end
fprintf('F2 PASS: PCHIP 内插与参考实现逐位一致\n');

%% F3: 启动燃油一次性（state=1×2500 步 + state=2×500 步，rate=0.02 kg/s → 总耗 0.6±0.01 kg）
rate_start = DG_C_start_kg / (DG_t_starting_s + DG_t_sync_s);
[r1, ~, ~] = FuelModelE(0, 1, 0, 0, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
assert(abs(r1 - rate_start) < 1e-15 && abs(r1 - 0.02) < 1e-12, ...
    'F3: STARTING rate=%.12g 期望 C_start/(t_start+t_sync)=%.12g', r1, rate_start);
total = 0;
for k = 1:(N_ticks_starting + N_ticks_sync)
    if k <= N_ticks_starting
        s1 = 1;   % STARTING 2500 tick
    else
        s1 = 2;   % SYNCHRONIZING 500 tick
    end
    [~, ~, rt] = FuelModelE(0, s1, 0, 0, ...
        DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
    total = total + rt * 0.01;
end
assert(abs(total - DG_C_start_kg) <= 0.01, ...
    'F3: 启动段积分总耗=%.6f kg 期望 %.2f±0.01 kg（只算一次、均摊实现）', total, DG_C_start_kg);
fprintf('F3 PASS: STARTING(%d tick)+SYNC(%d tick) 积分总耗=%.6f kg = C_start（无双算）\n', ...
    N_ticks_starting, N_ticks_sync, total);

%% F4: 域守卫（ON 且 P=100 防御 clamp → BSFC(125)，不外推、无 NaN；OFF/COOLDOWN → 0）
[r1, ~, ~] = FuelModelE(100, 3, 0, 0, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
assert(isfinite(r1), 'F4: ON 且 P=100 出现非有限值');
assert(abs(r1 * 3.6e6 / 100 - DG_BSFC_gkWh(1)) < 1e-9, ...
    'F4: ON 且 P=100 应按 clamp(125) 节点 BSFC=%g 计量，不外推', DG_BSFC_gkWh(1));
[~, ~, rt] = FuelModelE(0, 0, 0, 0, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
assert(rt == 0, 'F4: OFF → rate=0');
[~, ~, rt] = FuelModelE(0, 4, 0, 4, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
assert(rt == 0, 'F4: COOLDOWN → rate=0（怠速未计为 registered 局限）');
% 混合状态求和：DG1 ON@500 kW + DG2 STARTING
[r1, r2, rt] = FuelModelE(500, 3, 0, 1, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
assert(abs(rt - (500 * DG_BSFC_gkWh(end) / 3.6e6 + rate_start)) < 1e-12, ...
    'F4: 混合状态燃油率求和不符');
fprintf('F4 PASS: P=100 → BSFC(125)=%g 不外推；OFF/COOLDOWN rate=0；混合状态求和正确\n', ...
    DG_BSFC_gkWh(1));

%% F5: 经济性可区分（1×500 kW vs 2×250 kW，相对差 >1%，实际 ≈20.8%）
[~, ~, rt_1x500] = FuelModelE(500, 3, 0, 0, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
[~, ~, rt_2x250] = FuelModelE(250, 3, 250, 3, ...
    DG_BSFC_P_kW, DG_BSFC_gkWh, DG_C_start_kg, DG_t_starting_s, DG_t_sync_s);
e1 = 500 * DG_BSFC_gkWh(end) / 3.6e6;       % 500×453/3.6e6
e2 = 2 * 250 * DG_BSFC_gkWh(2) / 3.6e6;     % 2×250×547/3.6e6
assert(abs(rt_1x500 - e1) < 1e-15, 'F5: 1×500 rate 与解析值不符');
assert(abs(rt_2x250 - e2) < 1e-15, 'F5: 2×250 rate 与解析值不符');
rel = (rt_2x250 - rt_1x500) / rt_1x500;
assert(rel > 0.01, 'F5: 经济性应可区分（rel=%.4f ≤ 1%%）', rel);
fprintf('F5 PASS: 1×500=%.6f kg/s vs 2×250=%.6f kg/s，相对差=%.2f%%（>1%%）\n', ...
    rt_1x500, rt_2x250, rel * 100);

fprintf('T007-CHAIN-B-FUEL: ALL PASS\n');

function write_text(path, txt)
%WRITE_TEXT 把 block 源码字符串写盘（tempdir 测试实例）
fid = fopen(path, 'w');
assert(fid > 0, '无法写入 %s', path);
fwrite(fid, txt, 'char');
fclose(fid);
end