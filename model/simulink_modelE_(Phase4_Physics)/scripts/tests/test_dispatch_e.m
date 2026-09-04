% test_dispatch_e.m — T-003 链 B：DG_Dispatch_Allocator 均分分配器 纯 MATLAB 单测
%   方式：把 +model/code_dispatch_e() 返回的 block 源码字符串写入 tempdir
%   （PMSDispatchAllocatorE.m），addpath(tempdir) 后以 0.01 s 步进直接调用
%   （每调用一次 = 1 tick；本块为组合逻辑，单拍即稳态，S1 内含多拍循环验证无隐藏状态）。
%   每个用例前 clear PMSDispatchAllocatorE 复位 persistent（本块无 persistent，按口径统一执行）；
%   任一断言失败 assert→error 非零退出（matlab -batch 非零退出码）。
%   运行（worktree 内 Model E 工作区根）：
%   matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_dispatch_e.m')"
%   依据：design.md §5.5（is_dispatching 单一谓词，D-B15）+ §2 裁决③ + D-B6；
%   requirements FR-441/442/443；DD-1（genset-side canonical 域）。
%   参数事实源：config/ModelE_Physics_Parameters.m §5（本测试 run 读取，不重议数值；
%   期望值按冻结参数 P_min=125 / P_rated=500 推导，config 变更须同步重推本测试）。

ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
E = ROOT;
addpath(fullfile(E, 'config'), fullfile(E, 'scripts'));
run(fullfile(E, 'config', 'ModelE_Physics_Parameters.m'));

DT = 0.01;  % [s] 离散步（每调用一次 = 1 tick；分配器组合逻辑，输出与步长无积分耦合）

td = tempdir;
write_text(fullfile(td, 'PMSDispatchAllocatorE.m'), model.code_dispatch_e());
addpath(td);
rehash;

PMIN = DG_P_min_kW;    % 125 kW（冻结事实源，仅作入参传递，禁止改值）
PMAX = DG_P_rated_kW;  % 500 kW（同上）
assert(PMIN == 125 && PMAX == 500, ...
    '期望值按冻结参数 P_min=125/P_rated=500 推导：config 变为 %g/%g 须同步重推本测试', PMIN, PMAX);
TOL = 1e-9;

%% S1: 均分基线 + 多拍 0.01 s 步进输出稳定（无隐藏状态）
clear PMSDispatchAllocatorE
P1v = zeros(1, 3); P2v = zeros(1, 3); Nv = zeros(1, 3);
uv = zeros(1, 3); sv = zeros(1, 3); mv = zeros(1, 3); zv = zeros(1, 3);
for k = 1:3  % 3 tick × DT=0.01 s：同输入连拍调用，输出应逐拍相同（块无跨拍记忆）
    [P1v(k), P2v(k), Nv(k), uv(k), sv(k), mv(k), zv(k)] = ...
        PMSDispatchAllocatorE(600, 3, 3, 1, 1, PMIN, PMAX);
end
assert(all(Nv == 2), 'S1: N_dispatch 应=2');
assert(all(abs(P1v - 300) < TOL) && all(abs(P2v - 300) < TOL), 'S1: 均分应 300/300');
assert(all(uv == 0) && all(sv == 0), 'S1: unserved/surplus 应=0');
assert(all(mv == 0) && all(zv == 0), 'S1: min_clamp/n_zero 应=0');
assert(all(P1v == P1v(1)) && all(P2v == P2v(1)) && all(Nv == Nv(1)), ...
    'S1: 多拍输出应逐拍相同（组合逻辑无隐藏状态，每调用一次 = 1 tick = %g s）', DT);
fprintf('S1 PASS: req=600 双机 ON+eligible → 300/300，N=2，诊断全 0；3 拍×%g s 输出逐拍稳定\n', DT);

%% S2: clamp 上界（P_raw>P_max）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(1200, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2, 'S2: N_dispatch 应=2');
assert(abs(P1 - 500) < TOL && abs(P2 - 500) < TOL, 'S2: P_raw=600 应 clamp 到 500/500');
assert(abs(uns - 200) < TOL, 'S2: unserved=max(0,1200−1000) 应=200');
assert(sur == 0, 'S2: surplus 应=0');
assert(mc == 0, 'S2: P_raw≥P_min，min_clamp 应=0（上界 clamp 不置位）');
assert(nz == 0, 'S2: n_zero 应=0');
fprintf('S2 PASS: req=1200 → P_raw=600 clamp 500/500，unserved=200，surplus=0，min_clamp=0\n');

