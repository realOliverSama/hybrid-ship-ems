% test_safety_interlock_e.m — T-004 链 C：16_Safety_Interlock（PMSSafetyInterlockE）纯 MATLAB 单测
%   方式：把 +model/code_safety_interlock_e() 返回的 block 源码字符串写入 tempdir
%   （文件名 = 块函数名 PMSSafetyInterlockE.m），addpath(tempdir) 后以 0.01 s 步进循环
%   直接调用（每步一次 = 1 tick；无 Simulink/Stateflow）；每用例前 clear 复位 persistent。
%   范围：只测 Safety 函数本体；Safety→Commitment 的 UnitDelay 一拍拍序（override(t) ⇒
%   t+0.01 s 恰好一拍到达）是集成层断言，移交 T-007（tasks.md T-004 Notes）。
%   运行（Model E 工作区根）：
%   matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_safety_interlock_e.m')"
%   依据：.specs/phase4-b-pms-safety/design.md §5.6（评审修订版，D-B14/D-B15）+ §2 裁决
%   + D-B7/D-B11；requirements.md FR-444~447；参数事实源 config/ModelE_Physics_Parameters.m
%   + config/ModelE_Control_Parameters.m（T-001，本测试 run 读取，不改任何数值）。

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(E, 'config', 'ModelE_Control_Parameters.m'));

td = tempdir;
write_text(fullfile(td, 'PMSSafetyInterlockE.m'), model.code_safety_interlock_e());
addpath(td);
rehash;

% 参数快照（全部经入参传入块函数；数值零改动）
PAR.S2 = PMS_S2_hold_ticks;        % 200 tick（2 s，D-B14）
PAR.S4 = PMS_S4_hold_ticks;        % 200 tick（2 s）
PAR.SF = PMS_start_fail_ticks;     % 4000 tick（40 s，D-B7）
PAR.MG = PMS_firm_margin_kW;       % 50 kW
PAR.RATED = DG_P_rated_kW;         % 500 kW
PAR.PMIN = DG_P_min_kW;            % 125 kW
PAR.ETA = ASSUMED_eta_rect;        % 0.97

%% S1a-1: DG1 非预期丢失 3→4（cmd_onoff=1、无本方 force_stop）→ fault_latch=1 + S1 响应
clear PMSSafetyInterlockE
in = def_in(PAR);
[fs, fsr, fst, fstr, fl, ok] = call_e(in);          % tick1：DG1 ON 稳态（静默基线）
assert(isequal(fl, [0, 0]) && isequal(fs, [0, 0]) && isequal(fst, [0, 0]) && ok == 1, ...
    'S1a-1: tick1 基线应静默');
in.state1 = 4; in.P_DG1 = 0;                        % tick2：DG1 非预期进入 COOLDOWN
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df, gap] = call_e(in);
assert(isequal(fl, [1, 0]), 'S1a-1: 非预期丢失应锁存 fault_latch(1)=1');
assert(fstr == 3 && isequal(fst, [1, 0]), 'S1a-1: 故障机应被防御性 FORCE_STOP（reason=3）');
assert(fsr == 3 && isequal(fs, [0, 1]), 'S1a-1: S1 响应应 force_start 离线健康 DG2（reason=3）');
assert(gap == 300, 'S1a-1: S2_supply_gap 应=300（仅记录未触发）');
fprintf('S1a-1 PASS: 3→4 非预期丢失锁存 + 防御 FORCE_STOP + force_start DG2（reason=3）\n');

%% S1a-2: 先令 force_stop 一拍（S3 触发）→ 次拍 3→4 不锁存（prev_force_stop=1 口径）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state2 = 3; in.cmd2 = 1; in.P_DG2 = 50; in.P_DG1 = 50; in.P_load = 100; in.ndisp = 2;
in.P_prop = 0; in.P_hotel = 0;      % S3：2×125−100=150 > chg=50 且双健康 ON（无 hold 计数，即时触发）
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms] = call_e(in);   % tick1：S3 FORCE_STOP DG2
assert(isequal(fst, [0, 1]) && fstr == 6 && isequal(fl, [0, 0]) && ms == 0, ...
    'S1a-2: tick1 应 S3 force_stop DG2（reason=6）且无锁存');
