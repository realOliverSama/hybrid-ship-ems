function c = code_pms_demand_e()
%CODE_PMS_DEMAND_E 返回 PMSDemandPolicyE MATLAB Function block 源码字符串（Phase 4-B T-002，链 A）
%   c = model.code_pms_demand_e()
%   用途：builder 经 setMFCode 写入 07_PMS_v2 内 Baseline_Demand_Policy MATLAB Function 块
%   （离散 0.01 s，EMChart DISCRETE 口径由 builder 侧设定）；纯 MATLAB 单测
%   （scripts/tests/test_pms_demand_e.m）亦以此字符串写盘直接调用（design §5.3，A2 单源模式）。
%   依据：.specs/phase4-b-pms-safety/design.md §5.3 + requirements FR-432~435（AC-417）；
%   DD-1 canonical 功率域（genset-side generator-equivalent）；D-B3（Demand Policy 四数冻结）/
%   D-B4（P_commit 信号语义方案 b）。
%   参数：FR-435 四参数（60 s / 0.55 / 100 kW / 0.60，全 ENGINEERING_SURROGATE）与 DG_P_rated_kW
%   全部经函数入参传入，数值事实源 config/ModelE_Control_Parameters.m（FR-451，
%   DG_P_rated_kW 引自 ModelE_Physics_Parameters.m），本文件禁止重议任何数值。

lines = { ...
'function [P_gen_req_baseline_kW, P_commit_kW, u_recovery, P_recovery_actual_kW] = PMSDemandPolicyE(P_load_gen_kW, SOC, N_dispatch_fb, PMS_LoadLPF_tau_s, PMS_SOC_lo, PMS_SOC_recovered, PMS_P_chg_recovery_kW, DG_P_rated_kW)'
'%#codegen'
'% PMSDemandPolicyE：Baseline_Demand_Policy 发电需求律（Model E Phase 4-B, FR-432~435, T-002 链 A）'
'%   依据：.specs/phase4-b-pms-safety/design.md §5.3 + requirements FR-432/433/434/435（AC-417）；'
'%   brainstorm D-B3（Demand Policy 四数冻结）/ D-B4（P_commit 信号语义方案 b）/ D-B6 / D-B14。'
'%   角色：07_PMS_v2 内 Baseline source（ems_health_ok=0 时的确定性热备份，D-B7 ③）；'
'%   advanced EMS source 的 P_gen_req=P_EMS 直送不削减（FR-434），削减只发生在本 Baseline source 内部。'
'%'
'%   canonical 功率域（DD-1，genset-side generator-equivalent）：'
'%     - 全部输入/输出量均为 genset-side generator-equivalent；P_load_gen_kW 已在 07_PMS_v2'
'%       输入边界经 X_b2g_* 由 bus 侧 ÷ASSUMED_eta_rect 换算入域，本块零换算直用；'
'%     - PMS_P_chg_recovery_kW=100 kW 为 generator-equivalent recovery bias（≈bus 侧新增发电'
'%       约 97 kW），禁止表述为"电池 100 kW 实际充电"（DD-1 措辞纪律）。'
'%'
'%   输入：P_load_gen_kW 负荷（generator-equivalent）；SOC [-]；N_dispatch_fb dispatch 反馈'
'%     （在线机组数，**上一拍语义由调用方 UnitDelay 保证**——本函数直接消费当拍入参，'
'%     内部不再延迟，design §5.3）；PMS_LoadLPF_tau_s / PMS_SOC_lo / PMS_SOC_recovered /'
'%     PMS_P_chg_recovery_kW 为 FR-435 四参数（全 ENGINEERING_SURROGATE，数值事实源'
'%     config/ModelE_Control_Parameters.m，敏感性 τ∈{30,120} 冻结登记不扫）；DG_P_rated_kW'
'%     单机额定（genset 域字面，DD-1）。'
'%   输出：P_gen_req_baseline_kW = LPF(P_load) + P_recovery_actual（FR-432）；'
'%     P_commit_kW = LPF(P_load)（commitment 判据信号，不含 recovery 偏置，D-B4 方案 b）；'
'%     u_recovery SOC 滞回状态（供 logging，design §5.3）；P_recovery_actual_kW 实际恢复偏置。'
'%'
'%   离散约定：固定步 0.01 s，每调用一次 = 1 tick；dt=0.01 硬编码于 LPF 更新，'
'%   若模型固定步长变更须一并重推（同 GensetUnitE OQ-6 口径）。'
'%   纯 MATLAB：persistent 为唯一状态载体；无 toolbox 依赖；无 evalin/assignin/全局变量。'
''
'persistent P_load_filt u_rec'
'% 注：R2026a MATLAB Function 块解析纪律——persistent 变量不得直接作为输出参量，'
'% 故滞回状态本体为 u_rec，输出名 u_recovery 在每拍末拷贝输出（语义不变）。'
'if isempty(P_load_filt)'
'    P_load_filt = P_load_gen_kW;    % y[0]=x[0]（design §5.3；首拍 LPF 更新数值恒等）'
'    u_rec = false;                  % FR-433 初值 false'
'end'
''
'% (1) 一阶 LPF 前向欧拉（FR-432）：y[k]=y[k-1]+(Ts/τ)(x[k]-y[k-1])，τ=PMS_LoadLPF_tau_s'
'%     前向欧拉**非双线性**；标称 τ=60 s 时 Ts/τ≈1.67e-4，远在 0<Ts/τ<2 稳定界内、稳定裕量大'
'%     （禁称"无条件稳定"，design §5.3 措辞纪律）。'
'P_load_filt = P_load_filt + (0.01/PMS_LoadLPF_tau_s)*(P_load_gen_kW - P_load_filt);'
''
'% (2) SOC 滞回（FR-433，比较符号冻结；D-B3）：<=PMS_SOC_lo 进 / >=PMS_SOC_recovered 出 / 其间保持'
'if SOC <= PMS_SOC_lo'
'    u_rec = true;'
'elseif SOC >= PMS_SOC_recovered'
'    u_rec = false;'
'end'
'u_recovery = u_rec;                 % 输出本拍评估后的滞回状态（供 logging，design §5.3）'
''
'% (3) 恢复偏置（FR-434 non-firm/opportunistic）：负荷服务优先，削减只在 Baseline source 内部'
'%     （recovery 不得单独触发 DG2 启动——commitment 判据用 P_commit_kW，D-B4 结构性规则；'
'%       EMS 直送不削减，指令/功率 mismatch 经 dispatch 诊断量暴露，D-B6/D-B14）'
'P_recovery_actual_kW = double(u_rec) * max(0, min(PMS_P_chg_recovery_kW, ...'
'    DG_P_rated_kW*N_dispatch_fb - P_load_filt));'
''
'% (4) 基线发电需求 = LPF(P_load) + 实际恢复偏置（FR-432 / design §5.3）'
'P_gen_req_baseline_kW = P_load_filt + P_recovery_actual_kW;'
''
'% (5) commitment 判据信号 = LPF(P_load)，不含 recovery 偏置（D-B4 方案 b：'
'%     恢复充电为 opportunistic/non-firm 需求，不得影响启停门槛判定）'
'P_commit_kW = P_load_filt;'
'end'};
c = strjoin(lines, newline);
end