% test_commitment_chart_e.m — T-005：DG_Commitment_Supervisor Stateflow chart harness 单测
%   方式：tests/build_commitment_harness() 在 tempdir 建最小模型（与 builder 共用
%   model.build_commitment_chart_e()），每用例写 8 张注入表 → sim → logsout 断言。
%   运行（Model E 工作区根）：
%   /Applications/MATLAB_R2026a.app/bin/matlab -batch "run('scripts/tests/test_commitment_chart_e.m')"
%   依据：design.md §5.4（状态图/迁移优先级/timer 语义）+ FR-436~440 + D-B4/D-B5/D-B7；
%   参数事实源：config/ModelE_Control_Parameters.m（阈值 425/350 kW、tick 3000/12000/30000）。
%   注：FORCED(7) 为 DD-8 bench force 通道专用枚举值——正常 chart 路径永不产生，
%       本测试断言其零出现（44 号负向验证向量的正向侧证据）。

TESTS_ROOT = fileparts(mfilename('fullpath'));
if isempty(TESTS_ROOT), TESTS_ROOT = pwd; end
E = fileparts(fileparts(TESTS_ROOT));
addpath(TESTS_ROOT, fullfile(E, 'config'), fullfile(E, 'scripts'));

h = build_commitment_harness();
MDL = h.mdl;
TOL = 0.02;   % ±2 tick（迁移在条件满足后的下一拍判出，语义见 chart 注释）
IN = h.inNames;   % 8 输入名（tables 字段名与其一致）

%% C1 正常启动链：P_commit=500 → 30 s start hold → PENDING@30.00；state_DG2=3@70.01 → TWO
t1 = baseT(IN);
t1.P_commit_kW = [0 500; 1e9 500];
t1.state_DG2   = [0 0; 70 0; 70.01 3; 1e9 3];
o = run_case(h, 120, t1);
tStartCmd = first_true(o.cmd_onoff_DG2);
chk(abs(tStartCmd - 30.00) <= 2*TOL, 'C1 start cmd@%.3f 期望 30.00±0.02', tStartCmd);
chk(all(o.cmd_onoff_DG1.v > 0.5), 'C1 DG1 应从 t=0 恒令 ON（FR-439）');
chk(all(o.cmd_onoff_DG2.v(o.cmd_onoff_DG2.t < 30.0 - 1e-9) < 0.5), 'C1 30 s 前不得令 DG2');
i3 = find(o.commitment_state.v == 3, 1);
chk(abs(o.commitment_state.t(i3) - 70.01) <= 2*TOL, 'C1 TWO_DG 应在 state_DG2==ON 后成立');
chk(all(o.reason_code.v(o.reason_code.t >= tStartCmd - 1e-9) == 1), 'C1 正常启动 reason=1');
rows = audit_rows(o);
r = rows([rows.old] == 1 & [rows.new] == 2);
chk(numel(r) == 1 && r(1).t_start == 3000 && r(1).reason == 1 && r(1).safety == 0, ...
    'C1 正常 start 审计行（t_start=3000 饱和、reason=1、safety=0）');
chk(o.audit_valid.v(end) == 1, 'C1 audit_valid 迁移后保持 1');
chk(~any(o.reason_code.v == 7), 'C1 FORCED(7) 正常路径零出现（负向向量正向侧证据）');
pass('C1 正常启动链 30s hold → PENDING → TWO');

%% C2 正常停机 + 并行计时（min_up 绑定）：TWO@70.01，P<350@80.01（stop_hold 12000@200）；
%   online 30000@370.01（恰 300 s）→ 停机@≈370.02
t2 = baseT(IN);
t2.P_commit_kW = [0 500; 80 500; 80.01 300; 1e9 300];
t2.state_DG2   = [0 0; 70 0; 70.01 3; 1e9 3];
o = run_case(h, 420, t2);
tOn = commitment_time(o, 3, 1);
iStop = find([false; diff(o.commitment_state.v) == -2], 1);
tStop = o.commitment_state.t(iStop);
chk(abs((tStop - tOn) - 300.01) <= 2*TOL, 'C2 最短 ON=恰 300 s（实测 %.3f s）', tStop - tOn);
rows = audit_rows(o);
r = rows([rows.old] == 3 & [rows.new] == 1);
chk(numel(r) == 1 && r(1).t_stop == 12000 && r(1).t_online >= 30000 && r(1).reason == 2, ...
    'C2 正常 stop 审计行（t_stop=12000、t_online>=30000、reason=2）');
