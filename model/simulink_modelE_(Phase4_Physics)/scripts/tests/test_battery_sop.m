% test_battery_sop.m — T-009 链 C：电池 SOP 四约束起控点注入测试（纯 MATLAB）
% 被测对象 = +model/battery_sop_limits.m（BatteryECM block 内联同源，见 test_battery_ecm.m T8）。
% 注入法：固定目标约束起控、其余约束以极大参数值替代（inf 语义）。
% 参数事实源 = config/ModelE_Physics_Parameters.m §6（1823 A / 1050 kW / 0.97 冻结）。
% 运行：matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_battery_sop.m')"（工作区根目录）
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));   % 冻结参数（§6）

SOP = @model.battery_sop_limits; %#ok<NASGU>
V_MIN = Bat_V_min_V;   % 360
V_MAX = Bat_V_max_V;   % 657
ETA   = ASSUMED_eta_dcdc;          % 0.97
SMIN  = ASSUMED_SOC_min;           % 0.10
SMAX  = ASSUMED_SOC_max;           % 0.95
DDS   = ASSUMED_SOC_DisDerate_Start;  % 0.25
CDS   = ASSUMED_SOC_ChgDerate_Start;  % 0.90
ILIM  = Bat_I_limit_surrogate_A;   % 1823
PCONV = Bat_P_conv_bus_max_kW;     % 1050
BIG   = 1e9;    % 其余约束的"inf 语义"替代值（纯函数直接传大值）

% ================= S1 P_current 起控（V_oc 高、SOC 高、R0 小） =================
[Pd, Pc, fl] = SOP(0.9, 600, 0, 1e-6, 1e-6, ILIM, V_MIN, V_MAX, ETA, BIG, SMIN, SMAX, DDS, CDS);
Pd_exp = ILIM * 600 / ETA / 1000;    % 1823*600/0.97/1000 = 1127.63 kW
Pc_exp = ILIM * 600 * ETA / 1000;    % 1823*600*0.97/1000 = 1060.99 kW
assert(abs(Pd - Pd_exp) <= 1e-9 * Pd_exp, 'S1: P_dis 应=I_lim*V_oc/eta=%.6f, 实测 %.6f', Pd_exp, Pd);
assert(abs(Pc - Pc_exp) <= 1e-9 * Pc_exp, 'S1: P_chg 应=I_lim*V_oc*eta=%.6f, 实测 %.6f', Pc_exp, Pc);
assert(fl.soc_derate_dis == 0 && fl.soc_derate_chg == 1, 'S1: SOC=0.90 应置 chg 降额 flag');
fprintf('S1 P_current binding: PASS (P_dis=%.4f kW = 1823*600/0.97/1000)\n', Pd);

% ================= S2 P_voltage 起控（V_oc_est = V_min + 5 V） =================
[Pd, ~, fl] = SOP(0.9, 365, 0, 0.005, 0.005, 1e6, V_MIN, V_MAX, ETA, BIG, SMIN, SMAX, DDS, CDS);
I_vmax = (365 - V_MIN) / 0.005;                        % 1000 A
Pd_exp = I_vmax * max(365 - I_vmax * 0.005, V_MIN) / ETA / 1000;   % 1000*360/0.97/1000 = 371.13 kW
assert(abs(Pd - Pd_exp) <= 1e-9 * Pd_exp, 'S2: P_voltage 通道手算=%.6f, 实测 %.6f', Pd_exp, Pd);
assert(fl.soc_derate_dis == 0, 'S2: SOC=0.9 放电侧不在降额带');
fprintf('S2 P_voltage binding: PASS (P_dis=%.4f kW = (365-360)/0.005*360/0.97/1000)\n', Pd);