%% S3: clamp 下界（P_raw<P_min，意图置位）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(200, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2, 'S3: N_dispatch 应=2');
assert(abs(P1 - 125) < TOL && abs(P2 - 125) < TOL, 'S3: P_raw=100<P_min 应 clamp 到 125/125');
assert(uns == 0, 'S3: unserved 应=0（clamp 抬升后 ΣP≥req）');
assert(abs(sur - 50) < TOL, 'S3: surplus=max(0,250−200) 应=50');
assert(mc == 1, 'S3: P_raw=100<P_min（严格小于），dispatch_min_clamp_active 应=1');
assert(nz == 0, 'S3: n_zero 应=0');
fprintf('S3 PASS: req=200 → P_raw=100 clamp 125/125，surplus=50，min_clamp=1\n');

%% S4: 单机（DG1 ON+eligible，DG2 OFF）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(500, 3, 0, 1, 0, PMIN, PMAX);
assert(N == 1, 'S4: N_dispatch 应=1');
assert(abs(P1 - 500) < TOL && P2 == 0, 'S4: req=500 单机应 P1=500、P2=0');
assert(uns == 0 && sur == 0 && mc == 0 && nz == 0, 'S4: 诊断应全 0');
fprintf('S4 PASS: DG1 ON+elig / DG2 OFF，req=500 → 500/0，N=1\n');

%% S5: 非 ON 状态排除（is_dispatching 谓词 state==ON 分支；D-B15 单一谓词）
clear PMSDispatchAllocatorE
st_names = {'STARTING(1)', 'SYNCHRONIZING(2)', 'COOLDOWN(4)', 'OFF(0)'};
st_list  = [1 2 4 0];
for si = 1:numel(st_list)
    clear PMSDispatchAllocatorE
    [P1, P2, N, uns, sur, mc, nz] = ...
        PMSDispatchAllocatorE(400, st_list(si), 3, 1, 1, PMIN, PMAX);
    assert(P1 == 0, 'S5: %s+eligible 机组 ref 必须为 0（A2 不虚构功率契约）', st_names{si});
    assert(N == 1, 'S5: %s 不计入 N_dispatch，N 应=1（另一机 ON）', st_names{si});
    assert(abs(P2 - 400) < TOL, 'S5: %s 期间另一 ON 机应独立承担 req=400', st_names{si});
    assert(uns == 0 && sur == 0 && mc == 0 && nz == 0, 'S5: %s 诊断应全 0', st_names{si});
end
fprintf('S5 PASS: state1=STARTING/SYNC/COOLDOWN/OFF 且 eligible=1 → P1_ref=0、N=1（全部排除）\n');

%% S6: eligible=0 排除（fault 裁决后语义；谓词 eligible 分支）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(460, 3, 3, 0, 1, PMIN, PMAX);
assert(P1 == 0, 'S6: state=ON 但 eligible=0（fault 裁决后）必须 ref=0');
assert(N == 1, 'S6: eligible=0 不计入 N_dispatch，N 应=1');
assert(abs(P2 - 460) < TOL, 'S6: 另一 ON+eligible 机应独立承担 req=460');
assert(uns == 0 && sur == 0 && mc == 0 && nz == 0, 'S6: 诊断应全 0');
fprintf('S6 PASS: state1=3 但 eligible1=0 → 排除（fault 裁决后语义），P1_ref=0，N=1\n');

%% S7: N=0 防御（voyage active 期间不应发生；flag 显式暴露不虚构功率）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(500, 0, 0, 1, 1, PMIN, PMAX);
assert(P1 == 0 && P2 == 0, 'S7: N=0 双机 ref 应=0');
assert(N == 0 && nz == 1, 'S7: N_dispatch=0 应置 dispatch_n_zero_flag=1');
assert(abs(uns - 500) < TOL, 'S7: unserved=max(0,req−0) 应=req=500（不静默吞掉请求）');
assert(sur == 0 && mc == 0, 'S7: surplus/min_clamp 应=0');
% 子用例：混合非 ON 状态（COOLDOWN+STARTING，eligible 均=1）同样 N=0
clear PMSDispatchAllocatorE
[P1b, P2b, Nb, unsb, surb, mcb, nzb] = ...
    PMSDispatchAllocatorE(300, 4, 1, 1, 1, PMIN, PMAX);
