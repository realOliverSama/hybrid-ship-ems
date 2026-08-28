function results = gate_compare(testName, m, baseline, GATE)
% model.gate_compare — 门禁指标 vs 基线 + 硬阈值
% 输入:
%   testName : 测试名（如 'T01_still_water'，用作基线 struct 字段）
%   m        : gate_metrics 输出
%   baseline : 基线 struct（空 [] 表示无基线）
%   GATE     : Model_GateThresholds.m 定义的阈值 struct
% 输出: results = {检查项, 通过(logical), 说明} 的 N×3 cell
    results = {};
    function push(nm, pass, det)
        results{end + 1, 1} = nm; %#ok<AGROW>
        results{end, 2} = logical(pass); %#ok<AGROW>
        results{end, 3} = det; %#ok<AGROW>
    end

    % ---- 硬阈值：巡航段速度波动 ----
    if isfinite(m.v_ripple)
        push('v_ripple_max', m.v_ripple <= GATE.tol.v_ripple_max, ...
            sprintf('v_ripple=%.4f <= %.3f m/s', m.v_ripple, GATE.tol.v_ripple_max));
    end

    if isempty(baseline) || ~isfield(baseline, testName)
        push('baseline', true, '无基线（本次运行已建立基线，跳过回归对比）');
        return;
    end
    b = baseline.(testName);

    % ---- 相对偏差回归 ----
    relChecks = {'fuel_kg', GATE.tol.fuel_kg_rel; ...
                 'mean_load_kW', GATE.tol.mean_load_rel; ...
                 'v_ss', GATE.tol.v_ss_rel; ...
                 'E_gen_kWh', GATE.tol.E_gen_rel; ...
                 't_end', GATE.tol.t_end_rel};
    for i = 1:size(relChecks, 1)
        f = relChecks{i, 1}; tol = relChecks{i, 2};
        if ~isfield(b, f) || ~isfinite(b.(f)) || b.(f) == 0, continue; end
        dev = abs(m.(f) - b.(f)) / abs(b.(f));
        pass = dev <= tol || any(strcmp(f, GATE.whitelist));
        push([f '_regress'], pass, sprintf('%s: %.4g vs 基线 %.4g（偏差 %.2f%%，容差 %.0f%%）', ...
            f, m.(f), b.(f), dev * 100, tol * 100));
    end

    % ---- 绝对偏差回归 ----
    absChecks = {'soc_end', GATE.tol.soc_end_abs; ...
                 'soc_min', GATE.tol.soc_min_abs};
    for i = 1:size(absChecks, 1)
        f = absChecks{i, 1}; tol = absChecks{i, 2};
        if ~isfield(b, f) || ~isfinite(b.(f)), continue; end
        dev = abs(m.(f) - b.(f));
        pass = dev <= tol || any(strcmp(f, GATE.whitelist));
        push([f '_regress'], pass, sprintf('%s: %.4f vs 基线 %.4f（|Δ|=%.4f，容差 %.3f）', ...
            f, m.(f), b.(f), dev, tol));
    end
end
