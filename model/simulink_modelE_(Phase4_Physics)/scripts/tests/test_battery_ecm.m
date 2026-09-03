% test_battery_ecm.m — T-008/T-009 链 C：电池 1RC ECM（BatteryECM block 源码）单元测试
% 被测对象 = +model/code_battery_ecm_e.m 返回的 block 源码字符串（落盘 tempdir 直调，
% 保证测的代码 = block 的代码）；SOP 内联一致性对照 +model/battery_sop_limits.m。
% 纯 MATLAB（不依赖 Simulink）；参数事实源 = config/ModelE_Physics_Parameters.m §6。
% 运行：matlab -batch "run('model/simulink_modelE_(Phase4_Physics)/scripts/tests/test_battery_ecm.m')"（工作区根目录）
ROOT = fileparts(fileparts(fileparts(mfilename('fullpath'))));  % scripts/tests/ → Model E 工作区根
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
run(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));   % 冻结参数（§6）进 base

td = tempdir;
src = model.code_battery_ecm_e();
fpath = fullfile(td, 'BatteryECM.m');
if exist(fpath, 'file'), delete(fpath); end
fid = fopen(fpath, 'w');
fwrite(fid, src);
fclose(fid);
addpath(td);
err_c = [];
try
    %%% 公共实参（按 BatteryECM 签名顺序；可被各用例覆盖后重组） %%%
    dt_s = 0.01;
    lut_real = {Bat_SOC_LUT_nodes, Bat_OCV_LUT_V, Bat_R0_LUT_ohm, Bat_R1_LUT_ohm, Bat_C1_LUT_F};
    tail_real = {Bat_R0_chg_mult, Bat_eta_coul, Bat_Q_nom_Ah, Bat_V_min_V, Bat_V_max_V, ...
        Bat_I_limit_surrogate_A, Bat_P_conv_bus_max_kW, ASSUMED_eta_dcdc, ...
        ASSUMED_SOC_min, ASSUMED_SOC_max, ASSUMED_SOC_DisDerate_Start, ASSUMED_SOC_ChgDerate_Start};

    % ================= T1 解析根手算锚点（OQ-9 二次式） =================
    % OCV=550, V1=0, R0=0.007, P_t=50000 W → E=550, D=301100, I≈91.0145 A；
    % 自洽：V_t*I ≈ 50000 W（±0.1%）。eta=1 使 P_t = P_bus*1000。
    lut_c1 = {[0.4 0.6], [550 550], [0.007 0.007], [0.009 0.009], [3000 3000]};  % 常值双节点 LUT（interp1 线性需>=2 点）
    tail_c1 = {1.15, 0.98, 1860, 360, 657, 1823, 1e9, 1.0, 0.10, 0.95, 0.25, 0.90};
    clear BatteryECM
    [Pb, SOC_out, Vb, I_m, dsoc, Pdis, Pchg, fl_pl, fl_dl, fl_lut] = ...
        BatteryECM(50, 0.5, dt_s, lut_c1{:}, tail_c1{:});
    I_exp = 2 * 50000 / (550 + sqrt(550^2 - 4 * 0.007 * 50000));   % = 91.01449 A
    assert(abs(I_m - I_exp) <= 1e-3 * I_exp, 'T1: 解析根偏差超 0.1%%: I=%.6f vs %.6f', I_m, I_exp);
    assert(abs(Vb * I_m - 50000) <= 1e-3 * 50000, 'T1: V_t*I 自洽失败: %.4f W', Vb * I_m);
    assert(abs(Pb - 50) <= 1e-9, 'T1: P_bat_bus 应为请求值 50 kW');
    assert(fl_pl == 0 && fl_dl == 0 && fl_lut == 0, 'T1: 不应置位任何 flag');
    dsoc_exp = -I_exp / (3600 * 1860);   % 放电电荷积分：-I/(3600*Q_nom)
    assert(abs(dsoc - dsoc_exp) <= 1e-9, 'T1: dSOC_dt 偏差: %.3e vs %.3e', dsoc, dsoc_exp);
    assert(SOC_out == 0.5, 'T1: SOC_out 应透传');
    assert(Pdis > 50 && Pchg > 0, 'T1: SOP 不应钳制 50 kW 请求');
    fprintf('T1 analytic-root anchor: PASS (I=%.5f A, Vt*I=%.2f W)\n', I_m, Vb * I_m);

    % ================= T2 persistent V_1 状态 =================
    % 续跑（不清 persistent）：第 1 步 I=91.0145 A 已将 V_1 推进 0.01*I/C1
    [Pb2, ~, Vb2] = BatteryECM(0, 0.5, dt_s, lut_c1{:}, tail_c1{:});
    V1_after = 0.01 * I_exp / 3000;
    assert(Vb2 < 550 && abs(Vb2 - (550 - V1_after)) <= 1e-9, 'T2: persistent V_1 未按 Euler 推进: Vb=%.9f', Vb2);
    assert(abs(Pb2) <= 1e-12, 'T2: 零请求应输出零功率');
    clear BatteryECM
    [~, ~, Vb3] = BatteryECM(0, 0.5, dt_s, lut_c1{:}, tail_c1{:});
    assert(abs(Vb3 - 550) <= 1e-9, 'T2: clear 后 persistent 应复位 V_1=0');
    fprintf('T2 persistent V_1 (Euler step + clear reset): PASS\n');

    % ================= T3 方向性：放电/充电 =================
    lut_c3 = {[0.4 0.6], [593 593], [0.00715 0.00715], [0.008 0.008], [2600 2600]};
    tail_c3 = {1.15, 0.98, 1860, 360, 657, 1823, 1e9, 0.97, 0.10, 0.95, 0.25, 0.90};
    clear BatteryECM
    [Pb, ~, Vb, I_m, dsoc, ~, ~, fl_pl, fl_dl, ~] = BatteryECM(500, 0.5, dt_s, lut_c3{:}, tail_c3{:});
    assert(I_m > 0 && dsoc < 0 && Vb < 593, 'T3: 放电方向性失败 (I=%.4f, dSOC=%.3e, Vt=%.4f)', I_m, dsoc, Vb);
    assert(abs(Pb - 500) <= 1e-9 && fl_pl == 0 && fl_dl == 0, 'T3: 放电 500 kW 应被完全满足');
    Pt_exp = 500 * 1000 / 0.97;
    assert(abs(Vb * I_m - Pt_exp) <= 1e-6 * Pt_exp, 'T3: 放电功率自洽失败 Vt*I=%.3f vs %.3f', Vb * I_m, Pt_exp);
    clear BatteryECM
    [Pb, ~, Vb, I_m, dsoc, ~, ~, fl_pl, fl_dl, ~] = BatteryECM(-400, 0.5, dt_s, lut_c3{:}, tail_c3{:});
    assert(I_m < 0 && dsoc > 0 && Vb > 593, 'T3: 充电方向性失败 (I=%.4f, dSOC=%.3e, Vt=%.4f)', I_m, dsoc, Vb);
    assert(abs(Pb + 400) <= 1e-9 && fl_pl == 0 && fl_dl == 0, 'T3: 充电 -400 kW 应被完全满足');
    Pt_exp = -400 * 1000 * 0.97;
    assert(abs(Vb * I_m - Pt_exp) <= 1e-6 * abs(Pt_exp), 'T3: 充电功率自洽失败');
    fprintf('T3 discharge/charge directionality: PASS\n');

    % ================= T4 eta_coul 不对称（同 |I| 一循环，比值≈0.98） =================
    % 同 |I*|=90 A：由 ECM 正向物理构造两侧请求功率（eta=1、R0_chg_mult=1 消去方向差异）
    lut_c4 = {[0.4 0.6], [550 550], [0.007 0.007], [0.009 0.009], [3000 3000]};
    tail_c4 = {1.0, 0.98, 1860, 360, 657, 1e6, 1e9, 1.0, 0.10, 0.95, 0.25, 0.90};
    I_star = 90;
    P_dis_req = I_star * (550 - 0.007 * I_star) / 1000;    % 49.4433 kW
    P_chg_req = -I_star * (550 + 0.007 * I_star) / 1000;   % -49.5567 kW
    clear BatteryECM
    [~, ~, ~, I_d, dsoc_d] = BatteryECM(P_dis_req, 0.5, dt_s, lut_c4{:}, tail_c4{:});
    clear BatteryECM
    [~, ~, ~, I_c, dsoc_c] = BatteryECM(P_chg_req, 0.5, dt_s, lut_c4{:}, tail_c4{:});
    assert(abs(I_d - I_star) <= 1e-6 * I_star && abs(I_c + I_star) <= 1e-6 * I_star, ...
        'T4: 同 |I| 注入失败 (I_dis=%.6f, I_chg=%.6f)', I_d, I_c);
    assert(abs(dsoc_c) < abs(dsoc_d), 'T4: |dSOC_chg| 应 < |dSOC_dis|');
    ratio = abs(dsoc_c) / abs(dsoc_d);
    assert(abs(ratio - 0.98) <= 1e-9, 'T4: eta_coul 比值应≈0.98, 实测 %.10f', ratio);
    fprintf('T4 coulomb-counting asymmetry: PASS (|dSOC_chg|/|dSOC_dis|=%.6f)\n', ratio);

    % ================= T5 充电方向 R_0×1.15（同功率 |I_chg| < |I_dis|） =================
    tail_c5 = {1.15, 0.98, 1860, 360, 657, 1e6, 1e9, 1.0, 0.10, 0.95, 0.25, 0.90};
    clear BatteryECM
    [~, ~, ~, I_dis] = BatteryECM(50, 0.5, dt_s, lut_c4{:}, tail_c5{:});
    clear BatteryECM
    [~, ~, ~, I_chg] = BatteryECM(-50, 0.5, dt_s, lut_c4{:}, tail_c5{:});
    assert(abs(I_chg) < abs(I_dis), 'T5: 充电方向 R_0×1.15 未生效: |I_chg|=%.6f vs |I_dis|=%.6f', abs(I_chg), abs(I_dis));
    fprintf('T5 direction-aware R_0 (x1.15): PASS (|I_chg|=%.4f < |I_dis|=%.4f A)\n', abs(I_chg), abs(I_dis));

    % ================= T6 LUT 边界（九节点冻结 LUT，域外 clamp + flag） =================
    clear BatteryECM
    [~, ~, Vb, ~, ~, ~, ~, ~, ~, fl_lut] = BatteryECM(0, 0.93, dt_s, lut_real{:}, tail_real{:});
    assert(fl_lut == 1, 'T6: SOC=0.93 应置 ecm_lut_boundary_flag=1');
    assert(abs(Vb - Bat_OCV_LUT_V(end)) <= 1e-9, 'T6: SOC=0.93 应取 0.9 节点 OCV=600.55 V, 实测 %.6f', Vb);
    clear BatteryECM
    [~, ~, Vb, ~, ~, ~, ~, ~, ~, fl_lut] = BatteryECM(0, 0.5, dt_s, lut_real{:}, tail_real{:});
    assert(fl_lut == 0, 'T6: SOC=0.5 不应置 LUT 边界 flag');
    assert(abs(Vb - Bat_OCV_LUT_V(5)) <= 1e-9, 'T6: SOC=0.5 OCV 应为 592.87 V, 实测 %.6f', Vb);
    clear BatteryECM
    [~, ~, Vb, ~, ~, ~, ~, ~, ~, fl_lut] = BatteryECM(0, 0.95, dt_s, lut_real{:}, tail_real{:});
    assert(fl_lut == 1 && abs(Vb - Bat_OCV_LUT_V(end)) <= 1e-9, 'T6: SOC=0.95（域顶）应 clamp+flag');
    fprintf('T6 LUT boundary clamp + flag: PASS (0.93/0.95 -> flag=1, OCV=%.2f V)\n', Bat_OCV_LUT_V(end));

    % ================= T7 判别式边界 + D<0 power-limit 语义 =================
    % (a) 恰在边界（fp 精确构造：R0=0.03125、E=500、eta=1 → P_t = E^2/(4R0) = 2e6 W,
    %     D = 250000 - 0.125*2e6 = 0 精确为零）→ 不置 flag
    lut_c7 = {[0.4 0.6], [500 500], [0.03125 0.03125], [0.01 0.01], [3000 3000]};
    tail_c7a = {1.15, 0.98, 1860, 250, 657, 1e6, 1e9, 1.0, 0.10, 0.95, 0.25, 0.90};
    clear BatteryECM
    [Pb, ~, Vb, I_m, ~, Pdis, ~, fl_pl, fl_dl, ~] = BatteryECM(2000, 0.5, dt_s, lut_c7{:}, tail_c7a{:});
    assert(fl_pl == 0 && fl_dl == 0, 'T7a: 判别式恰为零不应置 flag');
    assert(abs(Pdis - 2000) <= 1e-9, 'T7a: P_voltage 通道恰在 P_max_R (2000 kW)');
    assert(abs(Pb - 2000) <= 1e-9 && abs(I_m - 8000) <= 1e-9 && abs(Vb - 250) <= 1e-9, ...
        'T7a: 边界点应为精确物理根 (P=%.6f, I=%.6f, Vt=%.6f)', Pb, I_m, Vb);
    % (b) 请求略超 P_max_R（eta=0.97：SOP P_voltage=2061.86 kW > P_max_R_bus=1940 kW，
    %     请求 2000 kW 过 SOP 不被钳、进 ECM 后 D<0）→ P_actual=P_max_R，双 flag=1
    tail_c7b = tail_c7a; tail_c7b{8} = 0.97;
    clear BatteryECM
    [Pb, ~, Vb, I_m, ~, Pdis, ~, fl_pl, fl_dl, ~] = BatteryECM(2000, 0.5, dt_s, lut_c7{:}, tail_c7b{:});
    P_max_R_bus = 500^2 * 0.97 / (4 * 0.03125 * 1000);   % = 1940 kW
    assert(fl_pl == 1 && fl_dl == 1, 'T7b: D<0 应置双 flag');
    assert(abs(Pb - P_max_R_bus) <= 1e-9, 'T7b: P_bat_bus 应压到 P_max_R*eta=1940 kW, 实测 %.6f', Pb);
    assert(abs(I_m - 8000) <= 1e-9 && abs(Vb - 250) <= 1e-9, 'T7b: 判别式零点电流 I=E/(2R0)=8000 A');
    assert(all(isfinite([Pb, Vb, I_m])), 'T7b: 不得产生 NaN/Inf');
    % (c) 请求略低于 P_max_R → 正常物理根，不置 flag
    clear BatteryECM
    [Pb, ~, ~, I_m, ~, ~, ~, fl_pl, fl_dl, ~] = BatteryECM(1930, 0.5, dt_s, lut_c7{:}, tail_c7b{:});
    assert(fl_pl == 0 && fl_dl == 0 && abs(Pb - 1930) <= 1e-9, 'T7c: P_max_R 以下请求应被完全满足');
    assert(I_m > 0 && I_m < 8000, 'T7c: 小电流物理根应 < 判别式零点电流');
    fprintf('T7 discriminant boundary + power-limit semantics: PASS (P_actual=1940 kW, dual flag)\n');

    % ================= T8 SOP 内联一致性（block 内联 == 纯函数） =================
    soc_chk = [0.2, 0.5, 0.93];
    for k = 1:numel(soc_chk)
        s_k = soc_chk(k);
        clear BatteryECM
        [~, ~, ~, ~, ~, Pdis_b, Pchg_b] = BatteryECM(0, s_k, dt_s, lut_real{:}, tail_real{:});
        ocv_k = interp1(Bat_SOC_LUT_nodes, Bat_OCV_LUT_V, min(max(s_k, 0.1), 0.9), 'linear');
        r0_k = interp1(Bat_SOC_LUT_nodes, Bat_R0_LUT_ohm, min(max(s_k, 0.1), 0.9), 'linear');
        [Pd_ref, Pc_ref] = model.battery_sop_limits(s_k, ocv_k, 0, r0_k, r0_k * Bat_R0_chg_mult, ...
            Bat_I_limit_surrogate_A, Bat_V_min_V, Bat_V_max_V, ASSUMED_eta_dcdc, ...
            Bat_P_conv_bus_max_kW, ASSUMED_SOC_min, ASSUMED_SOC_max, ...
            ASSUMED_SOC_DisDerate_Start, ASSUMED_SOC_ChgDerate_Start);
        assert(abs(Pdis_b - Pd_ref) <= 1e-9 * max(1, Pd_ref), 'T8: SOC=%.2f P_dis 内联/纯函数不一致 (%.9g vs %.9g)', s_k, Pdis_b, Pd_ref);
        assert(abs(Pchg_b - Pc_ref) <= 1e-9 * max(1, Pc_ref), 'T8: SOC=%.2f P_chg 内联/纯函数不一致', s_k);
    end
    fprintf('T8 inline-vs-function SOP consistency: PASS\n');

    % ================= T9 零 NaN/Inf 扫描（全组合 isfinite） =================
    soc_scan = [0.10, 0.5, 0.95];
    p_scan = [-1050, 0, 1050];
    n_fin = 0;
    for a = 1:numel(soc_scan)
        for b = 1:numel(p_scan)
            clear BatteryECM
            oc = cell(1, 10);
            [oc{:}] = BatteryECM(p_scan(b), soc_scan(a), dt_s, lut_real{:}, tail_real{:});
            o = [oc{:}];   % 10 个标量输出合成向量做 isfinite 全扫描
            assert(all(isfinite(o)), 'T9: SOC=%.2f P=%.0f 出现非有限值', soc_scan(a), p_scan(b));
            assert(o(6) <= Bat_P_conv_bus_max_kW + 1e-9 && o(7) <= Bat_P_conv_bus_max_kW + 1e-9, ...
                'T9: SOP 可用功率不得超过 1050 kW bus-side');
            n_fin = n_fin + 1;
        end
    end
    fprintf('T9 zero-NaN scan: PASS (%d/9 combos finite)\n', n_fin);
catch err_c
end
% ---- 清理临时 block 源码与路径 ----
if exist(fpath, 'file'), delete(fpath); end
warning('off', 'MATLAB:rmpath:NotFound');
rmpath(td);
warning('on', 'MATLAB:rmpath:NotFound');
if ~isempty(err_c), rethrow(err_c); end
fprintf('T008-BATTERY-ECM: ALL PASS\n');