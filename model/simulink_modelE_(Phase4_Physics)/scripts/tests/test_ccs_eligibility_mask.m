% test_ccs_eligibility_mask.m — T-005：10b CCS 稳态 eligibility mask 纯 MATLAB 单测
%   覆盖（DGN-14）：①稳态合成场景 eligible==t>60 且豁免窗 ②DG2 中途启动 settling 排除
%   ③v_ref 阶跃排除 ④全不 eligible（fail-closed 前提：n_eligible==0）⑤N_dispatch
%   不一致排除 ⑥阈值/编码消费（0=OFF/1=STARTING/2=SYNCHRONIZING/3=ON/4=COOLDOWN）。
%   运行：matlab -sd "Model E 工作区" -batch "run('scripts/tests/test_ccs_eligibility_mask.m')"

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));

DT = 0.01;
nFail = 0;

%% C1: 稳态合成（DG1 启动 0~30 s 后 ON，DG2 恒 OFF）
t = (0:DT:600)';
v_ref = 5.66 * ones(size(t));
st1 = zeros(size(t)); st1(t >= 25.01) = 1; st1(t >= 30.01) = 2; st1(t >= 30.02) = 3;
st2 = zeros(size(t));
ndisp = double(st1 == 3) + double(st2 == 3);
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, 0);
% 结构化断言（避开 0.01 步进浮点逐点比较）：计数 + 豁免窗数 + 排除域单侧检查
iE1 = find(m.eligible);
ok1 = m.n_eligible == 53999 && m.n_exemption_windows == 1 ...
    && ~any(m.eligible & t < 60.01 - 1e-9) ...
    && abs(t(iE1(1)) - 60.02) <= 0.011;
fprintf('[T] C1 steady: n_elig=%d (期望 53999) windows=%d -> %d\n', ...
    m.n_eligible, m.n_exemption_windows, ok1);
nFail = nFail + ~ok1;

%% C2: DG2 中途启动（t=500 起 STARTING/SYNC→ON），settling 30 s 排除
t = (0:DT:900)';
v_ref = 5.66 * ones(size(t));
st1 = 3 * ones(size(t));
st2 = zeros(size(t)); st2(t >= 500.01) = 1; st2(t >= 525.01) = 2; st2(t >= 530.01) = 3;
ndisp = double(st1 == 3) + double(st2 == 3);
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, 0);
% 迁移首样 t=500.01 → settling 排除至 530.01+30；启动窗排除 t<=60
elig_expect = t > 60 & ~(t >= 500.01 & t < 530.01 + 30.0);
ok2 = all(m.eligible == elig_expect) && m.n_eligible > 0;
fprintf('[T] C2 DG2 midstart: n_elig=%d 期望 %d -> %d\n', m.n_eligible, sum(elig_expect), ok2);
nFail = nFail + ~ok2;

%% C3: v_ref 阶跃（t=400 处 +0.5 阶跃）→ [400, 430) 排除
t = (0:DT:600)';
v_ref = 5.66 * ones(size(t)); v_ref(t >= 400.01) = 6.16;
st1 = 3 * ones(size(t)); st2 = zeros(size(t));
ndisp = ones(size(t));
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, 0);
elig_expect = t > 60 & ~(t >= 400.01 & t < 430.01);
ok3 = all(m.eligible == elig_expect);
fprintf('[T] C3 v_ref step: n_elig=%d 期望 %d -> %d\n', m.n_eligible, sum(elig_expect), ok3);
nFail = nFail + ~ok3;

%% C4: 全不 eligible（DG 恒 STARTING）→ fail-closed 前提（n_eligible==0）
t = (0:DT:300)';
v_ref = 5.66 * ones(size(t));
st1 = ones(size(t)); st2 = zeros(size(t));   % DG1 恒 STARTING（非法驻留仅作构造）
ndisp = zeros(size(t));
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, 0);
ok4 = m.n_eligible == 0;   % 10b 判据段对 n_eligible==0 error('GATE:*')
fprintf('[T] C4 all-ineligible: n_elig=%d -> %d\n', m.n_eligible, ok4);
nFail = nFail + ~ok4;

%% C5: N_dispatch 与 ON 机组数不一致 → 排除
t = (0:DT:300)';
v_ref = 5.66 * ones(size(t));
st1 = 3 * ones(size(t)); st2 = zeros(size(t));
ndisp = 2 * ones(size(t));   % 谎报双机
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, 0);
ok5 = m.n_eligible == 0;
fprintf('[T] C5 dispatch inconsistent: n_elig=%d -> %d\n', m.n_eligible, ok5);
nFail = nFail + ~ok5;

%% C6: 注入激活排除
t = (0:DT:300)';
v_ref = 5.66 * ones(size(t));
st1 = 3 * ones(size(t)); st2 = zeros(size(t)); ndisp = ones(size(t));
inj = zeros(size(t)); inj(t >= 100 & t < 150) = 1;
m = gate_e.ccs_eligibility_mask(t, v_ref, st1, st2, ndisp, inj);
ok6 = ~any(m.eligible(t >= 100 & t < 150)) && any(m.eligible(t > 200));
fprintf('[T] C6 injection window excluded -> %d\n', ok6);
nFail = nFail + ~ok6;

if nFail > 0
    error('GATE:test_ccs_eligibility_mask %d 项失败', nFail);
end
fprintf('test_ccs_eligibility_mask: ALL PASS（6/6）\n');
