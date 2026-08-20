%% exp5_causality_evidence.m — 因果性证据：降额窗口内 需求 vs 实际 分离
ROOT = fileparts(fileparts(mfilename('fullpath')));
md = fullfile(ROOT, 'results', 'ms_phase1');
od = fullfile(md, 'oracle_causality'); if ~exist(od,'dir'), mkdir(od); end
load(fullfile(md, '_exp4_derate_active.mat'), 'ds');

t = ds.t; k200 = find(t<=200, 1, 'last');
t = t(1:k200);
Vdc    = ds.V_dc_V(1:k200);
dem    = ds.P_prop_demand_kW(1:k200);
act    = ds.P_prop_actual_kW(1:k200);
lim    = ds.P_prop_limit_kW(1:k200);
us     = ds.P_unserved_kW(1:k200);
hotel  = ds.P_hotel_kW(1:k200);
load_tot = ds.P_load_total_kW(1:k200);
dem_exo = dem + hotel;   % 外生需求负荷（oracle 目标）

% 降额窗口
der = lim < 699.5;
fprintf('=== 因果性证据（弱机 560/5/300, S09）===\n');
fprintf('降额窗口: t ∈ [%.1f, %.1f] s (%d 时刻)\n', t(find(der,1)), t(find(der,1,'last')), sum(der));
% 窗口内 demand 是否恒定 700（饱和）而 actual 被压低
if any(der)
    w = find(der);
    fprintf('窗口内 demand: min=%.1f max=%.1f (期望 ≈700 饱和)\n', min(dem(w)), max(dem(w)));
    fprintf('窗口内 actual: min=%.1f max=%.1f (期望 < 700 被降额)\n', min(act(w)), max(act(w)));
    fprintf('窗口内 dem-act 分离: max|Δ|=%.1f kW\n', max(abs(dem(w)-act(w))));
    fprintf('窗口内 dem_exo (oracle 目标): min=%.1f max=%.1f (不受 Vdc 影响)\n', ...
        min(dem_exo(w)), max(dem_exo(w)));
    % 对照：load_total（降额后实际负荷）在窗口内被压低
    fprintf('窗口内 load_total: min=%.1f max=%.1f (降额后, 受 Vdc 影响)\n', ...
        min(load_tot(w)), max(load_tot(w)));
end

% 图
f = figure('Position',[100 100 1200 800]);
subplot(3,1,1); plot(t, Vdc); yline(675,'r--','derate 675'); yline(600,'r:','transient 600');
ylabel('Vdc [V]'); title('弱机 560kW/20kW/s/300kW 电池 — 欠压降额激活'); grid on;
subplot(3,1,2); plot(t, dem, 'b-', t, act, 'r-', t, lim, 'k--'); legend('P_prop_demand','P_prop_actual','P_prop_limit');
ylabel('kW'); grid on; title('需求 vs 实际（降额窗口内分离）');
subplot(3,1,3); plot(t, dem_exo, 'b-', t, load_tot, 'r-'); legend('demand+hotel (oracle目标)','P_load_total (降额后)');
ylabel('kW'); xlabel('t [s]'); grid on; title('oracle 目标不受 Vdc 影响');
saveas(f, fullfile(od, 'causality_derate_evidence.png'));
fprintf('saved %s\n', fullfile(od, 'causality_derate_evidence.png'));
