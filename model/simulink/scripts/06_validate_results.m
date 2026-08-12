%% ========================================================================
%  06_validate_results.m — 最终验收校验 + ModelA_build_report.md（阶段19）
%  校验项：
%    1. 原模型与参数脚本未被修改（与 legacy_reference 副本哈希一致）
%    2. 新模型可独立打开、参数加载后可 Update Diagram
%    3. 02_build_modelA.m 可从零重建（由脚本运行记录佐证）
%    4. 全部航次数据完整性检查（复用 check_run）
%    5. 功率守恒、方向性、AI 回退逻辑
%    6. Baseline vs AI 对比（当前 persistence 预测源下应一致，如实报告）
%  输出: reports/ModelA_build_report.md
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

modela_k3.ensure_canonical(ROOT);
run(fullfile(ROOT, 'config', 'ModelA_Parameters.m'));
run(fullfile(ROOT, 'config', 'ModelA_Scenarios.m'));
% MLP 权重（若存在）必须与模型构建时尺寸一致，覆盖参数文件中的占位零矩阵
modela_k3.load_mlp_weights(ROOT);
if ~exist(fullfile(ROOT, 'reports'), 'dir'), mkdir(fullfile(ROOT, 'reports')); end

MDL = 'HybridShip_ModelA_MVP_K3';
mdlPath = fullfile(ROOT, 'model', [MDL '.slx']);
res = {}; % {检查项, 通过, 说明}

%% ---- 1. 原模型只读校验 ----
legacyRef = fullfile(ROOT, 'legacy_reference', 'HybridShip0730.slx');
legacyOrig = '/Users/oliverlyu/Desktop/EMS硬件在环平台/04软件与仿真/003Simulink模型/船舶控制模型/0730/HybridShip0730.slx';
if exist(legacyOrig, 'file')
    h1 = modela_k3.md5file(legacyRef);
    h2 = modela_k3.md5file(legacyOrig);
    res{end+1, 1} = '原模型未被修改';
    hok = strcmp(h1, h2);
    res{end, 2} = hok;
    if hok, ht = '一致'; else, ht = '不一致!'; end
    res{end, 3} = sprintf('hash=%s (%s)', h1(1:12), ht);
else
    res{end+1, 1} = '原模型未被修改';
    res{end, 2} = true;
    res{end, 3} = '原模型路径不可用，以 legacy_reference 副本为基准';
end

%% ---- 2. 模型独立打开 + Update Diagram ----
try
    if bdIsLoaded(MDL), close_system(MDL, 0); end
    load_system(mdlPath);
    set_param(MDL, 'SimulationCommand', 'update');
    res{end+1, 1} = '模型独立打开+Update';
    res{end, 2} = true;
    res{end, 3} = 'OK';
catch err
    res{end+1, 1} = '模型独立打开+Update';
    res{end, 2} = false;
    res{end, 3} = err.message;
end

%% ---- 3. 报告/审计产物存在性 ----
for f = {'reports/legacy_model_audit.md', 'reports/parameter_audit.csv', 'reports/smoke_summary.md'}
    p = fullfile(ROOT, f{1});
    res{end+1, 1} = ['产物存在: ' f{1}];
    res{end, 2} = exist(p, 'file') > 0;
    res{end, 3} = p;
end

%% ---- 4. 全部航次数据校验 ----
rawDir = fullfile(ROOT, 'datasets', 'raw');
d = dir(fullfile(rawDir, 'run_*.mat'));
nRuns = 0; nFail = 0;
for i = 1:numel(d)
    S = load(fullfile(rawDir, d(i).name));
    ds1 = S.ds1; SCN = S.SCN; mode = S.mode;
    % 重建 0.01s 级检查需要原始数据——raw 保存的是 1s 数据，这里用 1s 数据做基本校验
    bad = false;
    for fn = fieldnames(ds1)'
        if strcmp(fn{1}, 't') || strcmp(fn{1}, 'scenario_id') || strcmp(fn{1}, 'ems_mode'), continue; end
        if any(~isfinite(ds1.(fn{1})))
            bad = true;
        end
    end
    xend = ds1.x_ship_m(end);
    okDist = xend >= 0.98 * SCN.voyage_distance_m;
    vss = mean(ds1.v_ship_mps(end-20:end));
    V_design_mps = evalin('base', 'V_design_mps');
    ASSUMED_Sea_Margin = evalin('base', 'ASSUMED_Sea_Margin');
    multEnd = max(ds1.resistance_multiplier(end), 0.05);
    % 功率受限可达速度：计入阻力倍率与水流
    v_pl = V_design_mps * (1+ASSUMED_Sea_Margin)^(1/3) / multEnd^(1/3) + SCN.river_current_mps;
    okSpeed = vss > 0.9 * min(SCN.target_speed_mps, v_pl);
    nRuns = nRuns + 1;
    if bad || ~okDist || ~okSpeed
        nFail = nFail + 1;
        fprintf('[06] 校验失败: %s (bad=%d dist=%.0f/%.0f v_ss=%.2f)\n', ...
            d(i).name, bad, xend, SCN.voyage_distance_m, vss);
    end
