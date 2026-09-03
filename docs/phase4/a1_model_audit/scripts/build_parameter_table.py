#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_parameter_table.py — Phase 4-A1 (FR-404/405) 参数总表生成。

纯文本解析（禁止调用 MATLAB），只读输入：
  model/simulink_modelD_(EMS_NN_AECMS)/config/Model_Parameters.m
  model/simulink_modelD_(EMS_NN_AECMS)/config/Model_Scenarios.m
  model/simulink_modelD_(EMS_NN_AECMS)/config/Ecms_Phase0_Config.m
  model/simulink_modelD_(EMS_NN_AECMS)/reports/parameter_audit.csv
输出：
  docs/phase4/a1_model_audit/modelE_parameter_audit.csv
  列: parameter,current_value,unit,module,legacy_caliber,source_class5,
      impact,user_value,user_reference,notes

幂等合并（用户回填永不丢失）：输出 CSV 已存在时，以 (parameter, module) 为键
保留人工列（source_class5/impact/user_value/user_reference/notes）原值，
只更新脚本列（current_value/unit/module/legacy_caliber）；新参数行才用默认值。

legacy_caliber 判定优先级（design §4.3）：
  ASSUMED_ 变量名前缀 > 行内注释 [DRAWING]/[LEGACY]/[ASSUMED] 标记
  > %% 章节头标记 > UNKNOWN（映射为 source_class5=pending，备注必填）。