in.state2 = 4; in.P_DG2 = 0;                        % tick2：DG2 执行停机进入 COOLDOWN（cmd 仍=1）
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms] = call_e(in);
assert(isequal(fl, [0, 0]), 'S1a-2: prev_force_stop=1 时 3→4 不得锁存');
assert(isequal(fst, [0, 0]) && fstr == 0, 'S1a-2: 次拍 S3 因单健康机转为 flag-only');
assert(ms == 1, 'S1a-2: 单健康机 unresolved 应登记');
fprintf('S1a-2 PASS: 先令 force_stop 一拍 → 3→4 不锁存（S1a"无 force_stop 在令"口径）\n');

%% S1a-3: cmd_onoff=0 时 3→4（正常停机）→ 不锁存
clear PMSSafetyInterlockE
in = def_in(PAR);
in.cmd1 = 0;
[~, ~, ~, ~, fl] = call_e(in);                      % tick1：DG1 ON、cmd=0
in.state1 = 4; in.P_DG1 = 0;                        % tick2：正常停机进入 COOLDOWN
[fs, fsr, fst, fstr, fl] = call_e(in);
assert(isequal(fl, [0, 0]) && isequal(fst, [0, 0]) && fstr == 0 && ...
    isequal(fs, [0, 0]) && fsr == 0, 'S1a-3: cmd_onoff=0 的 3→4 属正常停机，不得锁存/请求');
fprintf('S1a-3 PASS: cmd_onoff=0 时 3→4 不锁存\n');

%% S1b-1: state=1 连续 4000 tick（=PMS_start_fail_ticks）→ 锁存 + S1 响应
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state1 = 1; in.cmd1 = 1; in.dis = 1e6;           % SOP 富余隔离 S2；D_firm<0 隔离 S4
flv = zeros(1, 4000); fsv = zeros(2, 4000); fstv = zeros(2, 4000); fsrv = zeros(1, 4000);
for k = 1:4000
    [fs, fsr, fst, fstr, fl] = call_e(in);
    flv(k) = fl(1); fsv(:, k) = fs; fstv(:, k) = fst; fsrv(k) = fsr;
end
assert(all(flv(1:3999) == 0), 'S1b-1: 3999 tick 内不得锁存');
assert(flv(4000) == 1, 'S1b-1: 连续 4000 tick STARTING 应锁存');
assert(isequal(fsv(:, 3999), [0; 0]), 'S1b-1: 锁存前不得有 force_start');
assert(isequal(fsv(:, 4000), [0; 1]) && fsrv(4000) == 3, 'S1b-1: 锁存拍应 force_start DG2（reason=3）');
assert(isequal(fstv(:, 4000), [1; 0]) && fstr == 3, 'S1b-1: 锁存拍应防御 FORCE_STOP DG1');
fprintf('S1b-1 PASS: STARTING 连续 4000 tick 锁存（3999 不锁），S1 响应 reason=3\n');

%% S1b-2: state=1 连续 3999 tick → 不锁存
clear PMSSafetyInterlockE
in = def_in(PAR); in.state1 = 1; in.cmd1 = 1; in.dis = 1e6;
for k = 1:3999
    [fs, fsr, fst, fstr, fl] = call_e(in);
end
assert(fl(1) == 0 && isequal(fs, [0, 0]) && isequal(fst, [0, 0]), 'S1b-2: 3999 tick 不得锁存');
fprintf('S1b-2 PASS: 3999 tick 不锁存\n');

%% S1b-3: state=2（SYNCHRONIZING）同验 4000 tick
clear PMSSafetyInterlockE
in = def_in(PAR); in.state1 = 2; in.cmd1 = 1; in.dis = 1e6;
for k = 1:4000
    [fs, fsr, fst, fstr, fl] = call_e(in);