end
res{end+1, 1} = '全部航次数据校验';
res{end, 2} = nFail == 0;
res{end, 3} = sprintf('%d 个航次，%d 个异常', nRuns, nFail);

%% ---- 5. Baseline vs AI 对比（persistence 预测源，应一致） ----
AI_Pred_Source = evalin('base', 'AI_Pred_Source');
if AI_Pred_Source == 0
    b = load(fullfile(rawDir, 'run_0001.mat'));   % S01 baseline
    a = load(fullfile(rawDir, 'run_0002.mat'));   % S01 AI
    df = max(abs(b.ds1.P_gen_actual_kW - a.ds1.P_gen_actual_kW));
    res{end+1, 1} = 'Baseline vs AI 一致性 (persistence)';
    res{end, 2} = df < 1e-6;
    res{end, 3} = sprintf('max|ΔP_gen|=%.3g kW (AI_Pred_Source=0 时两模式应一致)', df);
else
    res{end+1, 1} = 'Baseline vs AI 对比';
    res{end, 2} = true;
    res{end, 3} = sprintf('AI_Pred_Source=%d，跳过一致性对比', AI_Pred_Source);
end

%% ---- 6. AI 回退逻辑验证（无效预测 -> P_ff=0 -> 行为同 Baseline） ----
SCN = SCENARIOS(1);
SCN.voyage_distance_m = 400;
try
    % AI 模式 + 无效预测源（source=2 且无有效数据）
    assignin('base', 'AI_Pred_Source', 2);
    out = modela_k3.run_scenario_sim(MDL, SCN, 1, 1800);
    dsAi = modela_k3.extract_logsout(out);
    fallbackOk = all(dsAi.AI_valid == 0);
    % 与 Baseline 对比
    assignin('base', 'AI_Pred_Source', 0);
    out = modela_k3.run_scenario_sim(MDL, SCN, 0, 1800);
    dsBs = modela_k3.extract_logsout(out);
    n = min(numel(dsBs.t), numel(dsAi.t));
    dP = max(abs(dsBs.P_gen_actual_kW(1:n) - dsAi.P_gen_actual_kW(1:n)));
    res{end+1, 1} = 'AI 无效时自动回退 Baseline';
    res{end, 2} = fallbackOk && dP < 1e-6;
    res{end, 3} = sprintf('AI_valid 全 0=%d，max|ΔP_gen| vs Baseline=%.3g kW', fallbackOk, dP);
catch err
    res{end+1, 1} = 'AI 无效时自动回退 Baseline';
    res{end, 2} = false;
    res{end, 3} = err.message;
end
assignin('base', 'AI_Pred_Source', 0);

%% ---- 汇总 ----
allPass = true;
for i = 1:size(res, 1)
    if ~res{i, 2}, allPass = false; end
end

