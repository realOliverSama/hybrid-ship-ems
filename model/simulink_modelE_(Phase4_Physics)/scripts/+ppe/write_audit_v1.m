function out = write_audit_v1(ds, cfg, vdir, voyage_id, arm, run_id)
% ppe.write_audit_v1 — epoch-2 高速率硬谓词审计工件（design Gate A6 / §5b）。
%
% 1 Hz 轨迹覆盖慢量谓词；本工件在 0.01 s 原生节拍就地评估快量谓词并持久化
% 紧凑审计 JSON：
%   - battery SOP V / I / P-over-avail（duration_tol 语义与 checker v2 一致）
%   - genset ramp（仅 ON↔ON 区间，逐 tick；断路器事件豁免同 checker v2）
% 记录：违规计数、最大越界、累计时长、首末时间戳、极值、事件态上下文
% （state/commitment）。
%
% 本工件是证据/诊断，不是分类器：正式可行性判定唯一来源是
% ppe.feasibility_contract_v2（runner 在调用本写入器之前完成）。
%
% 原子写（tmp → movefile）；任何写入失败抛错 → 臂不产生终端记录（臂原子）。
% 文件名：fullfile(vdir, [arm '.audit.v1.json'])。
%
% 返回 out：struct('path',…,'sha256',…,'n_samples',…,'any_violation',…)。

TOL = 1e-6;
need = {'t', 'V_bat_V', 'I_bat_A', 'P_bat_actual_kW', ...
    'P_bat_dis_avail_kW', 'P_bat_chg_avail_kW'};
for i = 1:numel(need)
    if ~isfield(ds, need{i})
        error('PPE:AUDIT:FAILCLOSED', '审计必需信号缺失：%s', need{i});
    end
end
hf = cfg.hard_feasibility;
bvi = hf.battery_sop_v_i;
rmp = hf.genset_limits.ramp_kW_per_s + hf.genset_limits.ramp_tol_kW_per_s;

tv = ds.t(:);
n = numel(tv);
V = ds.V_bat_V(:); I = ds.I_bat_A(:); Pb = ds.P_bat_actual_kW(:);
Pav = zeros(n, 1); chg = Pb < 0;
Pav(chg) = ds.P_bat_chg_avail_kW(chg);
Pav(~chg) = ds.P_bat_dis_avail_kW(~chg);

% ---- 事件收集器（局部函数，避免与主工作区变量遮蔽）----
    function e = ev_new(lim)
        e = struct('count', 0, 'duration_s', 0, 'max_exceed', 0, ...
            'first_t', NaN, 'last_t', NaN, 'limit', lim, ...
            'context_state_DG1', [], 'context_state_DG2', [], ...
            'context_commitment_state', []);
    end
    function e = ev_add(e, tk, dt_h, exceed, dsv, k)
        e.count = e.count + 1;
        e.duration_s = e.duration_s + dt_h;
        e.max_exceed = max(e.max_exceed, exceed);
        if isnan(e.first_t), e.first_t = tk; end
        e.last_t = tk;
        if isempty(e.context_state_DG1) && isfield(dsv, 'state_DG1')
            e.context_state_DG1 = dsv.state_DG1(k);
        end
        if isempty(e.context_commitment_state) && isfield(dsv, 'commitment_state')
            e.context_commitment_state = dsv.commitment_state(k);
        end
        if isempty(e.context_state_DG2) && isfield(dsv, 'state_DG2')
            e.context_state_DG2 = dsv.state_DG2(k);
        end
    end

v_lo = V < bvi.v_min_V - 1e-12;
v_hi = V > bvi.v_max_V + 1e-12;
i_out = abs(I) > bvi.i_abs_max_A + 1e-12;
p_over = abs(Pb) > Pav + 1e-9;
ev_v = ev_new([bvi.v_min_V, bvi.v_max_V]);
ev_i = ev_new(bvi.i_abs_max_A);
ev_p = ev_new('P_avail');
for k = 1:n
    if k > 1, h = tv(k) - tv(k-1); else, h = 0; end
    if v_lo(k) || v_hi(k)
        if v_lo(k), x = bvi.v_min_V - V(k);
        else, x = V(k) - bvi.v_max_V;
        end
        ev_v = ev_add(ev_v, tv(k), h, x, ds, k);
    end
    if i_out(k)
        ev_i = ev_add(ev_i, tv(k), h, abs(I(k)) - bvi.i_abs_max_A, ds, k);
    end
    if p_over(k)
        ev_p = ev_add(ev_p, tv(k), h, abs(Pb(k)) - Pav(k), ds, k);
    end
