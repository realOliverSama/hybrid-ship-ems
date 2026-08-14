%% ========================================================================
%  ModelA_GateThresholds.m — 质量门硬阈值配置（Agent 提交前置条件）
%  被 03_run_smoke_tests.m 引用。
%  注意：修改本文件 = 修改提交门禁标准，属于 AGENTS.md 批准类操作。
% ========================================================================

GATE = struct();
GATE.baseline_file = fullfile('config', 'gate_baseline_smoke.json');

% ---- 回归容差（相对已知良好基线，比例） ----
GATE.tol.fuel_kg_rel   = 0.05;   % 油耗 ±5%
GATE.tol.mean_load_rel = 0.05;   % 平均负荷 ±5%
GATE.tol.v_ss_rel      = 0.02;   % 稳态航速 ±2%
GATE.tol.E_gen_rel     = 0.05;   % 发电能量 ±5%
GATE.tol.t_end_rel     = 0.05;   % 航次时长 ±5%

% ---- 绝对容差/硬上限 ----
GATE.tol.soc_end_abs   = 0.02;   % 结束 SOC 绝对偏差上限
GATE.tol.soc_min_abs   = 0.02;   % 最低 SOC 绝对偏差上限
GATE.tol.v_ripple_max  = 0.10;   % 巡航段速度波动硬上限 [m/s]（后半程去均值 RMSE）

% ---- 退化白名单（默认空：任何超容差即 FAIL；加项需说明理由） ----
GATE.whitelist = {};