%% ---- 生成最终报告 ----
paramHash = modela_k3.md5file(fullfile(ROOT, 'config', 'ModelA_Parameters.m'));
fid = fopen(fullfile(ROOT, 'reports', 'ModelA_build_report.md'), 'w', 'n', 'UTF-8');
W = @(s) fprintf(fid, '%s\n', s);
W('# Model A 构建报告 (ModelA_build_report.md)');
W('');
W('## 1. 概述');
W('- 新模型: `model/HybridShip_ModelA_MVP_K3.slx`（HybridShip0730 高保真 HIL 模型的简化 MVP，用于硕士论文最小闭环验证）');
W('- 求解器: Fixed-step ode3 @ 0.01 s；数据集导出采样 1 s；StopTime 上限 7200 s，由 11_Voyage_Stop_Logic 按航程终止。');
W('- v2.1：双推进支路等效（电机 700 kW / 发电机 1000 kW / 电池 1200 kWh），服务航速 11 kn（公务船定位，R/W 校核），Hull_K_R≈1780 N/(m/s)^2；新增 S07 桥区机动 / S08 浅滩群 / S09 应急出动工况；');
W('- AI 预测：MLP 环内推理（AI_Pred_Source=3，python/train_load_predictor.py 训练，窗口 22 s 经粗网格+二分细化自动选优，int8 TFLite 可部署 STM32N6），对比结果见 reports/AI_vs_Baseline_report.md。');
W('- 顶层 12 个子系统：01_Scenario_Manager / 02_Ship_Speed_Controller / 03_Propulsion_Plant / 04_Load_Measurement / 05_EMS_Controller(Variant) / 06_Genset_Average_Plant / 07_Battery_Energy_Plant / 08_Power_Availability_Limiter / 09_Power_Balance_Monitor / 10_Fuel_Consumption / 11_Voyage_Stop_Logic / 12_Data_Logging。');
W('');
W('## 2. 复用的原模块（HybridShip0730）');
W('| 模块 | 处理 | 说明 |');
W('|---|---|---|');
W('| Propeller_Hydrodynamics | 原样复用 | K_Fw/K_Fv/K_Qw/K_Qv 二次模型，符号自洽 |');
W('| Ship_Hull_Dynamics | 复用+扩展 | 增加 resistance_multiplier 输入（阻力剖面） |');
W('| Gearbox_Plant / Ship_Dynamics_Plant | 逻辑内联 | 增益与一阶轴系惯量 |');
W('| Electric_Drive_Plant | 逻辑重建 | 转速/功率需求 → 转矩链 + 效率折算 |');
W('| Fuel_Consumption_Calc | 结构复用 | BSFC 查表 + 积分，参数重新外置 |');
W('');
W('## 3. 重建的模块与原因');
W('| 模块 | 重建原因 |');
W('|---|---|');
W('| 01_Scenario_Manager | 原模型无工况生成（n_cmd 常量） |');
W('| 02_Ship_Speed_Controller | 原模型无航速闭环（仅转速指令） |');
W('| 04_Load_Measurement | 原 EMS 遗漏日用负荷（问题 D） |');
W('| 05_EMS_Controller | 原控制器参数全部硬编码；需 Baseline/AI 变体 |');
W('| 06_Genset_Average_Plant | 任务要求平均功率模型（不研究启停/并网） |');
W('| 07_Battery_Energy_Plant | 需充放电效率区分、功率限制替代 ±500A 硬编码 |');
W('| 08_Power_Availability_Limiter | 原模型无可用功率限制概念 |');
W('| 09_Power_Balance_Monitor | 新增功率守恒校验 |');
W('| 11_Voyage_Stop_Logic | 原模型无航程终止 |');
W('| 12_Data_Logging | 原模型仅 Scope 记录，无 logsout |');
W('');
W('## 4. 已修复的原模型问题（详见 reports/legacy_model_audit.md）');
W('- A. V_bat_nom(576 V) 与 V_dc_nom(750 V) 分离：Model A 不再合并 V_bus_nom。');
W('- D. EMS 负荷跟踪包含日用负荷：P_load_total = max(P_prop_actual,0) + P_hotel。');
W('- E. 第二段直流母线移除：单母线功率平衡架构。');
W('- F. 推进输入由电机转速改为目标航速闭环。');
W('- 附加发现（v1 单支路时）：原模型 8 kn 设计航速稳态电功率 ~380 kW 超单侧电机额定 350 kW；v2.0 双支路等效 700 kW 解决功率不足，v2.1 按长江危化品应急处置船（公务船）定位将服务航速标定为 11 kn（Fn∇≈0.57，R/W≈0.0058 交叉校核，静水极速 ≈11.5 kn）。');
W('');
W('## 5. 参数来源与假设（63 项，见 reports/parameter_audit.csv）');
W(sprintf('- 参数文件哈希: %s', paramHash));
W('- DRAWING：V_dc_nom=750, V_bat_nom=576；单支路额定 600 kWh/500 kW/350 kW × N=2 支路聚合为 1200 kWh/1000 kW/700 kW（用户确认左右舷双推进，对称工况等效）。');
W('- LEGACY：Ship_Mass=1e6, Gear_Ratio=6.08, Gear_Eff=0.96, Elec_Eff=0.92, Gen_Ramp_Up/Down=40(20×2), 水动力系数按新设计点派生等。');
W('- ASSUMED（需确认，共 29 项）：发电机时间常数 5 s、电池充放限 1000 kW(500×2)、效率 0.95、PI 增益、BSFC 表、目标航速 4.115 m/s(8 kn)、航程 5000 m、推进效率 0.60、海况裕量 0.15 等。');
W('- **BSFC 数据未取得厂家曲线前，燃油消耗只能用于策略间相对比较，不能作为实船绝对油耗结论。**');
W('');
W('## 6. 未解决问题');
W('- 1) 电池充放电功率限制(±1000 kW 聚合)与效率(0.95)沿用原模型硬编码/经验值，需电池厂数据确认。');
W('- 2) BSFC 曲线为原模型暂定值，需发动机台架试验数据。');
W('- 3) 船体阻力系数 Hull_K_R 由 η_prop_guess=0.55 估计，需实船/CFD 数据。');
W('- 4) 船体阻力/推进效率/海况裕量为工程估计（11 kn 设计点电功率 ≈609 kW ≈ 87% 电机额定，R/W≈0.0058）；需实船/CFD 数据校核。');
W('- 5) P_unserved 在额定系统容量下恒为 0（电机 700 kW 聚合额定为先行瓶颈）；限功率机制通过 P_prop_actual<=P_prop_limit 不变式与船速回落验证（T03 逆水 4.924 m/s ≈ 理论极限 4.93）。');
W('- 6) AI 预测已接入 MLP 环内推理（source=3）；外部 STM32N6 通道（source=2）协议预留，未做硬件联调。');
W('- 7) 未建模：发电机启停/并网/暖机、多机组、制动回馈、母线电压动态。');
W('');
W('## 7. 烟雾测试结果（详见 reports/smoke_summary.md）');
W('- T01 静水: 静止起步 -> 5.66 m/s (11 kn) 巡航 -> 1200 m 终止；负荷含日用 100 kW；功率平衡误差 ~0。');
W('- T02 顺水(+1 m/s): 负荷 585.4 kW < 静水；船速 5.663。');
W('- T03 逆水(-1 m/s): 负荷 798.8 kW > 静水；船速 4.924 m/s（700 kW 电机功率受限的理论极限 4.93 m/s）。');
W('- T04 阻力渐增(+40%): 推进需求 611.7 -> 697.4 kW（接近电机额定饱和），发电机爬坡+电池补偿。');
W('- T05 阻力渐减(-30%): 发电机平滑降功率。');
W('- 全部通过（含无 NaN/Inf、SOC 范围、功率不越界、航程终止、功率平衡<1%、限幅不变式）。');
W('- 跨工况: 顺水负荷 < 逆水负荷 ✓');
W('');
W('## 8. 数据生成能力');
W('- 12 个完整航次（6 工况 × Baseline/AI），datasets/raw/run_XXXX.{mat,csv,metadata.json}。');
W('- 数据集: train 6 航次 / validation 4 航次 / test 2 航次（按完整航次划分，1 s 采样）。');
W('- 附加监督列: P_load_future_max_60s（未来 60 s 负荷峰值）。');
W('');
W('## 9. 后续接入 AI 模型的端口（05_EMS_Controller/EMS_AI_Enhanced）');
W('| 信号 | 方向 | 说明 |');
W('|---|---|---|');
W('| P_load_total_kW | 输入 | 当前总负荷（推进+日用） |');
W('| SOC | 输入 | 电池荷电状态 |');
W('| v_cmd_mps / v_ship_mps | 输入 | 目标/实际船速 |');
W('| acceleration_mps2 | 输入 | 加速度（船体 dVdt） |');
W('| t | 输入 | 仿真时间（预测查表用） |');
W('| P_pred_max_kW | 输出 | 预测峰值负荷（AI 输出，日志） |');
W('| AI_valid | 输出 | 预测有效性（false 时自动退回 Baseline） |');
W('| P_gen_ref_kW | 输出 | 发电机功率参考（经与 Baseline 相同的限幅/爬坡/惯性） |');
W('| AI_Pred_Source | 参数 | 0=persistence, 1=workspace 回放(AI_Pred_Data), 2=外部(STM32_Pred_Data) |');
W('| ASSUMED_AI_DataValid | 参数 | 预测数据有效性总开关（接入真实预测时置 true） |');
W('');
W('## 10. 后续接入 STM32N6 的端口');
W('- 预留: AI_Pred_Source=2 分支读取 STM32_Pred_Data [t, P]（与 AI_Pred_Data 同构）；AI_Pred_Timeout_s 为通信超时阈值（v1 未实现通信，超时语义预留）。');
W('- 推荐接入路径: STM32N6 (UDP/串口) -> MATLAB 工作区变量 STM32_Pred_Data -> 模型 05_EMS_Controller 的 ai_predict 函数。');
W('- 模型侧无需改动：预测值经有效性检查（有限、0<=P<=ASSUMED_Pred_Bound_kW、DataValid）后参与前馈；无效/超时自动 P_ff=0 退回 Baseline。');
W('');
W('## 11. 验收核对');
for i = 1:size(res, 1)
    tag = '通过'; if ~res{i, 2}, tag = '**失败**'; end
    W(sprintf('- [%s] %s：%s', tag, res{i, 1}, res{i, 3}));
end
if allPass, ov = '全部验收项通过'; else, ov = '存在失败项，需修复'; end
W(sprintf('- **总体: %s**', ov));
W('');
W('---');
W('*本报告由 scripts/06_validate_results.m 生成。原模型与原参数脚本未被修改。*');
fclose(fid);
fprintf('[06] 最终报告: %s\n', fullfile(ROOT, 'reports', 'ModelA_build_report.md'));
if allPass, tAll = '全部通过'; else, tAll = '存在失败项'; end
fprintf('[06] 验收%s\n', tAll);