end
assert(fl(1) == 1 && fsr == 3 && isequal(fs, [0, 1]) && isequal(fst, [1, 0]) && fstr == 3, ...
    'S1b-3: SYNC 连续 4000 tick 应锁存并 S1 响应');
fprintf('S1b-3 PASS: SYNC 连续 4000 tick 锁存 + S1 响应 reason=3\n');

%% S1b-4: {1,2} 连续驻留累计计数（2000+2000 → 4000 锁存）
clear PMSSafetyInterlockE
in = def_in(PAR); in.cmd1 = 1; in.dis = 1e6;
flv = zeros(1, 4000);
for k = 1:4000
    if k <= 2000, in.state1 = 1; else, in.state1 = 2; end
    [~, ~, ~, ~, fl] = call_e(in);
    flv(k) = fl(1);
end
assert(all(flv(1:3999) == 0) && flv(4000) == 1, ...
    'S1b-4: STARTING→SYNC 连续驻留应累计计数并在 4000 tick 锁存');
fprintf('S1b-4 PASS: {1,2} 连续驻留累计计数（2000+2000 → 4000 锁存）\n');

%% S1b-5: 离开 {1,2} 计数清零，重新累计 4000 tick 才锁存
clear PMSSafetyInterlockE
in = def_in(PAR); in.cmd1 = 1; in.dis = 1e6;
for k = 1:3000
    in.state1 = 1; [~, ~, ~, ~, fl] = call_e(in);
end
assert(fl(1) == 0, 'S1b-5: 3000 tick 不应锁存');
in.state1 = 3; in.P_DG1 = 300;                      % 一拍 ON（离开 {1,2} → 计数清零）
[~, ~, ~, ~, fl] = call_e(in);
assert(fl(1) == 0, 'S1b-5: ON 拍不锁存');
for k = 1:3999
    in.state1 = 1; in.P_DG1 = 0; [~, ~, ~, ~, fl] = call_e(in);
end
assert(fl(1) == 0, 'S1b-5: 清零后 3999 tick 不应锁存');
[~, ~, ~, ~, fl] = call_e(in);                      % 清零后第 4000 拍
assert(fl(1) == 1, 'S1b-5: 清零后重新累计 4000 tick 应锁存');
fprintf('S1b-5 PASS: 离开 {1,2} 计数清零，重新累计满窗才锁存\n');

%% S2-1: gap>SOP 连续 200 tick → force_start 离线健康 DG2（reason=4）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.P_DG1 = 100; in.P_load = 300;                    % gap=200 > dis=50；DG2 OFF 健康
in.P_prop = 0; in.P_hotel = 0;                      % D_firm=50−550<0 隔离 S4
for k = 1:199
    [fs, fsr] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S2-1: 前 199 tick 不得触发（k=%d）', k);
end
[fs, fsr, ~, ~, ~, ~, ~, ~, ~, ~, gap] = call_e(in); % 第 200 tick
assert(isequal(fs, [0, 1]) && fsr == 4, 'S2-1: 第 200 tick 应 force_start DG2（reason=4）');
assert(gap == 200, 'S2-1: S2_supply_gap 应=200');
fprintf('S2-1 PASS: gap>SOP 连续 200 tick 触发，命中离线健康 DG2（reason=4）\n');

%% S2-2: 连续 199 tick → 不触发
clear PMSSafetyInterlockE
in = def_in(PAR); in.P_DG1 = 100; in.P_load = 300; in.P_prop = 0; in.P_hotel = 0;
for k = 1:199
    [fs, fsr] = call_e(in);
end
assert(isequal(fs, [0, 0]) && fsr == 0, 'S2-2: 199 tick 不得触发');
fprintf('S2-2 PASS: 199 tick 不触发\n');

