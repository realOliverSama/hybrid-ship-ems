#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gate_a1_completeness.py — Phase 4-A1 完整性 Gate（FR-407）。

校验（design §4.4，缺项写入 stderr 并计入 gate_result_a1.json）：
  G1 subsystem 覆盖：architecture_audit.json 的 subsystem_inventory 非空、
     每项 disposition∈{KEEP,REFINE,REPLACE,ADD,REJECT}、physical_fidelity∈{高,中,低}、
     realtime_cost∈{低,中,高}、rationale 非空、name 无重复，且
     name 集合 == builder_subsystem_list 集合（100% 覆盖）；
  G2 ADD 清单：additions.category 覆盖 {shallow_water_ukc, dual_genset,
     genset_startstop, battery_ecm, battery_sop}；
  G3 螺旋桨结论：propeller_decision.choice ∈ {keep_simplified, upgrade_ktkq}
     且 basis 非空；
  G4 参数分类：modelE_parameter_audit.csv 每行 source_class5 ∈
     {drawing,vendor,literature,estimated,assumed,pending}、=pending 时 notes 非空、
     impact ∈ {high,medium,low}、无空 parameter；
  G5 报告存在：modelE_architecture_audit.md / modelE_parameter_audit.md 存在且非空。

输出：同目录 gate_result_a1.json = {"overall": bool, "checks": {...}, "timestamp": ...}
overall=false 时退出码非零。

