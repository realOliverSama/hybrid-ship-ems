#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""render_audit_reports.py — Phase 4-A1 JSON/CSV → 两份 Markdown 报告（幂等）。

输入（位于本脚本上级目录 docs/phase4/a1_model_audit/）：
  architecture_audit.json        架构审计单一事实源（手工撰写）
  modelE_parameter_audit.csv     参数总表（build_parameter_table.py 生成 + 人工复核）
输出（同目录，幂等可重跑）：
  modelE_architecture_audit.md   架构审计报告
  modelE_parameter_audit.md      参数审计报告（摘要 + 手工"假设影响分析"节）

保留块机制：`<!-- MANUAL:<名称> -->` … `<!-- /MANUAL:<名称> -->` 之间的内容
在重跑时原样保留（手工内容只存在于保留块中，design §2）。
  - modelE_architecture_audit.md 保留块：zero_change_evidence（附录：Model D 零改动证据）
  - modelE_parameter_audit.md   保留块：assumption_impact_analysis（假设影响分析）
目标文件不存在时创建含空占位保留块的骨架；已存在但保留块缺失时报错退出（不覆盖）。

Spec: .specs/phase4-a1-model-e-audit/design.md §1/§2/§4
"""
import csv
import json
import re
import sys
from pathlib import Path

CSV_HEADER = [
    "parameter", "current_value", "unit", "module", "legacy_caliber",
    "source_class5", "impact", "user_value", "user_reference", "notes",
]

ARCH_MD = "modelE_architecture_audit.md"
PARAM_MD = "modelE_parameter_audit.md"

ARCH_BLOCKS = [("zero_change_evidence", "## 附录：Model D 零改动证据")]
PARAM_BLOCKS = [("assumption_impact_analysis", "## 假设影响分析（重点假设清单）")]

ARCH_PLACEHOLDER = (
    "（占位）待主 Agent 填入：任务开始/结束两次 `git status --short model/` 记录"
    "与关键文件（Model_Parameters.m / Model_Scenarios.m / builder / .slx）sha256 对照表。\n")
PARAM_PLACEHOLDER = (
    "（占位）待主 Agent 填写\"假设影响分析\"（FR-406）：至少覆盖船体阻力、排水量、吃水、"
    "水深、螺旋桨参数、电机效率、发电机最低稳定负荷、发电机启停过程、电池内阻/OCV、"
    "辅机负荷十类重点假设，并给出\"最可能影响论文结论\"的明确清单。\n")


def die(msg, code=1):
    sys.stderr.write("[render_audit_reports] 错误：%s\n" % msg)
    sys.exit(code)


def esc(text):
    """Markdown 表格单元格转义。"""
    if text is None:
        return ""
    return re.sub(r"\s*\n\s*", " ", str(text)).replace("|", "\\|").strip()


def cell(v):
    return esc(v) if v not in (None, "") else "—"


def extract_blocks(text, names):
    """从既有 MD 提取全部保留块；任一缺失返回 None（调用方报错）。"""
    blocks = {}
    for name in names:
        m = re.search(
            r"(?ms)^[ \t]*<!--\s*MANUAL:%s\s*-->[ \t]*\n(.*?)^[ \t]*<!--\s*/MANUAL:%s\s*-->[ \t]*$"
            % (re.escape(name), re.escape(name)),
            text)
        if not m:
            return None
        blocks[name] = m.group(1).rstrip("\n")
    return blocks


def render_block(name, heading, content):
    return "%s\n<!-- MANUAL:%s -->\n%s\n<!-- /MANUAL:%s -->\n" % (heading, name, content, name)


def load_manual(path, names, placeholders):
    """目标存在：提取保留块（缺块报错）；不存在：用占位内容。"""
    if path.exists():
        blocks = extract_blocks(path.read_text(encoding="utf-8-sig"),
                                [n for n, _ in names])
        if blocks is None:
            die("%s 已存在但缺少保留块 %s（不覆盖，请先恢复块结构）"
                % (path.name, [n for n, _ in names]))
        return blocks
    return {name: placeholders[name] for name, _ in names}


def render_architecture(data, blocks):
    meta = data.get("meta") or {}
    lines = []
    lines.append("# Model E 架构审计报告（Phase 4-A1）\n")
    lines.append("> 本文件由 `scripts/render_audit_reports.py` 从 `architecture_audit.json` 渲染，"
                 "幂等可重跑；手工内容仅存在于附录保留块（`<!-- MANUAL:... -->`）。\n")

    lines.append("## 元信息\n")
    lines.append("| 字段 | 值 |")
    lines.append("|---|---|")
    for k in ("spec", "date", "model_under_audit"):
        lines.append("| %s | %s |" % (k, cell(meta.get(k, ""))))
    lines.append("")

    builders = data.get("builder_subsystem_list") or []
    lines.append("## Subsystem 清单（FR-401）\n")
    lines.append("builder 清点（`builder_subsystem_list`，%d 项）：%s\n"
                 % (len(builders), "、".join("`%s`" % b for b in builders)))
    lines.append("| name | builder_ref | summary | disposition | physical_fidelity | realtime_cost | rationale |")
    lines.append("|---|---|---|---|---|---|---|")
    for it in data.get("subsystem_inventory") or []:
        lines.append("| %s | %s | %s | %s | %s | %s | %s |" % (
            cell(it.get("name")), cell(it.get("builder_ref")), cell(it.get("summary")),
            cell(it.get("disposition")), cell(it.get("physical_fidelity")),
            cell(it.get("realtime_cost")), cell(it.get("rationale"))))
    lines.append("")

    lines.append("## ADD 清单（FR-402，Model E 新增模块）\n")
    lines.append("| name | category | summary | physical_fidelity | realtime_cost | rationale |")
    lines.append("|---|---|---|---|---|---|")
    for it in data.get("additions") or []:
        lines.append("| %s | %s | %s | %s | %s | %s |" % (
            cell(it.get("name")), cell(it.get("category")), cell(it.get("summary")),
            cell(it.get("physical_fidelity")), cell(it.get("realtime_cost")),
            cell(it.get("rationale"))))
    lines.append("")

    pd = data.get("propeller_decision") or {}
    lines.append("## 螺旋桨分支结论（FR-403）\n")
    lines.append("- **choice**: `%s`" % cell(pd.get("choice", "")))
    lines.append("- **basis**: %s" % cell(pd.get("basis", "")))
    lines.append("- **a2_consequence**: %s" % cell(pd.get("a2_consequence", "")))
    lines.append("")

    lines.append("## 发现（findings，不修复，零改动纪律）\n")
    findings = data.get("findings") or []
    if findings:
        for i, f in enumerate(findings, 1):
            lines.append("%d. %s" % (i, esc(f)))
    else:
        lines.append("（无）")
    lines.append("")
    return "\n".join(lines)


def render_parameter(csv_path, blocks):
    with csv_path.open(encoding="utf-8-sig", newline="") as f:
        rdr = csv.DictReader(f)
        if rdr.fieldnames != CSV_HEADER:
            die("参数总表 CSV 表头漂移：%s\n期望：%s" % (rdr.fieldnames, CSV_HEADER))
        rows = list(rdr)

    def count_by(col, order):
        cnt = {}
        for r in rows:
            cnt[r[col]] = cnt.get(r[col], 0) + 1
        seen = set()
        out = []
        for k in list(order) + sorted(set(cnt) - set(order)):
            if k in seen:
                continue
            seen.add(k)
            out.append((k, cnt.get(k, 0)))
        return out

    lines = []
    lines.append("# Model E 参数审计报告（Phase 4-A1）\n")
    lines.append("> 本文件由 `scripts/render_audit_reports.py` 从 `modelE_parameter_audit.csv` 渲染，"
                 "幂等可重跑；手工内容仅存在于\"假设影响分析\"保留块（`<!-- MANUAL:... -->`）。\n")
    lines.append("## 统计摘要\n")
    lines.append("参数总表共 **%d 行**（按 (parameter, module) 逐行登记，多处定义不合并）；"
                 "事实源：`modelE_parameter_audit.csv`（脚本列由 build_parameter_table.py 生成，"
                 "人工列由复核/回填维护）。\n" % len(rows))
    lines.append("### 按 source_class5（五级来源分类）\n")
    lines.append("| source_class5 | 行数 |")
    lines.append("|---|---|")
    for k, c in count_by("source_class5", ["drawing", "vendor", "literature",
                                           "estimated", "assumed", "pending"]):
        lines.append("| %s | %d |" % (k, c))
    lines.append("")
    lines.append("### 按 impact（对论文结论的影响，启发式预填 + 人工复核）\n")
    lines.append("| impact | 行数 |")
    lines.append("|---|---|")
    for k, c in count_by("impact", ["high", "medium", "low"]):
        lines.append("| %s | %d |" % (k, c))
    lines.append("")
    lines.append("### 按 legacy_caliber（现行口径；UNKNOWN = 无法判定）\n")
    lines.append("| legacy_caliber | 行数 |")
    lines.append("|---|---|")
    for k, c in count_by("legacy_caliber", ["DRAWING", "LEGACY", "ASSUMED", "UNKNOWN"]):
        lines.append("| %s | %d |" % (k, c))
    lines.append("")

    lines.append("## 五级来源默认映射规则（FR-405）\n")
    lines.append("映射为**默认值**，人工复核可对具体行 override 并写明理由（记录于报告）。\n")
    lines.append("| 现行口径（legacy_caliber） | 默认 source_class5 | 说明 |")
    lines.append("|---|---|---|")
    lines.append("| DRAWING | drawing | v2.2 起部分\"真实设备参数\"人工复核可升/改为 vendor |")
    lines.append("| LEGACY | estimated | 备注\"继承原模型 HybridShip0730，原始依据待追溯\"；查到依据后升级 |")
    lines.append("| ASSUMED | assumed | — |")
    lines.append("| UNKNOWN（无法判定） | pending | 备注必填（Gate 校验），不得猜测归类 |")
    lines.append("")
    return "\n".join(lines)


def main():
    base = Path(__file__).resolve().parent.parent  # docs/phase4/a1_model_audit/
    json_path = base / "architecture_audit.json"
    csv_path = base / "modelE_parameter_audit.csv"
    if not json_path.exists():
        die("输入缺失：%s（由主 Agent 手工撰写）" % json_path)
    if not csv_path.exists():
        die("输入缺失：%s（先运行 build_parameter_table.py）" % csv_path)
    try:
        data = json.loads(json_path.read_text(encoding="utf-8-sig"))
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        die("architecture_audit.json 解析失败：%s" % e)
    for key in ("meta", "builder_subsystem_list", "subsystem_inventory",
                "additions", "propeller_decision", "findings"):
        if key not in data:
            die("architecture_audit.json 缺少必需字段：%s" % key)

    arch_blocks = load_manual(base / ARCH_MD, ARCH_BLOCKS,
                              {"zero_change_evidence": ARCH_PLACEHOLDER})
    param_blocks = load_manual(base / PARAM_MD, PARAM_BLOCKS,
                               {"assumption_impact_analysis": PARAM_PLACEHOLDER})

    arch_text = render_architecture(data, arch_blocks)
    parts = [arch_text]
    for name, heading in ARCH_BLOCKS:
        parts.append(render_block(name, heading, arch_blocks[name]))
    (base / ARCH_MD).write_text("\n".join(parts), encoding="utf-8")

    param_text = render_parameter(csv_path, param_blocks)
    parts = [param_text]
    for name, heading in PARAM_BLOCKS:
        parts.append(render_block(name, heading, param_blocks[name]))
    (base / PARAM_MD).write_text("\n".join(parts), encoding="utf-8")

    sys.stderr.write("[render_audit_reports] 完成：%s、%s（幂等；保留块 %s）\n"
                     % (ARCH_MD, PARAM_MD,
                        "zero_change_evidence/assumption_impact_analysis"))


if __name__ == "__main__":
    main()