%% S2-3: 触发后 gap 回落 → 请求清零、计数归零（重新累计 200 tick 才再触发）
clear PMSSafetyInterlockE
in = def_in(PAR); in.P_DG1 = 100; in.P_load = 300; in.P_prop = 0; in.P_hotel = 0;
for k = 1:200
    [fs, fsr] = call_e(in);
end
assert(isequal(fs, [0, 1]) && fsr == 4, 'S2-3: 预置 200 tick 应已触发');
in.P_load = 100;                                    % gap=0 回落
[fs, fsr, ~, ~, ~, ~, ~, ~, ~, ~, gap] = call_e(in);
assert(isequal(fs, [0, 0]) && fsr == 0 && gap == 0, 'S2-3: gap 回落后请求应清零');
in.P_load = 500;                                    % gap=400 重新抬升（计数应已归零）
for k = 1:199
    [fs, fsr] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S2-3: 计数归零后 199 tick 不得触发（k=%d）', k);
end
[fs, fsr] = call_e(in);
assert(isequal(fs, [0, 1]) && fsr == 4, 'S2-3: 重新累计 200 tick 应再触发');
fprintf('S2-3 PASS: gap 回落请求清零且计数归零（重新 200 tick 才再触发）\n');

%% S2-4: 双机均 ON（无目标）→ 请求=0，缺口仍由 S2_supply_gap_kW 记录
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state2 = 3; in.cmd2 = 1; in.P_DG1 = 100; in.P_DG2 = 100; in.P_load = 400;
in.P_prop = 0; in.P_hotel = 0;                      % gap=200>50；D_firm=50−1050<0
for k = 1:250
    [fs, fsr, fst, fstr, fl] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0 && isequal(fst, [0, 0]) && isequal(fl, [0, 0]), ...
        'S2-4: 双机 ON 无目标应 flag-only（k=%d）', k);
end
[~, ~, ~, ~, ~, ~, ~, ~, ~, ~, gap] = call_e(in);
assert(gap == 200, 'S2-4: 缺口仍应由 S2_supply_gap_kW 记录');
fprintf('S2-4 PASS: 双机均 ON（无目标）→ 请求=0，缺口 flag 记录\n');

%% S2-5: fault_latch=1 的离线机不作 force_start 目标
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state2 = 3; in.cmd2 = 1; in.P_DG2 = 0;           % tick1：DG2 ON（出力 0，静默基线）
[~, ~, ~, ~, fl] = call_e(in);
assert(isequal(fl, [0, 0]), 'S2-5: tick1 应静默');
in.state2 = 4;                                      % tick2：DG2 非预期丢失 → 锁存
[fs, fsr, fst, fstr, fl] = call_e(in);
assert(isequal(fl, [0, 1]) && fstr == 3 && isequal(fst, [0, 1]) && isequal(fs, [0, 0]), ...
    'S2-5: DG2 应锁存并被 FORCE_STOP（DG1 ON 非启动目标）');
in.state2 = 0; in.cmd2 = 0;                         % tick3：DG2 离线（锁存保持）
in.P_load = 500;                                    % gap=200>50 → S2 连续判据
for k = 1:200
    [fs, fsr, fst, fstr, fl] = call_e(in);
    assert(isequal(fl, [0, 1]), 'S2-5: 锁存应保持（k=%d）', k);
    assert(isequal(fs, [0, 0]) && fsr == 0, ...
        'S2-5: 故障机不作目标、DG1 ON 亦非目标 → flag-only（k=%d）', k);
    assert(isequal(fst, [0, 1]) && fstr == 3, 'S2-5: 故障机 FORCE_STOP 应保持（k=%d）', k);
end
fprintf('S2-5 PASS: fault_latch=1 的离线机不被 force_start（flag-only）\n');

