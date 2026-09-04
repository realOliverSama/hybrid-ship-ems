% test_pms_demand_e.m — T-002 链 A：Baseline_Demand_Policy（PMSDemandPolicyE）纯 MATLAB 单测
%   方式：把 +model/code_pms_demand_e() 返回的 block 源码字符串写入 tempdir（PMSDemandPolicyE.m），
%   addpath(tempdir) 后以 0.01 s 步进直接调用（每调用一次 = 1 tick；无 Simulink/Stateflow）。
%   每个用例前 clear PMSDemandPolicyE 复位 persistent；任一断言失败 error() 非零退出。
%   运行（cd Model E 工作区根）：
%   matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_pms_demand_e.m')"
%   依据：design.md §5.3 + requirements FR-432~435（AC-417）；DD-1 canonical 域；D-B3/D-B4。
%   参数事实源：config/ModelE_Control_Parameters.m（FR-451；先 Physics 后 Control 顺序加载，
%   本测试只引用变量值、不重议数值——全部参数经函数入参传入，含 DG_P_rated_kW）。
%   用例编排注记：S1b 的解析式 y(k)=300(1-(1-a)^k) 对应阶跃前预稳态 y[0]=x[0]=0
%   （先喂一拍 0 kW，再恒定 300 kW 逐拍 8000 步）；S2 第 4 拍以 SOC=0.55 重进为第 5 拍
%   0.5549 hold 断言构造 true 前置态（同任务 S4 注记的状态构造自由度）。

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));
run(fullfile(E, 'config', 'ModelE_Control_Parameters.m'));

DT = 0.01;  % [s] 离散步（与 block 源码内硬编码 dt 一致；每调用一次 = 1 tick）
td = tempdir;
demand_code = model.code_pms_demand_e();
SIG1 = 'function [P_gen_req_baseline_kW, P_commit_kW, u_recovery, P_recovery_actual_kW] = PMSDemandPolicyE(P_load_gen_kW, SOC, N_dispatch_fb, PMS_LoadLPF_tau_s, PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW)';
sig_lines = strsplit(demand_code, newline);
assert(strcmp(sig_lines{1}, SIG1), 'block 源码首行 function 签名与冻结签名逐字符不一致');
write_text(fullfile(td, 'PMSDemandPolicyE.m'), demand_code);
addpath(td);
rehash;

