function c = code_dispatch_e()
%CODE_DISPATCH_E 返回 PMSDispatchAllocatorE MATLAB Function block 源码字符串（Model E，T-003 链 B）
%   c = model.code_dispatch_e()
%   用途：builder 经 setMFCode 写入 07_PMS_v2 内 DG_Dispatch_Allocator MATLAB Function 块
%   （输入 P_gen_req_eff 与 DG1/DG2 state、dispatch_eligible 均为 Commitment/fault 裁决后信号）；
%   纯 MATLAB 单测（scripts/tests/test_dispatch_e.m）亦以此字符串写盘直接调用。
%   依据：.specs/phase4-b-pms-safety/design.md §5.5（is_dispatching 单一谓词，D-B15 评审修订）
%   + §2 裁决③ + D-B6（分配律冻结）；requirements FR-441/442/443；
%   DD-1：canonical 域 = genset-side，dispatch 输出直连 Plant 零换算。
%   采样约定：固定步 0.01 s，每步调用一次 = 1 tick；dt=0.01 于块源码内登记，
%   若模型固定步长变更须一并重推（同 GensetUnitE OQ-6 口径）。
%   参数事实源：config/ModelE_Physics_Parameters.m §5（DG_P_min_kW / DG_P_rated_kW 数值冻结，
%   全部经函数入参传入，禁止在本块内写死或重议）。