%% S2-6: 双机离线时目标优先 DG2；同拍 S2、S4 同时触发 reason 取 4
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state1 = 0; in.state2 = 0; in.cmd1 = 0; in.cmd2 = 0; in.P_DG1 = 0; in.P_DG2 = 0;
in.P_prop = 200; in.P_hotel = 60;                   % gap=300>50 且 D_firm=268+50−50>0（同拍判据）
for k = 1:199
    [fs, fsr] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S2-6: 前 199 tick 不得触发（k=%d）', k);
end
[fs, fsr] = call_e(in);                             % 第 200 tick：S2、S4 同时到期
assert(isequal(fs, [0, 1]) && fsr == 4, 'S2-6: 应优先 DG2 且同拍 reason 取 4（S2 优先）');
fprintf('S2-6 PASS: 目标优先 DG2；同拍 S2/S4 触发 reason=4\n');

%% S3-1: N_dispatch=2、双健康 ON、盈余>SOP_chg → force_stop=[0,1]（reason=6，DG1 不动）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state2 = 3; in.cmd2 = 1; in.P_DG1 = 50; in.P_DG2 = 50; in.P_load = 100; in.ndisp = 2;
in.P_prop = 0; in.P_hotel = 0;                      % surplus=250−100=150 > chg=50；n_on=2
for k = 1:3
    [fs, fsr, fst, fstr, fl, ok, fb, fbr, ms] = call_e(in);
    assert(isequal(fst, [0, 1]) && fstr == 6, 'S3-1: 应 FORCE_STOP DG2（reason=6，k=%d）', k);
    assert(fst(1) == 0, 'S3-1: DG1 不得被停（k=%d）', k);
    assert(isequal(fs, [0, 0]) && isequal(fl, [0, 0]) && ms == 0, ...
        'S3-1: 无 start/锁存/unresolved（k=%d）', k);
end
fprintf('S3-1 PASS: 双健康 ON 盈余判据 → force_stop=[0,1] reason=6\n');

%% S3-2: 仅一台健康 ON → min_gen_surplus_unresolved=1 且无 force_stop（禁 0DG）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.P_DG1 = 100; in.P_load = 100; in.ndisp = 2;      % surplus=250−100=150 > chg=50；n_on=1（DG2 OFF）
in.P_prop = 0; in.P_hotel = 0;
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms] = call_e(in);
assert(ms == 1, 'S3-2: 单健康机应登记 min_gen_surplus_unresolved=1');
assert(isequal(fst, [0, 0]) && fstr == 0, 'S3-2: 不得 FORCE_STOP（保持 FR-439 N_DG_on>=1）');
assert(isequal(fl, [0, 0]) && isequal(fs, [0, 0]), 'S3-2: 无锁存/启动请求');
in.P_load = 300;                                    % 盈余条件消失（250−300<chg）
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms] = call_e(in);
assert(ms == 1, 'S3-2: unresolved 登记应锁存保持');
assert(isequal(fst, [0, 0]), 'S3-2: 条件消失后无 FORCE_STOP');
fprintf('S3-2 PASS: 单健康 ON → unresolved=1（锁存）、无 force_stop\n');

%% S4-1: 公式数值校验 (700+100)/0.97≈824.74；N_ON_healthy=1、SOP 高 → D_firm<0 不触发
clear PMSSafetyInterlockE
in = def_in(PAR);
in.P_prop = 700; in.P_hotel = 100; in.dis = 1000;   % firm_avail=500+1000=1500
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df, gap] = call_e(in);
D_exp = (700 + 100) / PAR.ETA + PAR.MG - (PAR.RATED + 1000);
assert(abs(Df - D_exp) < 1e-9, 'S4-1: D_firm 公式不符（%.6f vs %.6f）', Df, D_exp);
assert(abs((Df - PAR.MG + PAR.RATED + 1000) - 800 / PAR.ETA) < 1e-9, ...
    'S4-1: ÷η_rect 负荷要求校验失败（应≈824.74）');
assert(abs(Df - (-625.2577321981134)) < 1e-6, 'S4-1: D_firm 数值校验失败（%.6f）', Df);
for k = 1:250
    [fs, fsr] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S4-1: D_firm<0 不得触发（k=%d）', k);