assert(Nb == 0 && nzb == 1, 'S7: COOLDOWN+STARTING → N=0、n_zero=1（非 ON 态必排除）');
assert(P1b == 0 && P2b == 0, 'S7: 混合非 ON 态双机 ref 应=0');
assert(abs(unsb - 300) < TOL && surb == 0 && mcb == 0, 'S7: 混合非 ON 态诊断口径应与 S7 一致');
fprintf('S7 PASS: 双机 OFF（及 COOLDOWN+STARTING 混合）→ 0/0，n_zero=1，unserved=req\n');

%% S8: 诊断三件套数值（FR-442 公式逐项校验）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(750, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2 && abs(P1 - 375) < TOL && abs(P2 - 375) < TOL, 'S8a: req=750 → 375/375');
assert(uns == 0 && sur == 0 && mc == 0 && nz == 0, 'S8a: 域内均分诊断应全 0');
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(90, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2, 'S8b: N_dispatch 应=2');
assert(abs(P1 - 125) < TOL && abs(P2 - 125) < TOL, 'S8b: P_raw=45<P_min 应 clamp 125/125');
assert(abs(sur - 160) < TOL, 'S8b: surplus=max(0,250−90) 应=160');
assert(uns == 0 && mc == 1 && nz == 0, 'S8b: unserved=0、min_clamp=1、n_zero=0');
fprintf('S8 PASS: req=750 → 375/375 诊断全 0；req=90 → 125/125，surplus=160，min_clamp=1\n');

%% S9: 边界等号（P_raw==P_min 不置位；P_raw<P_min 严格小于才置位）
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(250, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2 && abs(P1 - 125) < TOL && abs(P2 - 125) < TOL, 'S9a: req=250 → P_raw=125 → 125/125');
assert(mc == 0, 'S9a: P_raw==P_min 等号不触发（P_raw<DG_P_min 为严格小于）');
assert(uns == 0 && sur == 0 && nz == 0, 'S9a: 边界等号诊断应全 0');
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(249.98, 3, 3, 1, 1, PMIN, PMAX);
assert(N == 2 && abs(P1 - 125) < TOL && abs(P2 - 125) < TOL, 'S9b: req=249.98 → P_raw=124.99 → clamp 125/125');
assert(mc == 1, 'S9b: P_raw=124.99<P_min，min_clamp 应=1');
assert(uns == 0, 'S9b: unserved 应=0');
assert(abs(sur - 0.02) < 1e-9, 'S9b: surplus=max(0,250−249.98) 应≈0.02（实测 %.12f）', sur);
assert(nz == 0, 'S9b: n_zero 应=0');
fprintf('S9 PASS: req=250（P_raw==125 等号）min_clamp=0；req=249.98（P_raw=124.99）min_clamp=1，surplus≈0.02\n');

%% S10: STARTING+healthy+available ⇒ P_ref==0 专项（A2 不得虚构输出功率契约，任务书点名必含项）
%   说明：healthy∧available 经 Commitment/fault 裁决后的显式通道表达为 dispatch_eligible=1；
%   即机组完全健康可用、仅处 STARTING 物理过程时，allocator 仍必须输出 ref==0。
clear PMSDispatchAllocatorE
[P1, P2, N, uns, sur, mc, nz] = PMSDispatchAllocatorE(750, 1, 3, 1, 1, PMIN, PMAX);
assert(P1 == 0, 'S10/A2契约: STARTING+eligible=1（healthy∧available）机组 P_ref 必须为 0');
assert(N == 1, 'S10: STARTING 不计入 N_dispatch，N 应=1');
assert(abs(P2 - 500) < TOL, 'S10: 另一 ON 机 P_raw=750 应 clamp 500 独立承担');
assert(abs(uns - 250) < TOL, 'S10: unserved=max(0,750−500) 应=250（纯诊断，红线 b）');
assert(sur == 0 && mc == 0 && nz == 0, 'S10: surplus/min_clamp/n_zero 应=0');
fprintf('S10 PASS: state1=STARTING(1)+eligible=1，req=750 → P1_ref=0、N=1（A2 不得虚构输出功率契约）\n');

fprintf('T003-CHAIN-B-DISPATCH: ALL PASS\n');

function write_text(path, txt)
%WRITE_TEXT 把 block 源码字符串写盘（tempdir 测试实例）
fid = fopen(path, 'w');
assert(fid > 0, '无法写入 %s', path);
fwrite(fid, txt, 'char');
fclose(fid);
end