用法：python3 gate_a1_completeness.py [base_dir]   # 缺省为本脚本上级目录
"""
import csv
import json
import sys
from datetime import datetime
from pathlib import Path

CSV_HEADER = [
    "parameter", "current_value", "unit", "module", "legacy_caliber",
    "source_class5", "impact", "user_value", "user_reference", "notes",
]
DISPOSITIONS = {"KEEP", "REFINE", "REPLACE", "ADD", "REJECT"}
FIDELITY = {"高", "中", "低"}
RTCOST = {"低", "中", "高"}
CLASS5 = {"drawing", "vendor", "literature", "estimated", "assumed", "pending"}
IMPACT = {"high", "medium", "low"}
REQUIRED_CATEGORIES = {"shallow_water_ukc", "dual_genset", "genset_startstop",
                       "battery_ecm", "battery_sop"}
PROPELLER_CHOICES = {"keep_simplified", "upgrade_ktkq"}


def nonempty_str(v):
    return isinstance(v, str) and v.strip() != ""


def check_g1(data, fail_common):
    failures = list(fail_common)
    if not fail_common and data is not None:
        inv = data.get("subsystem_inventory")
        builders = data.get("builder_subsystem_list")
        if not isinstance(inv, list) or not inv:
            failures.append("subsystem_inventory 非空清单缺失或为空")
            inv = []
        if not isinstance(builders, list) or not builders:
            failures.append("builder_subsystem_list 非空清单缺失或为空")
            builders = []
        names, dup = set(), set()
        for idx, it in enumerate(inv):
            if not isinstance(it, dict):
                failures.append("subsystem_inventory[%d] 不是对象" % idx)
                continue
            name = it.get("name")
            if not nonempty_str(name):
                failures.append("subsystem_inventory[%d] 缺少非空 name" % idx)
                continue
            if name in names:
                dup.add(name)
            names.add(name)
            if it.get("disposition") not in DISPOSITIONS:
                failures.append("subsystem '%s'：disposition '%s' 非法（五选一 KEEP/REFINE/REPLACE/ADD/REJECT）"
                                % (name, it.get("disposition")))
            if it.get("physical_fidelity") not in FIDELITY:
                failures.append("subsystem '%s'：physical_fidelity '%s' 非法（三级 高/中/低）"
                                % (name, it.get("physical_fidelity")))
            if it.get("realtime_cost") not in RTCOST:
                failures.append("subsystem '%s'：realtime_cost '%s' 非法（三级 低/中/高）"
                                % (name, it.get("realtime_cost")))
            if not nonempty_str(it.get("rationale")):
                failures.append("subsystem '%s'：rationale 为空" % name)
        if dup:
            failures.append("subsystem_inventory 存在重复 name：%s" % sorted(dup))
        bset = set(x for x in builders if isinstance(x, str))
        for miss in sorted(bset - names):
            failures.append("subsystem_inventory 缺少 builder 清点条目：%s" % miss)
        for extra in sorted(names - bset):
            failures.append("builder_subsystem_list 缺少 inventory 条目：%s" % extra)
    return {"pass": not failures, "failures": failures}


def check_g2(data, fail_common):
    failures = list(fail_common)
    if not fail_common and data is not None:
        adds = data.get("additions")
        if not isinstance(adds, list):
            failures.append("additions 清单缺失或不是数组")
            adds = []
        cats = set()
        for idx, it in enumerate(adds):
            if isinstance(it, dict) and nonempty_str(it.get("category")):
                cats.add(it["category"])
            else:
                failures.append("additions[%d] 缺少非空 category" % idx)
        for miss in sorted(REQUIRED_CATEGORIES - cats):
            failures.append("additions 未覆盖必需类别：%s" % miss)
    return {"pass": not failures, "failures": failures}


def check_g3(data, fail_common):
    failures = list(fail_common)
    if not fail_common and data is not None:
        pd = data.get("propeller_decision")
        if not isinstance(pd, dict):
            failures.append("propeller_decision 缺失或不是对象")
        else:
            if pd.get("choice") not in PROPELLER_CHOICES:
                failures.append("propeller_decision.choice '%s' 非法（二选一 keep_simplified/upgrade_ktkq）"
                                % pd.get("choice"))
            if not nonempty_str(pd.get("basis")):
                failures.append("propeller_decision.basis 为空")
    return {"pass": not failures, "failures": failures}


def check_g4(csv_path):
    failures = []
    try:
        with csv_path.open(encoding="utf-8-sig", newline="") as f:
            rdr = csv.DictReader(f)
            if rdr.fieldnames != CSV_HEADER:
                failures.append("CSV 表头漂移：%s（期望十列 %s）" % (rdr.fieldnames, CSV_HEADER))
                return {"pass": False, "failures": failures}
            shown = 0
            total_bad = 0
            for i, row in enumerate(rdr, 2):  # 数据行从第 2 行起
                bad = []
                if not nonempty_str(row.get("parameter")):
                    bad.append("parameter 为空")
                s5 = (row.get("source_class5") or "").strip()
                if s5 not in CLASS5:
                    bad.append("source_class5 '%s' 非法（六值枚举）" % s5)
                elif s5 == "pending" and not nonempty_str(row.get("notes")):
                    bad.append("source_class5=pending 但 notes 为空")
                imp = (row.get("impact") or "").strip()
                if imp not in IMPACT:
                    bad.append("impact '%s' 非法（high/medium/low）" % imp)
                if bad:
                    total_bad += 1
                    if shown < 20:
                        failures.append("CSV 第 %d 行（parameter=%r）：%s" % (i, row.get("parameter"), "；".join(bad)))
                        shown += 1
            if total_bad > shown:
                failures.append("…另有 %d 行同类问题未展开" % (total_bad - shown))
    except FileNotFoundError:
        failures.append("参数总表 CSV 不存在：%s" % csv_path.name)
    return {"pass": not failures, "failures": failures}


def check_g5(base):
    failures = []
    for name in ("modelE_architecture_audit.md", "modelE_parameter_audit.md"):
        p = base / name
        if not p.exists():
            failures.append("%s 不存在（先运行 render_audit_reports.py）" % name)
        elif p.stat().st_size == 0:
            failures.append("%s 存在但为空" % name)
    return {"pass": not failures, "failures": failures}


def main(argv):
    base = Path(argv[1]).resolve() if len(argv) > 1 else Path(__file__).resolve().parent.parent
    json_path = base / "architecture_audit.json"

    data = None
    fail_common = []
    try:
        data = json.loads(json_path.read_text(encoding="utf-8-sig"))
    except FileNotFoundError:
        fail_common.append("architecture_audit.json 不存在（主 Agent 手工撰写后重跑）")
    except (json.JSONDecodeError, UnicodeDecodeError) as e:
        fail_common.append("architecture_audit.json 解析失败：%s" % e)

    checks = {
        "G1": check_g1(data, fail_common),
        "G2": check_g2(data, fail_common),
        "G3": check_g3(data, fail_common),
        "G4": check_g4(base / "modelE_parameter_audit.csv"),
        "G5": check_g5(base),
    }
    overall = all(c["pass"] for c in checks.values())
    result = {"overall": overall, "checks": checks,
              "timestamp": datetime.now().isoformat(timespec="seconds")}
    out = base / "gate_result_a1.json"
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    ok = True
    for cid in sorted(checks):
        c = checks[cid]
        if c["pass"]:
            sys.stderr.write("[gate_a1_completeness] %s PASS\n" % cid)
        else:
            ok = False
            sys.stderr.write("[gate_a1_completeness] %s FAIL（缺项 %d）\n" % (cid, len(c["failures"])))
            for msg in c["failures"]:
                sys.stderr.write("  - %s\n" % msg)
    sys.stderr.write("[gate_a1_completeness] overall=%s → %s\n" % (overall, out))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main(sys.argv)