end
assert(gap == 0, 'S4-1: S2 应静默（gap=0）');
fprintf('S4-1 PASS: D_firm=%.4f（公式+÷η_rect≈824.74 数值校验过），SOP 高不触发\n', Df);

%% S4-2: SOP 收缩使 D_firm>0 连续 200 tick → force_start reason=5；199 不触发
clear PMSSafetyInterlockE
in = def_in(PAR);
in.P_prop = 700; in.P_hotel = 100; in.dis = 100;    % firm_avail=600 → D≈274.74>0
D_exp = (700 + 100) / PAR.ETA + PAR.MG - (PAR.RATED + 100);
for k = 1:199
    [fs, fsr, ~, ~, ~, ~, ~, ~, ~, Df] = call_e(in);
    assert(abs(Df - D_exp) < 1e-9, 'S4-2: D_firm 漂移（k=%d）', k);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S4-2: 前 199 tick 不得触发（k=%d）', k);
end
[fs, fsr] = call_e(in);
assert(isequal(fs, [0, 1]) && fsr == 5, 'S4-2: 第 200 tick 应 force_start DG2（reason=5）');
fprintf('S4-2 PASS: D_firm=%.4f>0 连续 200 tick → reason=5\n', D_exp);

%% S4-3: STARTING 机组不计入 N_ON_healthy（D_firm 用 ON 数=1 口径）且不作目标
clear PMSSafetyInterlockE
in = def_in(PAR);
in.P_prop = 700; in.P_hotel = 100; in.dis = 100; in.state2 = 1; in.cmd2 = 1;
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df] = call_e(in);
assert(abs(Df - ((700 + 100) / PAR.ETA + PAR.MG - (PAR.RATED + 100))) < 1e-9, ...
    'S4-3: STARTING 不计入 N_ON_healthy（D_firm 应与 ON=1 口径一致）');
for k = 1:200
    [fs, fsr] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, ...
        'S4-3: STARTING 非目标、DG1 ON 非目标 → flag-only（k=%d）', k);
end
assert(isequal(fl, [0, 0]), 'S4-3: 无锁存');
fprintf('S4-3 PASS: STARTING 不计入 N_ON_healthy；触发拍 flag-only\n');

%% S4-4: 双机 ON → 无目标 flag-only（D_firm>0 持续 200 tick）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.state2 = 3; in.cmd2 = 1; in.P_DG1 = 300; in.P_DG2 = 300; in.P_load = 600;
in.P_prop = 1300; in.P_hotel = 100; in.dis = 100;   % D=1400/0.97+50−1100≈393>0；gap=0
for k = 1:200
    [fs, fsr, fst, fstr, fl] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0 && isequal(fst, [0, 0]) && isequal(fl, [0, 0]), ...
        'S4-4: 双机 ON 应 flag-only（k=%d）', k);
end
fprintf('S4-4 PASS: 双机 ON → flag-only\n');

%% S5-A: ems_health_raw=0、nominal_is_ems=1 → fallback_active=1、reason=1
clear PMSSafetyInterlockE
in = def_in(PAR); in.health = 0;
[fs, fsr, fst, fstr, fl, ok, fb, fbr] = call_e(in);
assert(ok == 0 && fb == 1 && fbr == 1, 'S5-A: 通道失效应 fallback（reason=1）');
fprintf('S5-A PASS: raw=0 + nominal_is_ems=1 → fallback_active=1 reason=1\n');

%% S5-B: nominal_is_ems=0 → 健康恶化为非事件（不置位不报警）
clear PMSSafetyInterlockE
in = def_in(PAR); in.health = 0; in.nom = 0;
[fs, fsr, fst, fstr, fl, ok, fb, fbr] = call_e(in);
assert(ok == 0 && fb == 0 && fbr == 0, 'S5-B: BASELINE_POLICY 模式下不得置 fallback');
fprintf('S5-B PASS: nominal_is_ems=0 → 不置位不报警\n');