Spec: .specs/phase4-a1-model-e-audit/design.md §2/§3(OQ-1)/§4.3/§5
"""
import csv
import re
import sys
from pathlib import Path

EXPECTED_HEADER = [
    "parameter", "current_value", "unit", "module", "legacy_caliber",
    "source_class5", "impact", "user_value", "user_reference", "notes",
]
OLD_HEADER = ["parameter", "value", "unit", "source", "confidence",
              "used_by", "needs_confirmation"]
MANUAL_IDX = {"source_class5": 5, "impact": 6, "user_value": 7,
              "user_reference": 8, "notes": 9}

CALIBER_TO_CLASS5 = {"DRAWING": "drawing", "LEGACY": "estimated", "ASSUMED": "assumed"}
LEGACY_NOTE = "继承原模型 HybridShip0730，原始依据待追溯"

NUM_RE = re.compile(r"^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$")
BRACKET_RE = re.compile(r"\[([^\[\]]*)\]")
ASSIGN_RE = re.compile(r"^([A-Za-z]\w*)\s*=(?!=)\s*(.+)$")
CONTROL_RE = re.compile(
    r"^(if|elseif|else|for|parfor|while|switch|try|catch|end|function|classdef|"
    r"return|break|continue|global|persistent)\b")
BLOCK_OPENERS = ("if", "for", "parfor", "while", "switch", "try", "function")

# impact 启发式预填关键词（首中为准；仅为默认值，人工复核可改）
HIGH_KEYWORDS = ["hull", "resist", "wake", "prop", "k_f", "k_q", "bollard",
                 "shaft", "v_design", "mass", "bsfc", "fuel", "k_eq", "bat",
                 "soc", "gen", "ecms"]
MEDIUM_KEYWORDS = ["v_dc", "bus", "dcdc", "dc_dc", "rect", "droop", "eta_",
                   "elec", "gear", "mot", "voltage", "ccs", "v_floor", "c_bus",
                   "converter"]

DERIVED_MAX_LEN = 80


def die(msg, code=1):
    sys.stderr.write("[build_parameter_table] 错误：%s\n" % msg)
    sys.exit(code)


# ---------------------------------------------------------------- 基础词法
def split_code_comment(line):
    """引号感知地切分代码与 % 注释（MATLAB 引号内的 % 不是注释）。"""
    quote = None
    i, n = 0, len(line)
    while i < n:
        c = line[i]
        if quote is not None:
            if c == quote:
                if i + 1 < n and line[i + 1] == quote:  # '' / "" 转义
                    i += 2
                    continue
                quote = None
        else:
            if c in ("'", '"'):
                quote = c
            elif c == "%":
                return line[:i], line[i:]
        i += 1
    return line, ""


def quotes_balanced(code):
    q = None
    i = 0
    while i < len(code):
        c = code[i]
        if q is not None:
            if c == q:
                if i + 1 < len(code) and code[i + 1] == q:
                    i += 2
                    continue
                q = None
        elif c in ("'", '"'):
            q = c
        i += 1
    return q is None


def split_statements(code):
    """按未配对引号外、括号深度 0 处的 ';' 切分语句（矩阵内 ';' 保留）。"""
    stmts, cur = [], []
    quote, depth = None, 0
    i, n = 0, len(code)
    while i < n:
        c = code[i]
        if quote is not None:
            cur.append(c)
            if c == quote:
                if i + 1 < n and code[i + 1] == quote:
                    cur.append(code[i + 1])
                    i += 2
                    continue
                quote = None
            i += 1
            continue
        if c in ("'", '"'):
            quote = c
            cur.append(c)
        elif c in "([{":
            depth += 1
            cur.append(c)
        elif c in ")]}":
            if depth:
                depth -= 1
            cur.append(c)
        elif c == ";" and depth == 0:
            stmts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
        i += 1
    tail = "".join(cur)
    if tail.strip():
        stmts.append(tail)
    return [s.strip() for s in stmts if s.strip()]


def has_unquoted_eq(stmt):
    quote = None
    i, n = 0, len(stmt)
    while i < n:
        c = stmt[i]
        if quote is not None:
            if c == quote:
                if i + 1 < n and stmt[i + 1] == quote:
                    i += 2
                    continue
                quote = None
        else:
            if c in ("'", '"'):
                quote = c
            elif c == "=":
                return True
        i += 1
    return False


# ---------------------------------------------------------------- 语义辅助
def is_constant_expr(expr):
    e = expr.strip()
    if not e:
        return False
    if NUM_RE.match(e):
        return True
    if e in ("true", "false"):
        return True
    if len(e) >= 2 and e[0] == e[-1] and e[0] in ("'", '"'):
        return True
    if e.startswith("[") and e.endswith("]"):
        tokens = [t for t in re.split(r"[;,\s]+", e[1:-1]) if t]
        return all(NUM_RE.match(t) for t in tokens)
    return False


def norm_trunc(expr):
    e = re.sub(r"\s+", " ", expr).strip()
    if len(e) > DERIVED_MAX_LEN:
        e = e[:DERIVED_MAX_LEN] + "…"
    return e


def render_value(expr):
    e = norm_trunc(expr)
    if e == "{":  # 多行 cell 数组（逻辑行内未闭合）
        return "派生:<多行cell定义未展开>"
    return "派生:" + e


def tag_set(text):
    """提取文本中方括号组内的 DRAWING/LEGACY/ASSUMED 口径标记。"""
    tags = set()
    for m in BRACKET_RE.finditer(text or ""):
        for tok in re.split(r"[/|,、\s]+", m.group(1)):
            if tok in ("DRAWING", "LEGACY", "ASSUMED"):
                tags.add(tok)
    return tags


def extract_unit(comment):
    """行内注释首个方括号组作单位；纯数字组（如 '[675 795]'）不算单位。"""
    for m in BRACKET_RE.finditer(comment or ""):
        tok = m.group(1).strip()
        if not tok:
            continue
        if tok == "-":
            return "-"
        if re.search(r"[A-Za-z]", tok):
            return re.sub(r"\s+", " ", tok)
    return ""


def impact_for(name):
    low = name.lower()
    for kw in HIGH_KEYWORDS:
        if kw in low:
            return "high"
    for kw in MEDIUM_KEYWORDS:
        if kw in low:
            return "medium"
    return "low"


def caliber_for(name, inline_comment, chapter):
    """legacy_caliber 判定：ASSUMED_ 前缀 > 行内注释标记 > 章节标记 > UNKNOWN。"""
    if name.startswith("ASSUMED_"):
        return "ASSUMED", "ASSUMED_ 前缀"
    tags = tag_set(inline_comment)
    if len(tags) == 1:
        return tags.pop(), "行内注释口径标记"
    if len(tags) > 1:
        return None, "行内注释含多个口径标记 %s" % sorted(tags)
    ctags = chapter["tags"]
    if len(ctags) == 1:
        return list(ctags)[0], "章节口径标记 [%s]" % list(ctags)[0]
    if len(ctags) > 1:
        return None, "章节含多个口径标记 %s" % sorted(ctags)
    return None, "章节无 [DRAWING/LEGACY/ASSUMED] 口径标记"


def struct_def_note(expr):
    """对 struct(...) 定义行给出说明性备注。"""
    m = re.search(r"'name'\s*,\s*'([^']*)'", expr)
    label = "，name=%s" % m.group(1) if m else ""
    return "结构体定义（struct(...) 调用%s，字段未逐项入表），非顶层标量参数，待人工复核" % label


def clean_header(text):
    """清理 %% 头部装饰（引号/空白/首尾分隔线），得到章节标题。"""
    t = re.sub(r"^[%\s]+", "", text.strip())
    t = re.sub(r"^[-=—]+\s*", "", t)
    t = re.sub(r"\s*[-=]+$", "", t)
    return t.strip()


# ---------------------------------------------------------------- 解析主流程
def parse_file(path, rows, stats):
    raw = path.read_text(encoding="utf-8-sig").splitlines()
    n = len(raw)
    chapter = {"title": "(文件头)", "tags": set()}
    block_stack = []
    i = 0
    while i < n:
        code, comment = split_code_comment(raw[i])
        stripped = code.strip()

        # ---- %% 章节头（%% 本身是注释标记，必须检测原始行的注释部分） ----
        if not stripped and comment.lstrip().startswith("%%"):
            cleaned = clean_header(comment.lstrip()[2:])
            if cleaned:
                chapter = {"title": cleaned, "tags": tag_set(cleaned)}
            else:
                # 纯分隔线头：向后取第一条非分隔注释行作标题；
                # 找不到标题（如闭合分隔线）则保留上一章节，不重置
                title = None
                j = i + 1
                while j < n:
                    c2, m2 = split_code_comment(raw[j])
                    if c2.strip():
                        break  # 代码行：标题搜寻结束
                    t2 = clean_header(m2)
                    if t2:
                        title = t2
                        break
                    j += 1
                if title is not None:
                    chapter = {"title": title, "tags": tag_set(title)}
            i += 1
            continue
        if not stripped:
            i += 1
            continue

        # ---- '...' 续行合并 ----
        buf = stripped
        while buf.endswith("...") and quotes_balanced(buf):
            nxt = buf[:-3].rstrip()
            i += 1
            if i >= n:
                buf = nxt
                break
            code2, _ = split_code_comment(raw[i])
            seg = code2.strip()
            buf = (nxt + " " + seg).strip() if seg else nxt
        if not buf:
            i += 1
            continue

        # ---- 控制流块栈（控制语句本身不产出行） ----
        cm = CONTROL_RE.match(buf)
        if cm:
            kw = cm.group(1)
            if kw in BLOCK_OPENERS:
                block_stack.append(kw)
            elif kw == "end" and block_stack:
                block_stack.pop()
            i += 1
            continue

        block_kw = block_stack[-1] if block_stack else None
        for stmt in split_statements(buf):
            row = build_row(stmt, comment, chapter, block_kw, stats)
            if row:
                rows.append(row)
        i += 1


def build_row(stmt, inline_comment, chapter, block_kw, stats):
    m = ASSIGN_RE.match(stmt)
    kind = None
    name = expr = ""
    if m:
        kind = "assign"
        name, expr = m.group(1), m.group(2).strip()
    elif has_unquoted_eq(stmt):
        kind = "suspect"
        lhs, rhs = stmt.split("=", 1)
        name, expr = lhs.strip(), rhs.strip()
    else:
        return None  # 函数调用/纯表达式语句（fprintf/run/error 等）：跳过

    # ---- 噪声过滤：函数调用被 has_unquoted_eq 误判为疑似赋值的情形 ----
    # assert(x >= 0) 等断言语句的 >= / <= / == 内含 '='；动态字段引用 cfg.(f)
    # 与 override 机制变量不是参数，均不入表（SCENARIOS(n)=struct(...) 保留）。
    if re.match(r"^(assert|fprintf|error|warning|disp|input)\s*\(", name):
        return None
    if ".(" in name:
        return None

    stats["parsed"] += 1
    if kind == "suspect":
        stats["suspect"] += 1

    # ---- current_value（suspect 行 RHS 原样记录；assign 行按常量/派生区分） ----
    if kind == "assign":
        if is_constant_expr(expr):
            current_value = norm_trunc(expr)
            stats["constant"] += 1
        else:
            current_value = render_value(expr)
            stats["derived"] += 1
    else:
        current_value = norm_trunc(expr)

    # ---- module（%% 章节标题；控制流块内行加后缀避免与顶层定义同键） ----
    module = chapter["title"]
    if block_kw:
        module = "%s（%s 块内）" % (module, block_kw)

    # ---- legacy_caliber ----
    caliber, caliber_reason = caliber_for(name, inline_comment, chapter)
    legacy_caliber = caliber if caliber else "UNKNOWN"

    # ---- 人工列默认值（仅新行使用；幂等合并时被保留值覆盖） ----
    source_class5 = CALIBER_TO_CLASS5.get(legacy_caliber, "pending")
    impact = impact_for(name)
    notes_parts = []
    if kind == "suspect":
        if "(" in name or "." in name:
            notes_parts.append("struct 字段/索引赋值（非顶层 NAME=value 参数），逐行登记待人工复核")
        else:
            notes_parts.append("疑似赋值行（非标准 NAME=value 形式），不丢弃，待人工复核")
        mname = re.search(r"'name'\s*,\s*'([^']*)'", expr)
        if mname:
            notes_parts.append("工况定义 name=%s" % mname.group(1))
        elif norm_trunc(expr).startswith("struct("):
            notes_parts.append(struct_def_note(expr))
    if legacy_caliber == "UNKNOWN":
        notes_parts.append("legacy_caliber 无法判定（%s），待人工复核" % caliber_reason)
    if legacy_caliber == "LEGACY":
        notes_parts.append(LEGACY_NOTE)
    if block_kw and kind == "assign":
        notes_parts.append("位于 %s 块内的再赋值（非首次定义；默认定义见同名其他行）" % block_kw)
    if current_value == "派生:<多行cell定义未展开>":
        notes_parts.append("多行 cell 定义（元数据表，非物理参数），待人工复核")
    elif current_value.startswith("派生:struct("):
        notes_parts.append(struct_def_note(expr))

    return {
        "parameter": name,
        "current_value": current_value,
        "unit": extract_unit(inline_comment),
        "module": module,
        "legacy_caliber": legacy_caliber,
        "source_class5": source_class5,
        "impact": impact,
        "user_value": "",
        "user_reference": "",
        "notes": "；".join(notes_parts),
    }


CONFIDENCE_TOKENS = {"high", "medium", "low", "na"}


def parse_old_row(row, path_name):
    """解析旧审计 CSV 行。既有文件部分行的 unit/source/used_by 含未加引号的
    ASCII 逗号，导致 csv 层切出宽行；按确定性锚点重组：行尾为 needs_confirmation，
    向右找最后一个置信度 token（high/medium/low/na）定 used_by，中段用括号配平
    定 unit 与 source。逐行响亮警告，不静默猜测；无法重组则报错退出。"""
    if len(row) == len(OLD_HEADER):
        return {"unit": row[2], "source": row[3], "confidence": row[4]}
    if len(row) < len(OLD_HEADER):
        die("%s 行列数不足（真格式漂移，不猜格式）：%s" % (path_name, row))
    k = None
    for j in range(len(row) - 2, 1, -1):  # 向右找最后一个置信度 token
        if row[j].strip().lower() in CONFIDENCE_TOKENS:
            k = j
            break
    if k is None or k < 3:
        die("%s 宽行无法确定性重组（不猜格式）：%s" % (path_name, row))
    sys.stderr.write("[build_parameter_table] 警告：%s 行含未引号内嵌逗号（%d 列），"
                     "按锚点规则重组 unit/source/used_by：%s\n" % (path_name, len(row), row[0]))
    used_by = ",".join(row[k + 1:len(row) - 1])
    mid = row[2:k]
    if mid[0].startswith("["):
        unit = None
        for j in range(len(mid)):
            cand = ",".join(mid[:j + 1])
            if cand.count("[") == cand.count("]"):
                unit = cand
                source = ",".join(mid[j + 1:])
                break
        if unit is None:
            unit, source = ",".join(mid), ""
    else:
        unit, source = mid[0], ",".join(mid[1:])
    return {"unit": unit, "source": source, "confidence": row[k]}


def main():
    # ---- 仓库根（相对路径推导 + AGENTS.md 根标记校验 + pwd 兜底，NFR-402） ----
    root = Path(__file__).resolve().parents[4]
    if not (root / "AGENTS.md").exists():
        if (Path.cwd() / "AGENTS.md").exists():
            root = Path.cwd().resolve()
        else:
            die("无法定位仓库根（parents[4]=%s 无 AGENTS.md，pwd 兜底亦失败）" % root)
    model_dir = root / "model" / "simulink_modelD_(EMS_NN_AECMS)"
    p_params = model_dir / "config" / "Model_Parameters.m"
    p_scen = model_dir / "config" / "Model_Scenarios.m"
    p_ecms = model_dir / "config" / "Ecms_Phase0_Config.m"
    p_old = model_dir / "reports" / "parameter_audit.csv"
    out_path = root / "docs" / "phase4" / "a1_model_audit" / "modelE_parameter_audit.csv"
    for p in (p_params, p_scen, p_ecms, p_old):
        if not p.exists():
            die("输入缺失：%s" % p)

    # ---- 既有输出 CSV（幂等合并键；表头漂移即报错，design §5） ----
    existing = {}
    if out_path.exists():
        with out_path.open(encoding="utf-8-sig", newline="") as f:
            rdr = csv.reader(f)
            header = next(rdr, None)
            if header != EXPECTED_HEADER:
                die("既有输出 CSV 表头漂移：%s\n期望：%s" % (header, EXPECTED_HEADER))
            for row in rdr:
                if len(row) != len(EXPECTED_HEADER):
                    die("既有输出 CSV 行列数异常：%s" % row)
                key = (row[0], row[3])
                if key in existing:
                    sys.stderr.write("[build_parameter_table] 警告：既有 CSV 重复键 %s\n" % (key,))
                existing[key] = row

    # ---- 旧审计 CSV（只读；缺失/格式漂移报错退出，design §5） ----
    old_rows = {}
    old_count = 0
    with p_old.open(encoding="utf-8-sig", newline="") as f:
        rdr = csv.reader(f)
        header = next(rdr, None)
        if header != OLD_HEADER:
            die("既有 reports/parameter_audit.csv 表头漂移（不猜格式）：%s\n期望：%s"
                % (header, OLD_HEADER))
        for row in rdr:
            old_count += 1
            info = parse_old_row(row, p_old.name)
            old_rows.setdefault(row[0], []).append(info)

    # ---- 解析两个 .m ----
    fresh = []
    stats = {"parsed": 0, "constant": 0, "derived": 0, "suspect": 0}
    for path, label in ((p_params, "Model_Parameters.m"), (p_scen, "Model_Scenarios.m"),
                        (p_ecms, "Ecms_Phase0_Config.m")):
        before = len(fresh)
        parse_file(path, fresh, stats)
        sys.stderr.write("[build_parameter_table] %s 解析出 %d 行\n" % (label, len(fresh) - before))

    # 合并旧审计信息（unit 兜底 + source/confidence 进 notes；仅新行，重跑不重复追加）
    for r in fresh:
        olds = old_rows.get(r["parameter"])
        if not olds:
            continue
        if not r["unit"]:
            r["unit"] = olds[0]["unit"]
            r["notes"] += ("；" if r["notes"] else "") + "unit 沿用旧审计表"
        info = " ｜ ".join("source=%s; confidence=%s" % (o["source"], o["confidence"])
                           for o in olds)
        r["notes"] += ("；" if r["notes"] else "") + "旧审计: " + info

    # ---- 幂等合并：人工列保留原值，脚本列更新 ----
    final = []
    for r in fresh:
        key = (r["parameter"], r["module"])
        if key in existing:
            old = existing.pop(key)
            for col, idx in MANUAL_IDX.items():
                r[col] = old[idx]
        final.append(r)
    preserved = 0
    for key, old in existing.items():  # 仅存在于既有 CSV 的行：原样保留（用户回填永不丢失）
        preserved += 1
        sys.stderr.write("[build_parameter_table] 提示：仅存在于既有 CSV 的行原样保留：%s\n" % (key,))
        final.append(dict(zip(EXPECTED_HEADER, old)))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(EXPECTED_HEADER)
        for r in final:
            w.writerow([r[c] for c in EXPECTED_HEADER])

    # ---- 摘要与数量级对照报警 ----
    pending = sum(1 for r in final if r["source_class5"] == "pending")
    sys.stderr.write(
        "[build_parameter_table] 完成：输出 %d 行（解析 %d：常量 %d / 派生 %d / 疑似 %d；"
        "既有保留 %d；pending %d；旧审计 %d 行）→ %s\n"
        % (len(final), stats["parsed"], stats["constant"], stats["derived"],
           stats["suspect"], preserved, pending, old_count, out_path))
    if old_count and not (old_count / 3 <= len(final) <= old_count * 3):
        sys.stderr.write("[build_parameter_table] 警告：新表行数 %d 与旧审计 %d 行数量级不符，请人工复核\n"
                         % (len(final), old_count))


if __name__ == "__main__":
    main()