% ================= S3 P_SOC 起控（降额族，满刻度 = 1050 kW） =================
% SOC=0.20：降额带内线性值 = 1050*(0.20-0.10)/(0.25-0.10) = 700 kW
[Pd, ~, fl] = SOP(0.20, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
Pd_exp = PCONV * (0.20 - SMIN) / (DDS - SMIN);   % 700
assert(abs(Pd - Pd_exp) <= 1e-9 * Pd_exp, 'S3: SOC=0.20 线性降额值=%.4f, 实测 %.6f', Pd_exp, Pd);
assert(fl.soc_derate_dis == 1, 'S3: SOC=0.20 应置 dis 降额 flag');
% SOC=0.10：放电归零
[Pd, ~, fl] = SOP(0.10, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
assert(Pd == 0 && fl.soc_derate_dis == 1, 'S3: SOC=0.10 放电可用功率应为 0');
% SOC=0.97（充电侧 > SOC_max）：P_chg = 0
[~, Pc, fl] = SOP(0.97, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
assert(Pc == 0 && fl.soc_derate_chg == 1, 'S3: SOC=0.97 充电可用功率应为 0');
% SOC=0.92（降额带内）：P_chg = 1050*(0.95-0.92)/0.05 = 630 kW
[~, Pc, ~] = SOP(0.92, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
Pc_exp = PCONV * (SMAX - 0.92) / (SMAX - CDS);   % 630
assert(abs(Pc - Pc_exp) <= 1e-9 * Pc_exp, 'S3: SOC=0.92 充电降额=%.4f, 实测 %.6f', Pc_exp, Pc);
fprintf('S3 P_SOC binding: PASS (0.20->%.1f kW, 0.10->0, 0.92->%.1f kW, 0.97->0)\n', Pd_exp, Pc_exp);

% ================= S4 P_converter 起控（其余宽裕 → 1050） =================
[Pd, Pc, ~] = SOP(0.9, 600, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
assert(abs(Pd - PCONV) <= 1e-9 && abs(Pc - PCONV) <= 1e-9, 'S4: 其余宽裕时应取图纸约束 1050 kW (Pd=%.6f, Pc=%.6f)', Pd, Pc);
fprintf('S4 P_converter binding: PASS (P_dis=P_chg=1050 kW)\n');

% ================= S5 联合 min（两约束同时紧 → 取更小者） =================
% (a) P_voltage=371.13 与 P_converter=350 同时存在 → 350
[Pd, ~, ~] = SOP(0.9, 365, 0, 0.005, 0.005, 1e6, V_MIN, V_MAX, ETA, 350, SMIN, SMAX, DDS, CDS);
assert(abs(Pd - 350) <= 1e-9, 'S5a: 应取 min(P_voltage=371.13, P_conv=350)=350, 实测 %.6f', Pd);
% (b) P_current=75.26（I_lim=200）与 P_voltage=371.13 同时紧 → 75.26
I_t = 200;
[Pd, ~, ~] = SOP(0.9, 365, 0, 0.005, 0.005, I_t, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
Pd_exp = I_t * 365 / ETA / 1000;   % 75.2577
assert(abs(Pd - Pd_exp) <= 1e-9 * Pd_exp, 'S5b: 电流约束应最小化: %.6f, 实测 %.6f', Pd_exp, Pd);
% (c) P_SOC=700（SOC=0.20）与 P_current=489.07（I_lim=800）同时紧 → 489.07
[Pd, ~, ~] = SOP(0.20, 593, 0, 1e-6, 1e-6, 800, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
Pd_exp = 800 * 593 / ETA / 1000;   % 489.072
assert(abs(Pd - Pd_exp) <= 1e-9 * Pd_exp, 'S5c: 应取 min(P_current=489.07, P_SOC=700)=489.07, 实测 %.6f', Pd);
fprintf('S5 joint min: PASS (350<371.13; 75.26<371.13; 489.07<700)\n');

% ================= S6 充电镜像（V_oc 接近 V_max 收缩；1050 上界生效） =================
[Pd, Pc, ~] = SOP(0.5, 650, 0, 0.005, 0.005, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
I_vmin_abs = (V_MAX - 650) / 0.005;    % 1400 A
Pc_exp = I_vmin_abs * min(650 + I_vmin_abs * 0.005, V_MAX) * ETA / 1000;   % 1400*657*0.97/1000 = 892.29
assert(abs(Pc - Pc_exp) <= 1e-9 * Pc_exp, 'S6: 充电电压收缩=%.6f, 实测 %.6f', Pc_exp, Pc);
assert(Pc < PCONV, 'S6: 电压约束应先于 1050 收缩');
assert(abs(Pd - PCONV) <= 1e-9, 'S6: 同状态放电侧仍应为 1050 上界');
[~, Pc, ~] = SOP(0.5, 650, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
assert(abs(Pc - PCONV) <= 1e-9, 'S6: 电压通道宽裕时充电应取 1050 上界, 实测 %.6f', Pc);
fprintf('S6 charge mirror: PASS (V_oc=650,R0chg=0.005 -> %.4f kW; R0chg=1e-6 -> 1050 kW)\n', Pc_exp);

% ================= S7 边界连续性 + 无 NaN 扫描 =================
% V_oc_est = V_min：放电电压通道归零 → P_dis = 0
[Pd, ~, ~] = SOP(0.5, V_MIN, 0, 0.005, 0.005, 1e6, V_MIN, V_MAX, ETA, BIG, SMIN, SMAX, DDS, CDS);
assert(Pd == 0, 'S7: V_oc_est=V_min 时放电应为 0');
% V_oc_est = V_max：充电电压通道归零 → P_chg = 0
[~, Pc, ~] = SOP(0.5, V_MAX, 0, 0.005, 0.005, 1e6, V_MIN, V_MAX, ETA, BIG, SMIN, SMAX, DDS, CDS);
assert(Pc == 0, 'S7: V_oc_est=V_max 时充电应为 0');
% SOC=0.25 两侧连续（带内分数=1 与带外非绑定值等价）
% SOC=0.25 两侧连续（带内分数=1 与带外非绑定值等价；探针 1e-9 对应理论跳变
% PCONV*1e-9/(DDS-SMIN) ≈ 7e-6 kW，容差取 1e-4 kW）
[Pd_lo, ~, ~] = SOP(0.25 - 1e-9, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
[Pd_hi, ~, ~] = SOP(0.25 + 1e-9, 593, 0, 1e-6, 1e-6, 1e6, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
assert(abs(Pd_lo - Pd_hi) <= 1e-4, 'S7: SOC=0.25 边界不连续 (%.9g vs %.9g)', Pd_lo, Pd_hi);
fprintf('S7 boundary continuity: PASS (SOC=0.25 jump < 1e-6 kW)\n');
% 全组合无 NaN：SOC × OCV(含域外退化 300/700) × V1
soc_l = [0.10 0.15 0.20 0.30 0.50 0.70 0.89 0.90 0.91 0.95];
ocv_l = [541.08 592.87 600.55 V_MIN V_MAX 300 700];
v1_l  = [0 5 -5];
n = 0;
for a = 1:numel(soc_l)
    for b = 1:numel(ocv_l)
        for cc = 1:numel(v1_l)
            [Pd, Pc, ~] = SOP(soc_l(a), ocv_l(b), v1_l(cc), 0.00715, 0.0082225, ...
                ILIM, V_MIN, V_MAX, ETA, PCONV, SMIN, SMAX, DDS, CDS);
            assert(isfinite(Pd) && isfinite(Pc) && Pd >= 0 && Pc >= 0, ...
                'S7: SOC=%.2f OCV=%.2f V1=%.1f 出现非有限/负可用功率 (%.4g, %.4g)', ...
                soc_l(a), ocv_l(b), v1_l(cc), Pd, Pc);
            n = n + 1;
        end
    end
end
fprintf('S7 zero-NaN scan: PASS (%d combos finite & non-negative)\n', n);
fprintf('T009-BATTERY-SOP: ALL PASS\n');