%% S5-C: P_gen_ref=NaN（通道健康）→ fallback reason=2
clear PMSSafetyInterlockE
in = def_in(PAR); in.Pref = NaN;
[fs, fsr, fst, fstr, fl, ok, fb, fbr] = call_e(in);
assert(ok == 0 && fb == 1 && fbr == 2, 'S5-C: ref 非有限应 fallback（reason=2）');
fprintf('S5-C PASS: P_gen_ref=NaN → fallback reason=2\n');

%% S5-D: ems_health_raw=NaN → isfinite 防御，按通道失效 reason=1
clear PMSSafetyInterlockE
in = def_in(PAR); in.health = NaN;
[fs, fsr, fst, fstr, fl, ok, fb, fbr] = call_e(in);
assert(ok == 0 && fb == 1 && fbr == 1, 'S5-D: raw 非有限应按通道失效（reason=1）');
fprintf('S5-D PASS: raw=NaN → isfinite 防御 reason=1\n');

%% S5-E: 恢复健康后 fallback_active 保持 1（锁存不自动恢复，D-B11）+ 首因锁定
clear PMSSafetyInterlockE
in = def_in(PAR); in.health = 0;
[~, ~, ~, ~, ~, ok, fb, fbr] = call_e(in);
assert(fb == 1 && fbr == 1, 'S5-E: 首因应=1');
in.health = 1; in.Pref = 300;                       % 恢复健康
[~, ~, ~, ~, ~, ok, fb, fbr] = call_e(in);
assert(ok == 1, 'S5-E: ems_health_ok 应即时恢复');
assert(fb == 1 && fbr == 1, 'S5-E: fallback 锁存不自动恢复，reason 首因锁定');
in.Pref = NaN;                                      % 后续 ref 恶化不得改写首因
[~, ~, ~, ~, ~, ok, fb, fbr] = call_e(in);
assert(ok == 0 && fb == 1 && fbr == 1, 'S5-E: 首因不得被改写');
fprintf('S5-E PASS: 锁存不自动恢复；fallback_reason 首因锁定\n');

%% S6: 监视 flag（FR-446，纯监视无动作；SOC<=0.20 含等号 / Vdc<600 / 电池三 flag 透传）
clear PMSSafetyInterlockE
in = def_in(PAR);
in.SOC = 0.20;  [~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fsoc] = call_e(in);
assert(fsoc == 1, 'S6: SOC=0.20 应置 flag（<= 含等号）');
in.SOC = 0.21;  [~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fsoc] = call_e(in);
assert(fsoc == 0, 'S6: SOC=0.21 不应置 flag');
in.Vdc = 599.99; [~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fvdc] = call_e(in);
assert(fvdc == 1, 'S6: Vdc=599.99 应置 flag');
in.Vdc = 600.01; [~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fvdc] = call_e(in);
assert(fvdc == 0, 'S6: Vdc=600.01 不应置 flag');
in.b1 = 1; in.b2 = 0; in.b3 = 0.7;
[~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fbat] = call_e(in);
assert(isequal(fbat, [1, 0, 1]), 'S6: 电池 flag 应透传（>0.5 口径）');
in.b1 = 0; in.b2 = 1; in.b3 = 0.3;
[~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, ~, fbat] = call_e(in);
assert(isequal(fbat, [0, 1, 0]), 'S6: 电池 flag 应透传');
fprintf('S6 PASS: SOC<=0.20 / Vdc<600 / 电池三 flag 监视正确\n');

