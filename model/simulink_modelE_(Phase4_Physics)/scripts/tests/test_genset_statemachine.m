% test_genset_statemachine.m — T-006 链 B：双发电机组启停状态机 + 占位组合适配器 纯 MATLAB 单测
%   方式：把 +model/code_genset_unit_e()/code_commitment_adapter_e() 返回的 block 源码
%   字符串写入 tempdir，以 0.01 s 步进循环直接调用（每步一次 = 1 tick；无 Simulink/Stateflow）。
%   DG1/DG2 用同一代码的两个文件实例（GensetUnitE / GensetUnitE_DG2）分离 persistent 状态——
%   对应 Simulink 中同一代码的两个 MATLAB Function 块实例（块级 persistent 天然隔离）。
%   运行（worktree 根）：
%   matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_genset_statemachine.m')"
%   依据：design.md §4.3 + OQ-6（定稿）；D-4：无 min up/down（COOLDOWN 物理驻留表达）；
%   参数事实源：config/ModelE_Physics_Parameters.m §5（本测试 run 读取，不重议数值）。

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));

DT = 0.01;  % [s] 状态机离散步（与 block 源码内硬编码 dt 一致）
td = tempdir;
unit_code = model.code_genset_unit_e();
write_text(fullfile(td, 'GensetUnitE.m'), unit_code);
write_text(fullfile(td, 'GensetUnitE_DG2.m'), strrep(unit_code, 'GensetUnitE', 'GensetUnitE_DG2'));
write_text(fullfile(td, 'LegacyCommitmentAdapter.m'), model.code_commitment_adapter_e());
addpath(td);
rehash;

P0 = Pgen_init;