lines = { ...
'function [P_DG1_ref_kW, P_DG2_ref_kW, N_dispatch, P_gen_unserved_kW, P_gen_surplus_kW, dispatch_min_clamp_active, dispatch_n_zero_flag] = PMSDispatchAllocatorE(P_gen_req_eff_kW, state_DG1, state_DG2, dispatch_eligible_DG1, dispatch_eligible_DG2, DG_P_min_kW, DG_P_rated_kW)'
'%#codegen'
'% PMSDispatchAllocatorE：DG_Dispatch_Allocator 均分分配器（Model E, FR-441/442/443）'
'%   依据：design.md §5.5（is_dispatching 单一谓词，D-B15）+ §2 裁决③ + D-B6（分配律冻结）'
'%   + DD-1（canonical 域 = genset-side，dispatch 输出直连 Plant 零换算）。'
'%   采样约定：固定步 0.01 s，每调用一次 = 1 tick；本块为组合逻辑（无 persistent 状态，'
'%   state/eligible 输入均为 Commitment/fault 裁决后当拍值），dt 仅作口径登记——'
'%   若模型固定步长变更须一并重推（同 GensetUnitE OQ-6 口径）。'
'%   机组状态枚举（A2 GensetUnitE 口径）：0=OFF 1=STARTING 2=SYNCHRONIZING 3=ON 4=COOLDOWN。'
'%'
'%   is_dispatching 单一谓词（D-B15）：分母与输出共用同一谓词，禁止分母查 state 而'
'%   输出只查 eligible 的分裂写法（分裂可致 STARTING 机组获得非零 ref，违反 A2 契约）：'
'%       is_dispatching_i = (state_i==ON) ∧ dispatch_eligible_i；'
'%   STARTING/SYNC/COOLDOWN/OFF 对 N_dispatch 的贡献必为 0。'
'%'
'%   语义红线 a（A2 不得虚构输出功率契约）：STARTING/SYNC/COOLDOWN 机组即使'
'%   healthy∧available 也必须 ref==0（单测必含 STARTING+healthy+available ⇒ P_ref==0 case）。'
'%   语义红线 b（D-B14）：P_gen_unserved_kW 为 generation-command mismatch 纯诊断量'
'%   （EMS 过度请求≠真实缺口），禁用于 Safety 判据——S2 实际缺口判据用 realized'
'%   supply deficit 口径（design §5.6）。'
'%   语义红线 c（design §2 裁决③）：dispatch_min_clamp_active 为控制器意图语义'
'%   （is_dispatching ∧ P_raw<DG_P_min，严格小于），与 Plant below_min_flag'
'%   （state==3 && P_ref_kW<DG_P_min_kW 的意图口径，A2 GensetUnitE 输出约定）区分。'
'%'
'%   DISPATCH_MODE 登记（FR-443/D-B1，仅注释与接口文档，不写第二模式代码分支）：'
'%       0=AGGREGATE_AUTO（本块实现）/ 1=EXTERNAL_PER_UNIT（reserved）。'
'%   禁止 BSFC-aware 分配、不对称加载与轮换——DG1=lead / DG2=lag 固定对称均分'
'%   （D-B6 冻结，equal-rated parallel gensets under symmetric load-sharing configuration）。'
'%   N_dispatch==0 为防御分支（voyage active 期间不应发生，v1 无 0DG 基线假设）：'
'%   双机 ref=0 + dispatch_n_zero_flag=1，不虚构功率、不静默吞掉请求。'
'%'
'%   参数纪律：DG_P_min_kW / DG_P_rated_kW 全部经入参传入（数值事实源'
'%   config/ModelE_Physics_Parameters.m §5，禁止在此写死或重议）。'
''
'dt = 0.01;  % [s] 离散步长登记（每调用一次 = 1 tick）；本块无时间积分，dt 仅口径声明——若模型固定步长变更须一并重推（同 GensetUnitE OQ-6 口径）'
''
'% ---- is_dispatching 单一谓词（分母与输出共用同一谓词，D-B15）----'
'is1 = (state_DG1 == 3) && (dispatch_eligible_DG1 > 0.5);  % 3=ON 且裁决后 eligible 才参与'
'is2 = (state_DG2 == 3) && (dispatch_eligible_DG2 > 0.5);  % STARTING/SYNC/COOLDOWN/OFF 必排除'
'N_dispatch = double(is1) + double(is2);                   % 参与均分的机组数'
''
'% ---- 输出初值（N=0 防御分支语义：ref=0、surplus=0、min_clamp=0、unserved=max(0,req−0)）----'
'P_DG1_ref_kW = 0;'
'P_DG2_ref_kW = 0;'
'P_gen_unserved_kW  = max(0, P_gen_req_eff_kW);   % ΣP_i_ref=0 → unserved=max(0,req)'
'P_gen_surplus_kW   = 0;'
'dispatch_min_clamp_active = 0;'
'dispatch_n_zero_flag = 0;'
''
'if N_dispatch == 0'
'    % 防御：N_dispatch==0（不应发生于 voyage active）——flag 显式暴露，不虚构功率'
'    dispatch_n_zero_flag = 1;'
'else'
'    % ---- 均分 + clamp（FR-441/D-B6：P_i,ref = clamp(P_gen_req_eff/N_dispatch, P_min, P_max)）----'
'    P_raw = P_gen_req_eff_kW / N_dispatch;'
'    if is1'
'        P_DG1_ref_kW = min(max(P_raw, DG_P_min_kW), DG_P_rated_kW);'
'    end'
'    if is2'
'        P_DG2_ref_kW = min(max(P_raw, DG_P_min_kW), DG_P_rated_kW);'
'    end'
'    % ---- 诊断三件套（FR-442）----'
'    P_sum = P_DG1_ref_kW + P_DG2_ref_kW;'
'    P_gen_unserved_kW = max(0, P_gen_req_eff_kW - P_sum);  % 红线 b：纯诊断，禁用于 Safety 判据'
'    P_gen_surplus_kW  = max(0, P_sum - P_gen_req_eff_kW);'
'    % 红线 c：控制器意图语义——任一参与机组 P_raw 低于 P_min（严格小于，等号不触发）'
'    if (is1 && (P_raw < DG_P_min_kW)) || (is2 && (P_raw < DG_P_min_kW))'
'        dispatch_min_clamp_active = 1;'
'    end'
'end'
'end'};
c = strjoin(lines, newline);
end