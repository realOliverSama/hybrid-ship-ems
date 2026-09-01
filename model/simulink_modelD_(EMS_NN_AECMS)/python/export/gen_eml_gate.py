# -*- coding: utf-8 -*-
"""gen_eml_gate.py — T-310：生成 4 个门控模式（G0~G3）的 law7 EML chart 脚本

模板：debug_law7_eml_law7.txt（现有内联 law7 chart dump）。
修订：①W3 [32,3] → 三分位数输出；②ê 取 q50；③K_q 分位数门控（S1/S2 内嵌字面量）；
④G2 识别器（workclf_v1 卷积网络，权重内嵌）+ 迟滞状态机；⑤GATE_MODE 字面量。
输出：results/ecms_phase25/gating/eml_ecms_step_mode{0,1,2,3}.m
"""
import io, sys, os, json
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
import numpy as np

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
TPL = os.path.join(BASE, "debug_law7_eml_law7.txt")
DEP = os.path.join(BASE, "results", "ecms_phase25", "deploy", "selected_model_weights_v2.json")
OUTD = os.path.join(BASE, "results", "ecms_phase25", "gating")

NEW_FWD = r"""if ok
xnorm = (x - XMean(:)) ./ XStd(:);
h1 = max(W1.' * xnorm + b1(:), 0);
h2 = max(W2.' * h1 + b2(:), 0);
y_norm = W3.' * h2 + b3;                       % [3x1] = [q10;q50;q90]（归一化域）
    Ehat_q = y_norm * YStd + YMean;                % [3x1] kWh
    e_hat = (Ehat_q(2) - Ebar) / Escale;           % ê 取 q50（T-309 §2）

    % ---- G1：分位数门控（T-309 §3，S1/S2 = val P75/P95 字面量）----
    s_q = Ehat_q(3) - Ehat_q(1);                   % q90-q10 [kWh]
    if s_q <= GATE_S1
        K_q = 1;
    elseif s_q >= GATE_S2
        K_q = 0;
    else
        K_q = (GATE_S2 - s_q) / (GATE_S2 - GATE_S1);
    end

@@G2BEGIN@@
    % ---- G2：工况识别门控（workclf_v1，T-308；仅 upstream 可辨识，T-309 修订）----
    Pdec = xnorm(1:10:300);                        % 30x1（归一化域切片，与 T-312 训练口径一致；2026-09 口径修订，原 xP(1:10:300) raw 口径致 int8 标定错配坍缩，AI_MODELING §22）
    c1 = zeros(30, 16);
    for jf = 1:16
        for jt = 1:30
            acc = C1B(jf);
            for kk = 1:5
                jj = jt - 5 + kk;
                if jj >= 1
                    acc = acc + C1W(kk, 1, jf) * Pdec(jj);
                end
            end
            c1(jt, jf) = max(acc, 0);
        end
    end
    p1 = zeros(15, 16);
    for jt = 1:15
        p1(jt, :) = max(c1(2*jt - 1, :), c1(2*jt, :));
    end
    c2 = zeros(15, 32);
    for jf = 1:32
        for jt = 1:15
            acc = C2B(jf);
            for kk = 1:3
                jj = jt - 3 + kk;
                if jj >= 1
                    acc = acc + p1(jj, :) * reshape(C2W(kk, :, jf), 16, 1);
                end
            end
            c2(jt, jf) = max(acc, 0);
        end
    end
    p2 = zeros(7, 32);
    for jt = 1:7
        p2(jt, :) = max(c2(2*jt - 1, :), c2(2*jt, :));
    end
    c3 = zeros(7, 32);
    for jf = 1:32
        for jt = 1:7
            acc = C3B(jf);
            for kk = 1:3
                jj = jt - 3 + kk;
                if jj >= 1
                    acc = acc + p2(jj, :) * reshape(C3W(kk, :, jf), 32, 1);
                end
            end
            c3(jt, jf) = max(acc, 0);
        end
    end
    gfeat = mean(c3, 1)';                          % GAP [32x1]
    s8 = max(F1W' * [xnorm(301); xnorm(302)] + F1B(:), 0);  % [8x1]（归一化 v/accel，口径修订）
    hcl = max(F2W' * [gfeat; s8] + F2B(:), 0);     % [64x1]
    ocl = F3W' * hcl + F3B(:);                     % [7x1]
    [~, cls] = max(ocl);                           % 1..7（1=depart..6=upstream..7=berth）

    % 迟滞状态机（T-309 §4：连续 3 窗同类才切换）
    if cls_eff < 0
        cls_eff = cls; run_cls = cls; run_cnt = 0;
    elseif cls == cls_eff
        run_cls = cls; run_cnt = 0;
    elseif cls == run_cls
        run_cnt = run_cnt + 1;
        if run_cnt >= GATE_M
            cls_eff = cls; run_cnt = 0;
        end
    else
        run_cls = cls; run_cnt = 1;
    end
    if cls_eff == 6                                % 6=upstream（唯一可辨识受限类）
        K_w = 0;
    else
        K_w = 1;
    end
@@G2END@@

    % ---- GATE_MODE：0=G0 无门控 / 1=G1 / 2=G2 / 3=G3 ----
    if GATE_MODE == 0
        K = 1;
    elseif GATE_MODE == 1
        K = K_q;
    elseif GATE_MODE == 2
        K = K_w;
    else
        K = K_q * K_w;
    end
    e_hat = K * e_hat;
else
    % 回退：权重缺失或输入非有限 -> ê=0（≡law=5 行为，FR-208）
    e_hat = 0;
end"""