%% S1: tick 精确转移（OFF→STARTING→SYNC→ON）+ 启动期间功率=0
clear GensetUnitE
N = 3200;
stv = zeros(1, N); Pv = zeros(1, N); bv = zeros(1, N); nv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k), bv(k), nv(k)] = GensetUnitE(1, 0, 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
T2 = find(stv == 2, 1);          % 首个 SYNC tick
T3 = find(stv == 3, 1);          % 首个 ON tick
assert(~isempty(T2) && ~isempty(T3), 'S1: 未观察到 SYNC/ON 转移');
assert(abs((T2 - 1) - N_ticks_starting) <= 1, ...
    'S1: STARTING 驻留 (T2-1)=%d 期望 %d±1 tick', T2 - 1, N_ticks_starting);
assert(abs((T3 - 1) - (N_ticks_starting + N_ticks_sync)) <= 1, ...
    'S1: OFF→ON 总时长 (T3-1)=%d 期望 %d±1 tick', T3 - 1, N_ticks_starting + N_ticks_sync);
assert(all(stv(1:T2 - 1) == 1), 'S1: STARTING 期间状态漂移');
assert(all(stv(T2:T3 - 1) == 2), 'S1: SYNC 驻留 tick 数不符');
assert(all(Pv(1:T3) == 0), 'S1: STARTING/SYNC 全程 P_actual 应=0');
assert(Pv(T3 + 1) > 0, 'S1: 进入 ON 后功率链应开始爬坡');
assert(all(bv == 0), 'S1: ref=500≥P_min，below_min_flag 应恒 0');
assert(nv(T3) == 1, 'S1: 进入 ON 时 start_count 应=1');
fprintf('S1 PASS: SYNC@tick%d（STARTING %d tick）、ON@tick%d（启动总 %d tick）；启动期间 P=0\n', ...
    T2, T2 - 1, T3, T3 - 1);

%% S2: aborted-start（STARTING 段注入 cmd_stop → 不中断；ON 后一拍内转 COOLDOWN）
clear GensetUnitE
N = 6200;
cs  = ones(1, N); cs(3010:end) = 0;      % 启动命令（ON 后撤除，不影响已完成的启动）
cst = zeros(1, N); cst(100:end) = 1;     % STARTING 段（tick 100）注入停机命令
stv = zeros(1, N); Pv = zeros(1, N); nv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k), ~, nv(k)] = GensetUnitE(cs(k), cst(k), 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
kON = N_ticks_starting + N_ticks_sync + 1;   % 首个 ON tick（3001）
assert(all(stv(1:N_ticks_starting) == 1), 'S2: cmd_stop 不得中断 STARTING');
assert(all(stv(N_ticks_starting + 1:kON - 1) == 2), 'S2: SYNC 段不受 cmd_stop 影响');
assert(stv(kON) == 3, 'S2: 应先进入 ON（aborted-start 只置 pending_stop）');
assert(all(stv(kON + 1:kON + N_ticks_cooldown) == 4), ...
    'S2: 进入 ON 后一拍内转 COOLDOWN 并驻留 %d tick', N_ticks_cooldown);
assert(all(stv(kON + N_ticks_cooldown + 1:end) == 0), 'S2: COOLDOWN 期满应回 OFF 且不再启动');
assert(all(Pv(1:kON + 1) == 0), 'S2: 启动全程（含 ON 停机拍）P_actual 应=0');
assert(nv(kON) == 1, 'S2: aborted-start 的 start_count 仍应在 SYNC 完成时=1');
fprintf('S2 PASS: STARTING/SYNC 不可中断；ON@tick%d 一拍后 COOLDOWN@tick%d（驻留 %d tick）→ OFF\n', ...
    kON, kON + 1, N_ticks_cooldown);

%% S3a: ON 态 ref=50 < P_min → 功率目标被抬到 125 且 below_min_flag=1
clear GensetUnitE
N = 9000;
stv = zeros(1, N); Pv = zeros(1, N); bv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k), bv(k), ~] = GensetUnitE(1, 0, 50, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
on = stv == 3;
assert(any(on), 'S3a: 未进入 ON');
assert(all(bv(on) == 1), 'S3a: ON 态 ref<P_min 应 below_min_flag=1');
assert(all(bv(~on) == 0), 'S3a: 非 ON 态 below_min_flag 应=0');
assert(abs(Pv(end) - DG_P_min_kW) < 0.5, ...
    'S3a: 稳态功率应被抬到 P_min=%g（P(end)=%.4f）', DG_P_min_kW, Pv(end));
assert(max(Pv) <= DG_P_min_kW + 1e-6, 'S3a: P 不应超过抬升目标');
fprintf('S3a PASS: ref=50 → below_min_flag=1，P 稳态=%.4f（抬到 P_min=%g）\n', Pv(end), DG_P_min_kW);

%% S3b: ON 态 ref=800 > P_rated → clamp 500，below_min_flag=0
clear GensetUnitE
N = 9000;
stv = zeros(1, N); Pv = zeros(1, N); bv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k), bv(k), ~] = GensetUnitE(1, 0, 800, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
on = stv == 3;
assert(all(bv(on) == 0), 'S3b: ref≥P_min，below_min_flag 应=0');
assert(abs(Pv(end) - DG_P_rated_kW) < 0.5, ...
    'S3b: 稳态功率应 clamp 到 P_rated=%g（P(end)=%.4f）', DG_P_rated_kW, Pv(end));
assert(max(Pv) <= DG_P_rated_kW + 1e-6, 'S3b: P 不得超过额定');
fprintf('S3b PASS: ref=800 → P 稳态=%.4f（clamp 到 P_rated=%g），below_min_flag=0\n', Pv(end), DG_P_rated_kW);

%% S4: 爬坡验证（ref=500：稳态 500；爬坡斜率 ≈20 kW/s，容差 5%）
% 注：级联 限幅爬坡(20 kW/s)→一阶惯性(τ=5 s) 使 P_actual 斜率从 0 渐近 20 kW/s；
% 取爬坡平台后段 300→400 kW 交越点测割线斜率（此处斜率已收敛 ≈20×(1−e^(−t/τ))→20）。
clear GensetUnitE
N = 13000;
stv = zeros(1, N); Pv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k)] = GensetUnitE(1, 0, 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
k300 = find(Pv >= 300, 1);
k400 = find(Pv >= 400, 1);
assert(~isempty(k300) && ~isempty(k400) && k400 > k300 && stv(k300) == 3, ...
    'S4: 爬坡未到达 300→400 kW 区间');
slope = (400 - 300) / ((k400 - k300) * DT);
assert(abs(slope - DG_ramp_kW_per_s) / DG_ramp_kW_per_s <= 0.05, ...
    'S4: 300→400 kW 段斜率 %.3f kW/s 偏离 %.3f±5%%', slope, DG_ramp_kW_per_s);
assert(abs(Pv(end) - DG_P_rated_kW) < 0.5, 'S4: 稳态值应=500（P(end)=%.4f）', Pv(end));
fprintf('S4 PASS: 300→400 kW 段斜率=%.3f kW/s（%.1f%% of 20），稳态=%.4f kW\n', ...
    slope, slope / DG_ramp_kW_per_s * 100, Pv(end));

%% S5: COOLDOWN 期间 cmd_start 忽略；start_count 一轮启停=1、二轮=2
clear GensetUnitE
N = 9800;
cs  = zeros(1, N);
cs(1:3001) = 1;          % 第一轮启动
cs(4000:5000) = 1;       % COOLDOWN 期间再启动命令（应被忽略）
cs(6600:end) = 1;        % COOLDOWN 结束后的第二轮启动
cst = zeros(1, N); cst(3500:3510) = 1;   % ON 态停机命令
stv = zeros(1, N); Pv = zeros(1, N); nv = zeros(1, N);
for k = 1:N
    [Pv(k), stv(k), ~, nv(k)] = GensetUnitE(cs(k), cst(k), 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
assert(all(stv(4000:5000) == 4), 'S5: COOLDOWN 期间 cmd_start 应被忽略');
assert(all(Pv(3500:6599) == 0), 'S5: COOLDOWN/OFF 期间 P_actual 应=0');
assert(nv(4000) == 1, 'S5: 一轮完整启停后 start_count 应=1');
assert(all(stv(6500:6599) == 0), 'S5: COOLDOWN 期满应回 OFF');
assert(nv(N) == 2, 'S5: 第二轮启动完成后 start_count 应=2');
fprintf('S5 PASS: COOLDOWN（%d tick）期间 cmd_start 忽略；start_count 一轮=1、二轮=2\n', ...
    N_ticks_cooldown);

%% S6: DG2 启动缺口（adapter ref=800：DG1 满功率 ON，DG2 启动期 P=0，不虚构功率）
clear GensetUnitE
clear GensetUnitE_DG2
% DG1 预热到 ON 态满功率（adapter: ref=800 → DG1_ref=500）
N1 = 9000;
P1a = zeros(1, N1); st1a = zeros(1, N1);
for k = 1:N1
    [P1a(k), st1a(k), ~, ~] = GensetUnitE(1, 0, 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
assert(st1a(end) == 3 && P1a(end) >= 495, ...
    'S6: DG1 预热应达 ON 满功率（state=%d, P=%.3f）', st1a(end), P1a(end));
% DG2 从 OFF 启动（adapter: DG2_ref=800-500=300）；DG1 保持 ON 满功率
N2 = 3600;
P1b = zeros(1, N2); P2b = zeros(1, N2); st2b = zeros(1, N2);
for k = 1:N2
    [P1b(k), ~, ~, ~] = GensetUnitE(1, 0, 500, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
    [P2b(k), st2b(k), ~, ~] = GensetUnitE_DG2(1, 0, 300, ...
        DG_P_rated_kW, DG_P_min_kW, DG_ramp_kW_per_s, DG_tau_s, ...
        N_ticks_starting, N_ticks_sync, N_ticks_cooldown, P0);
end
T2b = find(st2b == 2, 1);
T3b = find(st2b == 3, 1);
assert(~isempty(T2b) && ~isempty(T3b), 'S6: DG2 未进入 SYNC/ON');
assert(abs((T2b - 1) - N_ticks_starting) <= 1 && ...
    abs((T3b - 1) - (N_ticks_starting + N_ticks_sync)) <= 1, 'S6: DG2 启动时序不符');
assert(all(P2b(1:T3b) == 0), ...
    'S6: DG2 STARTING/SYNC 期间 P_actual 应=0（缺口由电池/母线响应，不虚构功率）');
assert(all(P1b >= 495), 'S6: DG1 应全程保持 ON 满功率');
gap = 800 - (P1b(1:T3b) + P2b(1:T3b));
fprintf('S6 PASS: DG1=ON/%.1f kW；DG2 启动 30 s 内 P=0；母线功率缺口 ≈%.0f kW（电池/母线自然响应）\n', ...
    P1a(end), min(gap));

%% S7: adapter 规则（auto 边界 ≤500 仅 DG1；>500 拆分；force 组合）
[a1, p1, a2, p2] = LegacyCommitmentAdapter(400, 0, 0, 0);
assert(a1 == 1 && p1 == 400 && a2 == 0 && p2 == 0, 'S7: ref=400 应 → (1,400,0,0)');
[a1, p1, a2, p2] = LegacyCommitmentAdapter(800, 0, 0, 0);
assert(a1 == 1 && p1 == 500 && a2 == 1 && p2 == 300, 'S7: ref=800 应 → (1,500,1,300)');
[a1, p1, a2, p2] = LegacyCommitmentAdapter(500, 0, 0, 0);
assert(a1 == 1 && p1 == 500 && a2 == 0 && p2 == 0, 'S7: 边界 ref=500 应仅 DG1');
[a1, p1, a2, p2] = LegacyCommitmentAdapter(1000, 0, 0, 0);
assert(a1 == 1 && p1 == 500 && a2 == 1 && p2 == 500, 'S7: ref=1000 应 → (500,500)');
[a1, p1, a2, p2] = LegacyCommitmentAdapter(999, 1, 0, 250);
assert(a1 == 0 && a2 == 1 && p2 == 250, 'S7: force_mode=1/force_P2=250 应 → DG2=1/P2=250');
[a1, p1, a2, p2] = LegacyCommitmentAdapter(999, 1, 400, 0);
assert(a1 == 1 && p1 == 400 && a2 == 0, 'S7: force_mode=1/force_P1=400 应 → DG1=1/P1=400');
fprintf('S7 PASS: adapter auto/force 规则与 ≤500 边界正确（PLACEHOLDER, FR-421）\n');

fprintf('T006-CHAIN-B-STATEMACHINE: ALL PASS\n');

function write_text(path, txt)
%WRITE_TEXT 把 block 源码字符串写盘（tempdir 测试实例）
fid = fopen(path, 'w');
assert(fid > 0, '无法写入 %s', path);
fwrite(fid, txt, 'char');
fclose(fid);
end