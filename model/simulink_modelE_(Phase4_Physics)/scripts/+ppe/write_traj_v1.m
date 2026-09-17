function out = write_traj_v1(ds, deriv, out_dir, voyage_id, arm, run_id)
% ppe.write_traj_v1 — epoch-2 1 Hz 植物轨迹工件（FC-5A 观测契约修正，
% schema ppe_plant_traj_v1）。
%
% 通道只包含【真实 Model-E logsout 信号】与【有源精确导出】两类：
%   - 22 个 PLANT_LOGGED 监督/物理信号（C2 运行时普查在册，0.01 s 原生）；
%   - 6 个 16_Safety_Interlock 联锁族信号（替代不可得的抽象
%     safety_hard_count —— 普查证实其无 logsout 来源）；
%   - 2 个精确导出（来源=模型自身算术，构建脚本 build_model_e.m 定义）：
%       P_bat_bus_kW    = P_bus_residual_kW − eta_rect·P_gen_actual_kW
%                         + P_load_total_kW（09_Bus_Residual_Monitor 恒等式反解）
%       derate08_active = double(V_dc_V < CCS_Vdc_Steady_Band(1))
%                         （derate_fn 线性降额 f<1 ⇔ V < v_hi）
%   - delta_lambda / forecast_source_id 不在植物表：属 controller_trace_v1
%     与终端记录元数据（RUNNER_DERIVED / RUN_METADATA）。
%
% FAIL CLOSED：任一 PLANT 通道或导出所需输入缺失即抛错。
% 原子写 MAT v7.3（tmp → movefile）+ sidecar JSON（含推导常数溯源）。
% 命名（design §5.2）：<formal_dir>/<voyage_id>/<arm>.traj.v1.mat。

CH = {'P_load_total_kW','P_prop_demand_kW','P_prop_actual_kW','P_unserved_kW', ...
      'P_gen_unserved_kW','P_gen_actual_kW','P_DG1_kW','P_DG2_kW','state_DG1','state_DG2', ...
      'commitment_state','N_dispatch','P_bat_actual_kW','P_bat_dis_avail_kW','P_bat_chg_avail_kW', ...
      'SOC','V_bat_V','I_bat_A','V_dc_V', ...
      'alarm_steady','alarm_transient','flag_vdc_low','flag_soc_low', ...
      'safety_baseline_fallback_active'};
% 注：fault_latch 为每机 [1×2] 锁存（DG1,DG2），1 Hz 单列表语义含混，
% 且属 PMS 内部锁存诊断——不入强制植物表（C3-B 处置原则）。
DERIVED_NEED = {'P_bus_residual_kW'};

missing = CH(~cellfun(@(c) isfield(ds, c), CH));
missing = [missing, DERIVED_NEED(~cellfun(@(c) isfield(ds, c), DERIVED_NEED))];
if ~isfield(ds, 't')
    error('PPE:TRAJ:FAILCLOSED', '必需信号缺失：t（时间轴）');
end
if ~isempty(missing)
    error('PPE:TRAJ:FAILCLOSED', '轨迹必需通道缺失：%s', strjoin(missing, ','));
end

% 1 Hz 右端点采样：t 网格 = ceil(ds.t)（去重保末值）
t_all = ds.t(:);
edges = unique(ceil(t_all - 1e-9));
n = numel(edges);
idx = zeros(n, 1);
cur = 1;
for k = 1:n
    while cur < numel(t_all) && t_all(cur) < edges(k) - 1e-9
        cur = cur + 1;
    end
    idx(k) = min(cur, numel(t_all));
end
traj = struct();
traj.time = edges(:);
traj = orderfields(traj, {'time'});
for c = 1:numel(CH)
    sig = ds.(CH{c});
    if numel(sig) == n
        traj.(CH{c}) = sig(:);
    elseif numel(sig) == 1
        traj.(CH{c}) = repmat(sig, n, 1);
    else
        traj.(CH{c}) = sig(idx);
    end
end
% ---- 精确导出（来源：build_model_e.m 09_Bus_Residual_Monitor / CODE_DERATE）----
pbat_bus = ds.P_bus_residual_kW(:) - deriv.eta_rect * ds.P_gen_actual_kW(:) ...
    + ds.P_load_total_kW(:);
traj.P_bat_bus_kW = pbat_bus(idx);
derate_active = double(ds.V_dc_V(:) < deriv.ccs_v_hi);
traj.derate08_active = derate_active(idx);

% 原子写：tmp → movefile；命名 <voyage>/<arm>.traj.v1.mat
if ~exist(out_dir, 'dir'), mkdir(out_dir); end
mat = fullfile(out_dir, sprintf('%s.traj.v1.mat', char(arm)));
tmp = [mat '.writing'];   % char 拼接用方括号（+ 对 char 是数值加法）
save(tmp, '-v7.3', 'traj');
movefile(tmp, mat, 'f');

% sidecar
sha = model.sha256_file(mat);
all_ch = [{'time'}, CH, {'P_bat_bus_kW','derate08_active'}];
d1 = struct('channel', 'P_bat_bus_kW', ...
    'relation', 'P_bus_residual_kW - eta_rect*P_gen_actual_kW + P_load_total_kW', ...
    'source', 'build_model_e.m buildBusResidualMonitor (09_Bus_Residual_Monitor)', ...
    'eta_rect', deriv.eta_rect);
d2 = struct('channel', 'derate08_active', ...
    'relation', 'double(V_dc_V < CCS_Vdc_Steady_Band(1))  % derate_fn(V)<1', ...
    'source', 'build_model_e.m CODE_DERATE', ...
    'ccs_v_hi', deriv.ccs_v_hi, 'ccs_v_lo', deriv.ccs_v_lo);
side = struct('schema', 'ppe_plant_traj_v1', 'voyage_id', char(voyage_id), ...
    'arm', char(arm), 'run_id', char(run_id), 'n_samples', n, ...
    't_end', traj.time(end), 'sha256', sha, ...
    'utc', char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z')));
side.channels = all_ch;
side.derivations = {d1, d2};   % cell 赋值 → JSON 数组（struct() cell 语义陷阱）
fid = fopen(fullfile(out_dir, sprintf('%s.traj.v1.sidecar.json', char(arm))), 'w');
fwrite(fid, jsonencode(side, 'PrettyPrint', true));
fclose(fid);

out = struct('path', mat, 'sha256', sha, 'n_samples', n, 'sidecar', side);
end