OLD_FWD = """if ok
xnorm = (x - XMean(:)) ./ XStd(:);
h1 = max(W1.' * xnorm + b1(:), 0);
h2 = max(W2.' * h1 + b2(:), 0);
y_norm = W3.' * h2 + b3;
    Ehat_kWh = y_norm * YStd + YMean;
    e_hat = (Ehat_kWh - Ebar) / Escale;
else
    % 回退：权重缺失或输入非有限 -> ê=0（≡law=5 行为，FR-208）
    e_hat = 0;
end"""

def mat_lit(A, name):
    A = np.asarray(A, dtype=float)
    if A.ndim == 2:
        body = "; ".join(" ".join("%.10g" % v for v in row) for row in A.tolist())
        return "%s = [%s];" % (name, body)
    if A.ndim == 3:
        pages = []
        for pi in range(A.shape[2]):
            body = "; ".join(" ".join("%.10g" % v for v in row) for row in A[:, :, pi].tolist())
            pages.append("[" + body + "]")
        return "%s = cat(3, %s);" % (name, ", ".join(pages))
    raise ValueError(str(A.shape))

def main():
    no_clf = "--no-clf" in sys.argv
    clf_stub = "--clf-stub" in sys.argv
    tpl = open(TPL, encoding="utf-8").read()
    tpl = tpl.split("----\n", 1)[1]
    j = json.load(open(DEP, encoding="utf-8"))
    C = j["clf"]
    G = j["gate"]
    S1, S2 = G["S1_kwh"], G["S2_kwh"]

    # 1) 标记剥离（行级：fwd_with 保留 G2 段只删标记行；fwd_without/stub 删段）
    nf_lines = NEW_FWD.splitlines(True)
    in_g2 = False
    g2_lines = []
    fwd_with_lines = []
    for _l in nf_lines:
        _s = _l.strip()
        if _s == '@@G2BEGIN@@':
            in_g2 = True
            continue
        if _s == '@@G2END@@':
            in_g2 = False
            continue
        fwd_with_lines.append(_l)
        if in_g2:
            g2_lines.append(_l)
    g2_body = ''.join(g2_lines)
    fwd_with = ''.join(fwd_with_lines)
    assert '@@' not in fwd_with, '标记未剥离'
    assert 'Pdec = xnorm(1:10:300);' in fwd_with, 'G2 前向代码缺失（生成器回归）'
    fwd_without = fwd_with.replace(g2_body, "    K_w = 1;   % --no-clf：识别器关闭\n")
    g2stub = fwd_with.replace(g2_body, "    cls = 3;   % --clf-stub：分类器固定 eco\n")

    # 2) 常数块
    consts = []
    consts.append("% ---- T-310 内嵌常数：识别器权重（T-308 workclf_v1）+ 门控阈值（T-309）----")
    consts.append("GATE_MODE = @@MODE@@;   %% 本变体门控模式（64 按消融批次切换）")
    consts.append("GATE_M = %d;" % G["hysteresis_windows"])
    consts.append("GATE_S1 = %.6f; GATE_S2 = %.6f;  %% kWh（val P75/P95，禁调参）" % (S1, S2))
    consts.append(mat_lit(np.asarray(C["conv1_w"]).reshape(5, 1, 16), "C1W"))
    consts.append("C1B = [%s];" % " ".join("%.10g" % v for v in C["conv1_b"]))
    consts.append(mat_lit(np.asarray(C["conv2_w"]).reshape(3, 16, 32), "C2W"))
    consts.append("C2B = [%s];" % " ".join("%.10g" % v for v in C["conv2_b"]))
    consts.append(mat_lit(np.asarray(C["conv3_w"]).reshape(3, 32, 32), "C3W"))
    consts.append("C3B = [%s];" % " ".join("%.10g" % v for v in C["conv3_b"]))
    consts.append(mat_lit(np.asarray(C["fc1_w"]).reshape(2, 8), "F1W"))
    consts.append("F1B = [%s];" % " ".join("%.10g" % v for v in C["fc1_b"]))
    consts.append(mat_lit(np.asarray(C["fc2_w"]).reshape(40, 64), "F2W"))
    consts.append("F2B = [%s];" % " ".join("%.10g" % v for v in C["fc2_b"]))
    consts.append(mat_lit(np.asarray(C["fc3_w"]).reshape(64, 7), "F3W"))
    consts.append("F3B = [%s];" % " ".join("%.10g" % v for v in C["fc3_b"]))
    consts_str = "\n".join(consts)

    # 3) 模式循环：先换前向，再换常数与模式
    os.makedirs(OUTD, exist_ok=True)
    modes = [0, 1, 2, 3]
    suffix = "_noclf" if no_clf else ("_stub" if clf_stub else "")
    for mode in modes:
        fwd = fwd_without if no_clf else (g2stub if clf_stub else fwd_with)
        tpl_m = tpl.replace(OLD_FWD, fwd)
        tpl_m = tpl_m.replace("persistent lastT Pheld buf k vprev",
                              consts_str + "\npersistent lastT Pheld buf k vprev cls_eff run_cls run_cnt")
        # codegen 要求：迟滞持久变量须在 isempty(lastT) 初始化分支全路径定义（T-310）
        tpl_m = tpl_m.replace("""if isempty(lastT)
    lastT = -1e9;
    Pheld = Pgen_init;
    k = 1;
    buf = zeros(300, 1);
    buf(:) = P_load;
    vprev = v_ship;
end""",
"""if isempty(lastT)
    lastT = -1e9;
    Pheld = Pgen_init;
    k = 1;
    buf = zeros(300, 1);
    buf(:) = P_load;
    vprev = v_ship;
    cls_eff = -1; run_cls = -1; run_cnt = 0;
end""")
        tpl_m = tpl_m.replace("""if t < lastT
    % 新仿真开始：重置缓冲/写指针/速度差基线（与 ecms2.nn_predict_ehat 同语义）
    lastT = t;
    k = 1;
    buf(:) = P_load;
    vprev = v_ship;
end""",
"""if t < lastT
    % 新仿真开始：重置缓冲/写指针/速度差基线（与 ecms2.nn_predict_ehat 同语义）
    lastT = t;
    k = 1;
    buf(:) = P_load;
    vprev = v_ship;
    cls_eff = -1; run_cls = -1; run_cnt = 0;   % T-310：门控迟滞状态复位（-1=无状态）
end""")
        code = tpl_m.replace("@@MODE@@", str(mode))
        assert "@@" not in code, "生成代码残留标记: " + repr([l for l in code.splitlines() if "@@" in l][:2])
        out = os.path.join(OUTD, "eml_ecms_step_mode%d%s.m" % (mode, suffix))
        open(out, "w", encoding="utf-8").write(code)
        print("mode %d -> %s (%d chars)" % (mode, out, len(code)))
    print("VERIFY done")



if __name__ == "__main__":
    main()