pass('C2 正常停机链 + 并行计时 AND 汇合（min_up 绑定）');

%% C3 stop_hold 绑定（online 先满足，P<350 晚来 500 s → 停机 @500.01+120.01）
t3 = baseT(IN);
t3.P_commit_kW = [0 500; 500 500; 500.01 300; 1e9 300];
t3.state_DG2   = [0 0; 70 0; 70.01 3; 1e9 3];
o = run_case(h, 700, t3);
iStop = find([false; diff(o.commitment_state.v) == -2], 1);
tStop = o.commitment_state.t(iStop);
chk(abs((tStop - 500.01) - 120.01) <= 2*TOL, 'C3 stop_hold 绑定：停机@%.2f', tStop);
pass('C3 stop_hold 120s 绑定（online 已先行满足）');

%% C4 滞回带驻留：ONE 态 P=400（<425）不启动；经 force_start 进 TWO 后 P=400（>350）不停机
t4 = baseT(IN);
t4.P_commit_kW         = [0 400; 1e9 400];
t4.state_DG2           = [0 0; 330 0; 330.01 3; 1e9 3];
t4.force_start_dg2_req = [0 0; 300 1; 1e9 1];
t4.force_start_reason  = [0 0; 300 4; 1e9 4];
o = run_case(h, 700, t4);
chk(all(o.commitment_state.v(o.commitment_state.t < 300) == 1), 'C4 滞回带内 ONE 驻留不启动');
chk(all(o.commitment_state.v(o.commitment_state.t >= 330.01) == 3), 'C4 滞回带内 TWO 驻留不停机');
chk(~any(o.cmd_onoff_DG2.v(o.cmd_onoff_DG2.t < 300) > 0.5), 'C4 DG2 在 force 前未令');
pass('C4 滞回带 [350,425] 驻留（状态由历史保持）');

%% C5 min_down=0 即时重启 + PENDING 等物理：停机@370.02 后 P=500@420.01 →
%   30 s hold → 重令@≈450.02；COOLDOWN(state=4) 期间 PENDING 保持等待
t5 = baseT(IN);
t5.P_commit_kW = [0 500; 80 500; 80.01 300; 420 300; 420.01 500; 1e9 500];
% COOLDOWN 注入置于 normal stop（@370.02）完成之后，避免与 online 饱和竞态（真实模型中外部丢失会先被 S1a 锁存）
t5.state_DG2   = [0 0; 70 0; 70.01 3; 380.01 4; 410.01 0; 480.01 3; 1e9 3];
o = run_case(h, 900, t5);
tStop1 = stop_time(o, 1);
tRecmd = first_true_after(o.cmd_onoff_DG2, tStop1);
chk(abs(tRecmd - (420.01 + 30.01)) <= 2*TOL, 'C5 min_down=0：重令@%.3f（期望≈450.02）', tRecmd);
i3b = find(o.commitment_state.v == 3 & o.commitment_state.t > tStop1, 1);
chk(abs(o.commitment_state.t(i3b) - 480.01) <= 2*TOL, 'C5 PENDING 等 state_DG2==ON（COOLDOWN 不打断等待）');
pass('C5 min_down=0 即时重启 + PENDING 等物理');

%% C6 PENDING 等物理 + aborted-start 不中断：PENDING 期间 P_commit 回落不撤指令
t6 = baseT(IN);
t6.P_commit_kW = [0 500; 35 500; 35.01 200; 1e9 200];
t6.state_DG2   = [0 0; 70 0; 70.01 3; 1e9 3];
o = run_case(h, 120, t6);
tStartCmd = first_true(o.cmd_onoff_DG2);
chk(abs(tStartCmd - 30.0) <= 2*TOL, 'C6 start cmd@%.3f', tStartCmd);
seg = o.cmd_onoff_DG2.v(o.cmd_onoff_DG2.t >= 35.01 & o.cmd_onoff_DG2.t < 70);
chk(all(seg > 0.5), 'C6 PENDING 期间负荷回落不撤启动指令（aborted-start 契约）');
pass('C6 PENDING 等待 Plant 物理（不中断、不虚构）');