%% S1: LPF 阶跃响应 τ 校验（FR-432；前向欧拉解析式 + y[0]=x[0] 语义）
clear PMSDemandPolicyE
% S1a 初拍语义：首拍输出应 = 首输入（y[0]=x[0]，design §5.3；非零输入下严格成立）
[reqA, comA, uA, recA] = PMSDemandPolicyE(300, 0.57, 0, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(comA == 300 && reqA == 300, ...
    'S1a: 初拍 y(1) 应=x(1)=300（y[0]=x[0]），实测 com=%.10g req=%.10g', comA, reqA);
assert(~uA && recA == 0, 'S1a: 初拍 u_recovery 应=false（FR-433 初值），P_recovery_actual 应=0');
% S1b 阶跃响应：先喂一拍 0 kW（预稳态 y[0]=x[0]=0），随后恒定 300 kW 逐拍 8000 步
clear PMSDemandPolicyE
[~, comB0, ~, ~] = PMSDemandPolicyE(0, 0.57, 0, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(comB0 == 0, 'S1b: 预稳态拍 y=x=0 不成立（y[0]=x[0]），实测 %.10g', comB0);
N1 = 8000;
yv = zeros(1, N1);
for k = 1:N1
    [~, yv(k), ~, ~] = PMSDemandPolicyE(300, 0.57, 0, PMS_LoadLPF_tau_s, ...
        PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
end
a_e = DT / PMS_LoadLPF_tau_s;               % 与 block 内 0.01/τ 同式同值（a=0.01/60）
ya = 300 * (1 - (1 - a_e).^(1:N1));         % 解析式：y(k)=300(1-(1-a)^k)，y(0)=0
rel = abs(yv - ya) ./ ya;
assert(all(rel < 1e-9), 'S1b: LPF 解析式逐点相对误差超差（max=%.3e ≥ 1e-9）', max(rel));
ks = [1 100 6000 8000];
assert(all(rel(ks) < 1e-9), 'S1b: 抽查点 k=1/100/6000/8000 相对误差超差');
y6k = yv(6000);
assert(abs(y6k - 300*(1 - exp(-1))) / 300 < 1e-3, ...
    'S1b: k=6000（60 s）y=%.4f 应≈300*(1-exp(-1))=%.4f', y6k, 300*(1 - exp(-1)));
fprintf('S1 PASS: 初拍 y(1)=x(1)=300；阶跃 8000 拍解析式逐点相对误差 max=%.2e（<1e-9）；y(6000)=%.3f ≈ %.3f（60 s，τ 校验）\n', ...
    max(rel), y6k, 300*(1 - exp(-1)));

%% S2: SOC 滞回进出含等号边界（FR-433 比较符号冻结：<=0.55 进 / >=0.60 出 / 其间保持）
clear PMSDemandPolicyE
soc_seq2 = [0.55 0.60 0.57 0.55 0.5549 0.6001];
uv2 = false(1, numel(soc_seq2));
for k = 1:numel(soc_seq2)
    [~, ~, uv2(k), ~] = PMSDemandPolicyE(300, soc_seq2(k), 0, PMS_LoadLPF_tau_s, ...
        PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
end
assert(uv2(1),  'S2: SOC=0.55 精确 → 应 true（<= 含等号，FR-433）');
assert(~uv2(2), 'S2: 随后 SOC=0.60 精确 → 应 false（>= 含等号，FR-433）');
assert(~uv2(3), 'S2: SOC=0.57 应保持前值 false（带内保持）');
assert(uv2(4),  'S2: SOC=0.55 重进 → true（为第 5 拍 hold 断言构造前置态）');
assert(uv2(5),  'S2: SOC=0.5549（带内恰高于 lo）应保持 true');
assert(~uv2(6), 'S2: SOC=0.6001（恰高于 recovered）应清 false');
fprintf('S2 PASS: 等号边界 0.55→true / 0.60→false；带内 0.57、0.5549 保持；0.6001→false\n');

%% S3: 初值 false（FR-433：全新 persistent 首拍 SOC=0.57 → u_recovery=false）
clear PMSDemandPolicyE
[~, ~, u3, ~] = PMSDemandPolicyE(300, 0.57, 0, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(islogical(u3) && ~u3, 'S3: 全新 persistent 首拍 SOC=0.57 应 u_recovery=false（逻辑型初值）');
fprintf('S3 PASS: 初值 u_recovery=false（FR-433）\n');

%% S4: 削减公式两档 + N=0 防御（u_recovery=true 时；FR-434，AC-417 含 max(0,…) 与单机/双机两档）
% 每个 sub-case：clear 后先以 SOC=0.50（<=PMS_SOC_lo）喂一拍构造 u_recovery=true（任务 S4 注记），
% 预置拍同喂目标 P_load 使 P_load_filt 精确=目标（常数输入下 LPF 恒等）；随后同输入断言拍。
s4P = [300 450 520 950 1050 300];   % P_load 目标（kW，generator-equivalent）
s4N = [  1   1   1   2    2   0];   % N_dispatch_fb
s4R = [100  50   0  50    0   0];   % 期望 P_recovery_actual = max(0, min(100, 500N - P_load))
for i = 1:numel(s4P)
    clear PMSDemandPolicyE
    Px = s4P(i); Nd = s4N(i); Rx = s4R(i);
    [~, comP, uP, rP] = PMSDemandPolicyE(Px, 0.50, Nd, PMS_LoadLPF_tau_s, ...
        PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
    assert(uP, 'S4(%d): 预置拍 SOC=0.50 应置 u_recovery=true', i);
    assert(comP == Px, 'S4(%d): 预置拍 P_commit 应=%g（y[0]=x[0]+常数输入恒等）', i, Px);
    assert(rP == Rx, 'S4(%d): 预置拍 P_recovery_actual 应=%g（FR-434），实测 %.10g', i, Rx, rP);
    [reqS4, comS4, uS4, rS4] = PMSDemandPolicyE(Px, 0.50, Nd, PMS_LoadLPF_tau_s, ...
        PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
    assert(uS4 && rS4 == Rx, ...
        'S4(%d): P_load=%g / N=%d → P_recovery_actual 应=%g（FR-434），实测 %.10g', i, Px, Nd, Rx, rS4);
    assert(comS4 == Px, 'S4(%d): P_commit=%g 应不含 recovery 偏置（D-B4 方案 b）', i, Px);
    assert(reqS4 == Px + Rx, 'S4(%d): P_gen_req_baseline 应=LPF+recovery=%g，实测 %.10g', i, Px + Rx, reqS4);
end
fprintf('S4 PASS: N=1 cap500：300→100 / 450→50 / 520→max(0,-20)=0；N=2 cap1000：950→50 / 1050→0；N=0→0（防御）；P_commit 无偏置\n');

%% S5: N_dispatch_fb 直接消费（当拍生效，无内部延迟；上一拍语义由调用方 UnitDelay 保证，design §5.3）
clear PMSDemandPolicyE
[~, ~, u5a, r5a] = PMSDemandPolicyE(300, 0.50, 0, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(u5a && r5a == 0, 'S5: 预置拍（SOC=0.50, N=0）应 u_recovery=true 且 P_recovery_actual=0');
[~, ~, u5b, r5b] = PMSDemandPolicyE(300, 0.50, 2, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(u5b && r5b == 100, ...
    'S5: 同输入仅改 N_dispatch_fb 0→2，当拍 P_recovery_actual 应立即=100（无内部延迟），实测 %.10g', r5b);
[~, ~, ~, r5c] = PMSDemandPolicyE(300, 0.50, 0, PMS_LoadLPF_tau_s, ...
    PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
assert(r5c == 0, 'S5: 仅改 N_dispatch_fb 2→0，当拍 P_recovery_actual 应立即=0，实测 %.10g', r5c);
fprintf('S5 PASS: N_dispatch_fb 当拍直接消费（0→2→0 时 P_recovery 0→100→0），无内部延迟\n');

%% S6: 连续运行恢复循环（SOC 0.70→0.54→0.58→0.61→0.58 整环；FR-433/434）
clear PMSDemandPolicyE
soc6 = [0.70 0.54 0.58 0.61 0.58];
exp6u = [false true true false false];
exp6r = [0 100 100 0 0];
uv6 = false(1, 5); rv6 = zeros(1, 5); cv6 = zeros(1, 5); qv6 = zeros(1, 5);
for k = 1:5
    [qv6(k), cv6(k), uv6(k), rv6(k)] = PMSDemandPolicyE(300, soc6(k), 1, PMS_LoadLPF_tau_s, ...
        PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW);
end
assert(isequal(uv6, exp6u), ...
    'S6: u_recovery 序列应为 [F T T F F]（0.70→false、0.54→true、0.58 保持、0.61→false、0.58 保持）');
assert(all(rv6 == exp6r), 'S6: P_recovery_actual 序列应为 [0 100 100 0 0]（FR-434，N=1 cap=500）');
assert(all(cv6 == 300), 'S6: 恒定负荷下 P_commit 应恒 300（LPF 常数输入保持）');
assert(all(qv6 == cv6 + rv6), 'S6: P_gen_req_baseline 应逐拍 = P_commit + P_recovery_actual');
fprintf('S6 PASS: 恢复循环整环 0.70(F)→0.54(T)→0.58(T 保持)→0.61(F)→0.58(F 保持)；recovery 偏置随滞回归零/恢复\n');

fprintf('T002-CHAIN-A-PMS-DEMAND: ALL PASS\n');

function write_text(path, txt)
%WRITE_TEXT 把 block 源码字符串写盘（tempdir 测试实例）
fid = fopen(path, 'w');
assert(fid > 0, '无法写入 %s', path);
fwrite(fid, txt, 'char');
fclose(fid);
end