%% S7: fault_latch 锁存持久性（跨 100 tick 保持 1）
clear PMSSafetyInterlockE
in = def_in(PAR);
[~, ~, ~, ~, fl] = call_e(in);                      % tick1：DG1 ON
in.state1 = 4; in.P_DG1 = 0;                        % tick2：非预期丢失 → 锁存
[~, ~, ~, ~, fl] = call_e(in);
assert(isequal(fl, [1, 0]), 'S7: 应锁存');
for k = 1:100
    [~, ~, fst, fstr, fl, ~, ~, ~, ~, ~, ~, ~, ~, ~] = call_e(in);
    assert(fl(1) == 1, 'S7: 锁存跨 tick 保持失败（k=%d）', k);
    assert(isequal(fst, [1, 0]) && fstr == 3, 'S7: 防御 FORCE_STOP 应保持（k=%d）', k);
end
fprintf('S7 PASS: fault_latch 跨 100 tick 保持 1\n');

%% S8: 健康稳态不误触发（500 tick > 2 s 判据窗，全 0 请求、全 0 锁存）
clear PMSSafetyInterlockE
in = def_in(PAR);
for k = 1:500
    [fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df, gap, fsoc, fvdc, fbat] = call_e(in);
    assert(isequal(fs, [0, 0]) && fsr == 0, 'S8: 无 force_start（k=%d）', k);
    assert(isequal(fst, [0, 0]) && fstr == 0, 'S8: 无 force_stop（k=%d）', k);
    assert(isequal(fl, [0, 0]), 'S8: 无故障锁存（k=%d）', k);
    assert(ok == 1 && fb == 0 && fbr == 0 && ms == 0, 'S8: 健康通道/unresolved 应静默（k=%d）', k);
    assert(fsoc == 0 && fvdc == 0 && isequal(fbat, [0, 0, 0]), 'S8: 监视 flag 应静默（k=%d）', k);
    assert(gap == 0 && Df < 0, 'S8: S2/S4 判据应静默（k=%d）', k);
end
fprintf('S8 PASS: 健康稳态 500 tick 全 0 请求、全 0 锁存\n');

fprintf('T004-CHAIN-C-SAFETY-INTERLOCK: ALL PASS\n');

function in = def_in(par)
%DEF_IN 静稳默认输入（健康稳态：DG1 ON 供 300 kW，全判据不触发；各用例按需覆写）
in = struct( ...
    'state1', 3, 'state2', 0, ...       % DG1=ON, DG2=OFF
    'cmd1', 1, 'cmd2', 0, ...
    'P_load', 300, 'P_prop', 200, 'P_hotel', 60, ...
    'P_DG1', 300, 'P_DG2', 0, ...
    'dis', 50, 'chg', 50, 'ndisp', 1, ...
    'SOC', 0.8, 'Vdc', 650, 'b1', 0, 'b2', 0, 'b3', 0, ...
    'Pref', 300, 'health', 1, 'nom', 1, 'voyage', 1, ...
    'p2', par.S2, 'p4', par.S4, 'psf', par.SF, ...
    'mg', par.MG, 'rated', par.RATED, 'pmin', par.PMIN, 'eta', par.ETA);
end

function [fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df, gap, fsoc, fvdc, fbat] = call_e(in)
%CALL_E 以 28 入参调用 PMSSafetyInterlockE（参数全部经入参传入）
[fs, fsr, fst, fstr, fl, ok, fb, fbr, ms, Df, gap, fsoc, fvdc, fbat] = PMSSafetyInterlockE( ...
    in.state1, in.state2, in.cmd1, in.cmd2, ...
    in.P_load, in.P_prop, in.P_hotel, in.P_DG1, in.P_DG2, ...
    in.dis, in.chg, in.ndisp, in.SOC, in.Vdc, in.b1, in.b2, in.b3, ...
    in.Pref, in.health, in.nom, in.voyage, ...
    in.p2, in.p4, in.psf, in.mg, in.rated, in.pmin, in.eta);
end

function write_text(path, txt)
%WRITE_TEXT 把 block 源码字符串写盘（tempdir 测试实例）
fid = fopen(path, 'w');
assert(fid > 0, '无法写入 %s', path);
fwrite(fid, txt, 'char');
fclose(fid);
end