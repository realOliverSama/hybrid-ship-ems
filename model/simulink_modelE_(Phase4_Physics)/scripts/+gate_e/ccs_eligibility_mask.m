function m = ccs_eligibility_mask(t, v_ref, st1, st2, n_dispatch, inj_active)
% gate_e.ccs_eligibility_mask — 10b CCS 稳态 eligibility mask（DGN-14/FR-490，hygiene）
%   eligible(t) = (t > T_START_MIN_S)
%               ∧ （v_ref 在 (t−VREF_STEP_WIN_S, t] 内无阶跃变化）
%               ∧ （无 bench 注入处于激活窗口）
%               ∧ （relevant DG 稳态：全部机组 ∈ {OFF(0),ON(3)}、settling 窗内无状态
%                  迁移、N_dispatch 与 ON 机组数一致——消费 GensetUnitE 现有编码
%                  0=OFF/1=STARTING/2=SYNCHRONIZING/3=ON/4=COOLDOWN，禁新造枚举）
%   CCS 阈值语义不变（带值/恢复阈值由调用方原样引用，本函数不接触）；本函数只产
%   mask 与暂态豁免证据，不做 PASS/FAIL。fail-closed（n_eligible==0 ⇒ FAIL）由
%   调用方（10b 判据段）执行——保持本函数纯函数可单测。
%   常数为 design 冻结工程判断（启动 25+5 s 物理 + 收敛裕量）：
%     T_START_MIN_S=60、VREF_STEP_WIN_S=30、VREF_STEP_TOL_MPS=0.10、DG_SETTLE_WIN_S=30。
%   VREF 阶跃容差 0.10 m/s/采样：> 最大指令斜坡 0.20 m/s² 的一半（0.01 s 网格下
%   逐采样 0.002）——只捕阶跃/查表拐点，不捕连续斜坡。

    T_START_MIN_S    = 60.0;
    VREF_STEP_WIN_S  = 30.0;
    VREF_STEP_TOL_MPS = 0.10;
    DG_SETTLE_WIN_S  = 30.0;

    t = double(t(:)); v_ref = double(v_ref(:));
    st1 = double(st1(:)); st2 = double(st2(:));
    n = numel(t);
    if ~isscalar(inj_active)
        inj_active = double(inj_active(:));
        assert(numel(inj_active) == n, 'inj_active 长度须与 t 一致');
    else
        inj_active = repmat(double(inj_active), n, 1);
    end
    n_dispatch = double(n_dispatch(:));
    assert(numel(v_ref) == n && numel(st1) == n && numel(st2) == n && numel(n_dispatch) == n, ...
        '输入长度不一致');
    assert(all(diff(t) > 0), 't 必须严格递增');

    % --- c1: 启动窗 ---
    c1 = t > T_START_MIN_S;

    % --- c2: v_ref 无阶跃（(t-30, t] 窗内）---
    c2 = true(n, 1);
    if n >= 2
        dv = abs(diff(v_ref));
        t_tail = t(2:end);
        step_t = t_tail(dv > VREF_STEP_TOL_MPS);
        for k = 1:numel(step_t)
            c2 = c2 & ~(t >= step_t(k) & t < step_t(k) + VREF_STEP_WIN_S);
        end
    end

    % --- c3: 无注入激活 ---
    c3 = inj_active == 0;

    % --- c4: relevant DG 稳态 ---
    states_ok = all([st1, st2] == 0 | [st1, st2] == 3, 2);
    n_on = double(st1 == 3) + double(st2 == 3);
    dispatch_ok = (n_dispatch == n_on);
    c4_state = states_ok & dispatch_ok;
    c4_settle = true(n, 1);
    if n >= 2
        d1 = diff(st1); d2 = diff(st2);
        t_tail = t(2:end);
        trans_t = t_tail((d1 ~= 0) | (d2 ~= 0));
        for k = 1:numel(trans_t)
            c4_settle = c4_settle & ~(t >= trans_t(k) & t < trans_t(k) + DG_SETTLE_WIN_S);
        end
    end
    c4 = c4_state & c4_settle;

    eligible = c1 & c2 & c3 & c4;

    % --- 暂态豁免窗（最大连续 ineligible 段的 [起, 止]）---
    d = diff([false; eligible; false]);
    istart = find(d == 1);
    istop = find(d == -1) - 1;
    exemption_windows = [t(istart), t(istop)];

    m = struct('eligible', eligible, 'n_eligible', sum(eligible), 'n_total', n, ...
        'exemption_windows', exemption_windows, ...
        'n_exemption_windows', size(exemption_windows, 1), ...
        'constants', struct('T_START_MIN_S', T_START_MIN_S, ...
        'VREF_STEP_WIN_S', VREF_STEP_WIN_S, 'VREF_STEP_TOL_MPS', VREF_STEP_TOL_MPS, ...
        'DG_SETTLE_WIN_S', DG_SETTLE_WIN_S));
end
