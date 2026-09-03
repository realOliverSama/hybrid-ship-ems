function c = code_commitment_adapter_e()
%CODE_COMMITMENT_ADAPTER_E 返回 LegacyCommitmentAdapter block 源码字符串（Model E，T-006）
%   c = model.code_commitment_adapter_e()
%   用途：builder 经 setMFCode 写入顶层 Legacy_Commitment_Adapter（占位组合适配器，FR-421，
%   Plant 之外的可拔插块；B 包 PMS v2 (T-415/T-416) 直接整体替换本块，不动被控对象）；
%   纯 MATLAB 单测（scripts/tests/test_genset_statemachine.m S7）以此字符串写盘直接调用。
%   依据：design.md §4.3 + FR-421。单机额定 500 kW 为冻结事实源（config §5 DG_P_rated_kW），
%   adapter 签名无参数端口，故 500 以字面量写入并注释锚定。

lines = { ...
'function [DG1_enable, P_DG1_ref_kW, DG2_enable, P_DG2_ref_kW] = LegacyCommitmentAdapter(...'
'    P_gen_ref_kW, force_mode, force_P1_kW, force_P2_kW)'
'%#codegen'
'% LegacyCommitmentAdapter：占位机组组合适配器（Model E, FR-421, design 4.3）'
'% PLACEHOLDER (FR-421): 占位机组组合逻辑，不得用于任何正式 EMS 经济性/节油结论；B 包 PMS v2 (T-415/T-416) 直接替换本 adapter，不动被控对象'
'%   auto（force_mode<0.5）：P_gen_ref_kW<=500 → DG1=1/P1=ref、DG2=0/P2=0；'
'%                           P_gen_ref_kW>500 → DG1=1/P1=500、DG2=1/P2=ref-500。'
'%   force（force_mode>=0.5）：直接采用 force_P1/force_P2（scenario 脚本化强制组合测试用）；'
'%                           enable 由对应 force 功率>0 派生（=0 表示该机组不投入）。'
'%   上游 EMS 已把 P_gen_ref_kW clamp 到 Gen_P_max(=1000)，故 auto 分支 DG2_ref<=500 恒成立。'
'%   本 adapter 只输出 enable/参考值：机组 STARTING/SYNC 期间不虚构其功率，'
'%   启动缺口由电池下垂/母线自然响应（design 4.3 设计意图，无需 adapter 处理）。'
'if force_mode < 0.5'
'    if P_gen_ref_kW <= 500'
'        DG1_enable = 1;'
'        P_DG1_ref_kW = P_gen_ref_kW;'
'        DG2_enable = 0;'
'        P_DG2_ref_kW = 0;'
'    else'
'        DG1_enable = 1;'
'        P_DG1_ref_kW = 500;              % 字面 500 = 单机额定（DG_P_rated_kW，config §5 冻结）'
'        DG2_enable = 1;'
'        P_DG2_ref_kW = P_gen_ref_kW - 500;'
'    end'
'else'
'    DG1_enable = double(force_P1_kW > 0);'
'    P_DG1_ref_kW = force_P1_kW;'
'    DG2_enable = double(force_P2_kW > 0);'
'    P_DG2_ref_kW = force_P2_kW;'
'end'
'end'};
c = strjoin(lines, newline);
end