%% C7 仲裁顺序 veto > safety > normal（D-B7/FR-438）
% a) veto 压过 normal：DG2 故障锁存时 P=500 满 30 s 也不启动
t7a = baseT(IN);
t7a.P_commit_kW     = [0 500; 1e9 500];
t7a.fault_latch_DG2 = [0 1; 1e9 1];
o = run_case(h, 120, t7a);
chk(~any(o.cmd_onoff_DG2.v > 0.5), 'C7a fault veto 禁 START');
chk(all(o.cmd_onoff_DG1.v > 0.5), 'C7a DG1 不受 DG2 fault 影响（lead 常开）');
% b) veto 压过 safety：fault_latch=1 且 force_start_req=1 → 不启动
t7b = baseT(IN);
t7b.P_commit_kW         = [0 500; 1e9 500];
t7b.fault_latch_DG2     = [0 1; 1e9 1];
t7b.force_start_dg2_req = [0 1; 1e9 1];
t7b.force_start_reason  = [0 4; 1e9 4];
o = run_case(h, 60, t7b);
chk(~any(o.cmd_onoff_DG2.v > 0.5), 'C7b veto>safety：故障机不得被 force_start');
% c) safety 压过 normal：ONE 态 P=400（hold 永不满足）+ force_start@10 → PENDING@10，reason=4
t7c = baseT(IN);
t7c.P_commit_kW         = [0 400; 1e9 400];
t7c.force_start_dg2_req = [0 0; 10 1; 1e9 1];
t7c.force_start_reason  = [0 0; 10 4; 1e9 4];
t7c.state_DG2           = [0 0; 40 0; 40.01 3; 1e9 3];
o = run_case(h, 80, t7c);
tForce = first_true(o.cmd_onoff_DG2);
chk(abs(tForce - 10.0) <= 2*TOL, 'C7c force start 绕过全部 timer（@%.3f）', tForce);
rows = audit_rows(o);
r = rows([rows.old] == 1 & [rows.new] == 2);
chk(numel(r) == 1 && r(1).reason == 4 && r(1).safety == 1, 'C7c safety start 审计 reason=4（S2）');
% d) force stop 压过 normal：TWO 态 force_stop@100 → ONE@100，reason=6
t7d = baseT(IN);
t7d.P_commit_kW         = [0 500; 1e9 500];
t7d.state_DG2           = [0 0; 70 0; 70.01 3; 1e9 3];
t7d.force_stop_dg2_req  = [0 0; 100 1; 1e9 1];
t7d.force_stop_reason   = [0 0; 100 6; 1e9 6];
o = run_case(h, 160, t7d);
iStop = find([false; diff(o.commitment_state.v) == -2], 1);
chk(abs(o.commitment_state.t(iStop) - 100.0) <= 2*TOL, 'C7d force stop 绕过 dwell');
rows = audit_rows(o);
r = rows([rows.old] == 3 & [rows.new] == 1);
chk(numel(r) >= 1 && r(1).reason == 6 && r(1).safety == 1, 'C7d force stop 审计 reason=6（S3）');
pass('C7 仲裁顺序 veto > safety > normal');

%% C8 TWO 态 DG2 故障丢失（注入 fault_latch + state 3→4）→ FORCE_STOP 语义回 ONE（reason=3）
t8 = baseT(IN);
t8.P_commit_kW     = [0 500; 1e9 500];
t8.state_DG2       = [0 0; 70 0; 70.01 3; 100 3; 100.01 4; 1e9 4];
t8.fault_latch_DG2 = [0 0; 100.01 1; 1e9 1];
o = run_case(h, 160, t8);
iF = find([false; diff(o.commitment_state.v) == -2], 1);
chk(~isempty(iF) && abs(o.commitment_state.t(iF) - 100.01) <= 2*TOL, 'C8 DG2 丢失后 commitment 回 ONE');
rows = audit_rows(o);
r = rows([rows.old] == 3 & [rows.new] == 1);
chk(numel(r) >= 1 && r(1).reason == 3 && r(1).safety == 1, 'C8 故障停机审计 reason=3（SAFETY_DG_FAULT）');
chk(all(o.cmd_onoff_DG2.v(o.cmd_onoff_DG2.t >= 100.02) < 0.5), 'C8 故障后 DG2 指令撤除（veto 强制 STOP）');
pass('C8 TWO 态 DG2 故障 → ONE_DG（reason=3）');