end

% ---- genset ramp 逐 tick（仅 ON↔ON）----
ramp_events = struct('count', 0, 'duration_s', 0, 'max_rate_kW_per_s', 0, ...
    'limit_kW_per_s', rmp, 'first_t', NaN, 'last_t', NaN);
gn = {'P_DG1_kW', 'P_DG2_kW'};
gs = {'state_DG1', 'state_DG2'};
if ~(isfield(ds, gn{1}) && isfield(ds, gn{2}) ...
        && isfield(ds, gs{1}) && isfield(ds, gs{2}))
    % 与 checker v2 同口径：缺分机组信号必须 FAIL CLOSED（审计也不例外）
    error('PPE:AUDIT:FAILCLOSED', '审计必需信号缺失：P_DG1_kW/P_DG2_kW/state');
end
p1 = ds.(gn{1})(:); p2 = ds.(gn{2})(:);
s1 = ds.(gs{1})(:); s2 = ds.(gs{2})(:);
on1 = (s1 == 3); on2 = (s2 == 3);
for k = 1:n-1
    if ~(on1(k) && on1(k+1) && on2(k) && on2(k+1)), continue; end
    h = max(tv(k+1) - tv(k), 1e-12);
    r1 = abs(p1(k+1) - p1(k)) / h;
    r2 = abs(p2(k+1) - p2(k)) / h;
    r = max(r1, r2);
    if r > rmp
        ramp_events.count = ramp_events.count + 1;
        ramp_events.duration_s = ramp_events.duration_s + h;
        ramp_events.max_rate_kW_per_s = max(ramp_events.max_rate_kW_per_s, r);
        if isnan(ramp_events.first_t), ramp_events.first_t = tv(k); end
        ramp_events.last_t = tv(k);
    end
end

any_viol = (ev_v.count + ev_i.count + ev_p.count + ramp_events.count) > 0;
derate_s = 0;
if isfield(ds, 'derate08_active')
    derate_s = sum(ds.derate08_active(:) > 0.5);
end

doc = struct();
doc.schema = 'ppe_audit_v1';
doc.voyage_id = char(voyage_id);
doc.arm = char(arm);
doc.run_id = char(run_id);
doc.n_samples = n;
doc.t_first = tv(1);
doc.t_last = tv(end);
doc.tolerances = struct('ramp_kW_per_s', rmp, ...
    'v_min_V', bvi.v_min_V, 'v_max_V', bvi.v_max_V, ...
    'i_abs_max_A', bvi.i_abs_max_A);
doc.battery_voltage = ev_v;
doc.battery_current = ev_i;
doc.battery_power_over_avail = ev_p;
doc.genset_ramp_on_on = ramp_events;
doc.extremes = struct('V_bat_min_V', min(V), 'V_bat_max_V', max(V), ...
    'I_bat_min_A', min(I), 'I_bat_max_A', max(I));
doc.derate08_active_s = derate_s;
doc.any_violation = any_viol;
doc.note = ['diagnostic evidence only; epoch-2 classification authority ', ...
    'is ppe.feasibility_contract_v2'];
doc.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z'));

% 原子写：tmp → movefile
if ~exist(vdir, 'dir'), mkdir(vdir); end
fp = fullfile(vdir, [char(arm) '.audit.v1.json']);
tmp = [fp '.writing'];
fid = fopen(tmp, 'w');
fwrite(fid, jsonencode(doc, 'PrettyPrint', true));
fclose(fid);
movefile(tmp, fp, 'f');

out = struct('path', fp, 'sha256', model.sha256_file(fp), ...
    'n_samples', n, 'any_violation', any_viol);
end
