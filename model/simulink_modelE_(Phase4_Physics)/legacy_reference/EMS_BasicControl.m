function [out, state] = EMS_Controller(in, state, param, dt)

% ================= 输入 =================
Vdc   = in.Vdc;
SOC   = in.SOC;
Pprop = in.Pprop;
n_cmd = in.n_cmd;

% 发电机状态
Gen_running = in.Gen_running;
Gen_fault   = in.Gen_fault;

% ================= Step 1: 负载低通滤波 =================
alpha = dt / (param.LPF_tau + dt);
state.P_load_avg = (1 - alpha)*state.P_load_avg + alpha*Pprop;

% ================= Step 2: SOC调节 =================
SOC_error = param.SOC_target - SOC;

% ================= Step 3: 发电机功率参考 =================
Pgen_ref_raw = state.P_load_avg + param.K_soc * SOC_error;

% 限幅
Pgen_ref_raw = min(max(Pgen_ref_raw, param.P_gen_min), param.P_gen_max);

% ================= Step 4: 发电机爬坡限制 =================
dP = Pgen_ref_raw - state.Pgen_ref;
dP = max(min(dP, param.Gen_ramp_rate * dt), -param.Gen_ramp_rate * dt);

state.Pgen_ref = state.Pgen_ref + dP;

% ================= Step 5: 电池功率计算 =================
Pbat_ref = Pprop - state.Pgen_ref;

% 限幅
Pbat_ref = min(max(Pbat_ref, param.P_bat_min), param.P_bat_max);

% 判断是否饱和
Bat_saturation = (abs(Pbat_ref) >= param.P_bat_max * 0.95);

% ================= Step 6: 推进 droop 控制 =================
if (Vdc < param.Vdc_nom) || Bat_saturation
    n_ref = n_cmd + param.K_droop * (Vdc - param.Vdc_nom);
else
    n_ref = n_cmd;
end

% ================= Step 7: 发电机启停逻辑 =================
Gen_start = false;
Gen_stop  = false;

if ~Gen_running && ~Gen_fault
    if (SOC < param.SOC_low) || (Pprop > 100)
        Gen_start = true;
    end
end

if Gen_running
    if (SOC > param.SOC_high) && (Pprop < 50)
        Gen_stop = true;
    end
end

% ================= 输出 =================
out.Pgen_ref = state.Pgen_ref;
out.Pbat_ref = Pbat_ref;
out.n_ref    = n_ref;
out.Gen_start = Gen_start;
out.Gen_stop  = Gen_stop;

end