%% C9 停机后 cmd_onoff=0 波形（输出级映射 cmd_stop=1−cmd_onoff → OFF/COOLDOWN 期阻断重启）
t9 = baseT(IN);
t9.P_commit_kW = [0 500; 80 500; 80.01 300; 1e9 300];
t9.state_DG2   = [0 0; 70 0; 70.01 3; 1e9 3];
o = run_case(h, 420, t9);
tStop = stop_time(o, 1);
seg = o.cmd_onoff_DG2.v(o.cmd_onoff_DG2.t >= tStop + 1);
chk(all(seg < 0.5), 'C9 停机后 cmd_onoff_DG2=0（映射 cmd_stop=1 → 阻断重启）');
pass('C9 停机后启停互锁波形');

% ---- 总判定 ----
fprintf('test_commitment_chart_e ALL PASS\n');

% ========================================================================
%  局部函数（脚本内函数须置于末尾；显式传参，不依赖脚本工作区）
% ========================================================================
function t = baseT(IN)
% 8 张注入表安全基线（无故障/无 force/全零输入），字段名 = harness 输入名
t = struct();
for k = 1:numel(IN)
    t.(IN{k}) = [0 0; 1e9 0];
end
end

function out = run_case(h, tEnd, tables)
% 注入表写入 base（HC_<大写>_TS）→ 设 StopTime → sim → 按名提取 logsout
    names = fieldnames(tables);
    for k = 1:numel(names)
        assignin('base', sprintf('HC_%s_TS', upper(names{k})), tables.(names{k}));
    end
    set_param(h.mdl, 'StopTime', sprintf('%.4f', tEnd));
    so = sim(h.mdl);
    ls = so.logsout;
    out = struct();
    for k = 1:numel(h.outNames)
        sig = ls.get(h.outNames{k});
        out.(h.outNames{k}) = struct('t', sig.Values.Time, 'v', sig.Values.Data);
    end
end

function tv = first_true(sig)
    idx = find(sig.v > 0.5, 1);
    if isempty(idx), tv = NaN; else, tv = sig.t(idx); end
end

function tv = first_true_after(sig, tAfter)
    idx = find(sig.v > 0.5 & sig.t > tAfter, 1);
    if isempty(idx), tv = NaN; else, tv = sig.t(idx); end
end

function t = commitment_time(o, stateVal, occurrence)
    idx = find(o.commitment_state.v == stateVal, occurrence, 'first');
    if isempty(idx), t = NaN; else, t = o.commitment_state.t(idx); end
end

function t = stop_time(o, occurrence)
    idx = find([false; diff(o.commitment_state.v) == -2], occurrence, 'first');
    if isempty(idx), t = NaN; else, t = o.commitment_state.t(idx); end
end

function rows = audit_rows(sig)
% 迁移行提取：audit_new_state 变化时刻 = 迁移拍（迁移后保持至下次迁移）；
% reason/timer/safety/P_commit 在迁移拍取当前（保持）值——entry 动作先记审计后复位 timer
    rows = struct('t', {}, 'old', {}, 'new', {}, 'reason', {}, 't_start', {}, ...
        't_stop', {}, 't_online', {}, 'safety', {}, 'P_commit', {});
    nv = sig.audit_new_state.v; tv = sig.audit_new_state.t;
    for k = 2:numel(nv)
        if nv(k) ~= nv(k-1)
            r = struct('t', tv(k), 'old', sig.audit_old_state.v(k), 'new', nv(k), ...
                'reason', nearv(sig.reason_code, tv(k)), ...
                't_start', nearv(sig.audit_t_start_hold, tv(k)), ...
                't_stop', nearv(sig.audit_t_stop_hold, tv(k)), ...
                't_online', nearv(sig.audit_t_online, tv(k)), ...
                'safety', nearv(sig.audit_safety_override, tv(k)), ...
                'P_commit', nearv(sig.audit_P_commit_kW, tv(k)));
            rows(end+1) = r; %#ok<AGROW>
        end
    end
end

function v = nearv(sig, tq)
    [~, i] = min(abs(sig.t - tq));
    v = sig.v(i);
end

function chk(cond, msg, varargin)
    if ~cond
        error('test_commitment_chart_e FAIL: %s', sprintf(msg, varargin{:}));
    end
end

function pass(caseName)
    fprintf('S PASS: %s\n', caseName);
end