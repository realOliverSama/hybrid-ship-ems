#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""validate_contracts.py — Dataset v3 机器契约校验器（FR-465/T-001/T-003）.

执行层契约的唯一强制入口：
  - signal/role registry（config/dataset_v3_signal_registry.json）
  - C-producer interface schema（config/dataset_v3_producer_schema.json）
  - forecast provenance 六字段记录（含 source_cutoff_time_s ≤ issue_time_s 不变量）

用法（仓库相对，禁绝对路径；从 MODELE_ROOT 运行）:
  python -m dataset_v3.validate_contracts --registry
  python -m dataset_v3.validate_contracts --producer-schema
  python -m dataset_v3.validate_contracts --registry --write-hash     # 重算并写回 self_hash
  python -m dataset_v3.validate_contracts --forecast-provenance <json> --stage pilot|formal

校验 FAIL 时打印全部违规并 exit 1（fail-fast，契约错误不降级为警告——design Error Handling）。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
REGISTRY_PATH = os.path.join(MODELE_ROOT, "config", "dataset_v3_signal_registry.json")
PRODUCER_SCHEMA_PATH = os.path.join(MODELE_ROOT, "config", "dataset_v3_producer_schema.json")

REGISTRY_SCHEMA = "dataset_v3_signal_registry_v1"
PRODUCER_SCHEMA_ID = "dataset_v3_producer_schema_v1"

# ---- 冻结枚举（design DGN-02 + R1；与 registry enums 节双向一致） ----
ROLE_ENUM = ("INPUT_H1", "INPUT_H2", "INPUT_K", "INPUT_F",
             "TARGET", "ORACLE", "METADATA", "FORBIDDEN")
TEMPORAL_ENUM = ("history", "future", "both", "none")
AGGREGATION_ENUM = ("time_mean", "interval_endpoint", "last_mode", "interval_any")
ISSUE_ENUM = ("none", "issue_snapshot")
ACCESS_ENUM = ("default", "extended", "oracle_only", "forbidden", "metadata_only")
ROLE_ACCESS_BINDING = {
    "INPUT_H1": "default", "INPUT_H2": "extended", "INPUT_K": "default",
    "INPUT_F": "default", "TARGET": "forbidden", "ORACLE": "oracle_only",
    "METADATA": "metadata_only", "FORBIDDEN": "forbidden",
}
SOURCE_PREFIXES = ("logsout:", "scenario:", "fixture:")

# ---- FR-461 冻结清单（requirements 唯一事实源；validator 为其忠实性锚点） ----
FR461_H1_NAMES = frozenset({
    "P_load_total_kW", "v_ship_mps", "acceleration_mps2", "n_prop_rpm",
    "P_hotel_kW", "v_current_mps", "water_depth_m", "P_prop_demand_kW",
    "ctrl_mode", "v_ref_mps", "manual_power_cmd_pct",
})
FR461_H2_NAMES = frozenset({
    "P_gen_actual_kW", "P_bat_actual_kW", "SOC", "V_dc_V", "state_DG1",
    "state_DG2", "V_bat_V", "I_bat_A", "P_bat_dis_avail_kW", "P_bat_chg_avail_kW",
})

SIGNAL_FIELDS = ("name", "unit", "role", "source_signal", "temporal_domain",
                 "aggregation", "issue_time_semantics", "predictor_access")

# ---- C-producer 11 项能力族（FR-495/DGN-03） ----
PRODUCER_CAPABILITY_FAMILIES = (
    "truth_stream", "s_plan", "planned_future_trajectory", "static_constraints",
    "input_k_snapshots", "input_f_snapshots", "forecast_provenance",
    "oracle_counterpart", "voyage_metadata", "generation_provenance",
    "ood_config_surface",
)
PRODUCER_FIELD_FIELDS = ("name", "type", "shape_semantics", "required", "causality_rule")

# ---- forecast provenance 六字段（design R6） ----
PROVENANCE_FIELDS = ("forecast_generator_id", "forecast_generator_version",
                     "forecast_generator_hash", "issue_time_s",
                     "source_cutoff_time_s", "forecast_provenance_class")
PROVENANCE_CLASS_PILOT = ("CAUSAL_GENERATOR", "FIXTURE_CAUSAL_STUB")
PROVENANCE_CLASS_FORMAL = ("CAUSAL_GENERATOR",)
PROVENANCE_CLASS_FORBIDDEN = ("ORACLE_DERIVED",)


def canonical_json(obj) -> bytes:
    """契约内容规范化（self-hash 输入）：sort_keys + 紧凑分隔符 + utf-8。"""
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def compute_self_hash(reg: dict) -> str:
    """SHA256(canonicalized content 排除 self_hash 字段)（T-001 冻结算法）。"""
    body = {k: v for k, v in reg.items() if k != "self_hash"}
    return hashlib.sha256(canonical_json(body)).hexdigest()


def _load_json(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def validate_registry(reg: dict) -> list:
    """registry 全量校验，返回违规清单（空=PASS）。"""
    errs = []

    def err(msg):
        errs.append(msg)

    if reg.get("schema") != REGISTRY_SCHEMA:
        err(f"schema 字段必须为 {REGISTRY_SCHEMA}，实际 {reg.get('schema')!r}")
    if not isinstance(reg.get("signals"), list) or not reg["signals"]:
        err("signals 必须为非空数组")
        return errs

    # 枚举自声明与 validator 冻结副本一致（双向防漂移）
    enums = reg.get("enums", {})
    frozen = {"role": ROLE_ENUM, "temporal_domain": TEMPORAL_ENUM,
              "aggregation": AGGREGATION_ENUM, "issue_time_semantics": ISSUE_ENUM,
              "predictor_access": ACCESS_ENUM}
    for key, ref in frozen.items():
        got = enums.get(key)
        if list(got or []) != list(ref):
            err(f"enums.{key} 与冻结枚举不一致: {got!r} != {list(ref)}")
    if reg.get("role_predictor_access_binding") != ROLE_ACCESS_BINDING:
        err("role_predictor_access_binding 与冻结绑定表不一致")

    seen_pairs = set()
    names_by_role: dict[str, set] = {}
    for i, sig in enumerate(reg["signals"]):
        tag = f"signals[{i}]"
        if not isinstance(sig, dict):
            err(f"{tag} 条目必须为对象，实际 {type(sig).__name__}")
            continue
        for fld in SIGNAL_FIELDS:
            if fld not in sig:
                err(f"{tag} 缺字段 {fld}")
        name = sig.get("name")
        role = sig.get("role")
        if not isinstance(name, str) or not name:
            err(f"{tag} name 非法")
            continue
        tag = f"signals[{i}]({name})"
        pair = (name, role)
        if pair in seen_pairs:
            err(f"{tag} (name,role) 重复: {pair}")
        seen_pairs.add(pair)
        if role not in ROLE_ENUM:
            err(f"{tag} role 非法: {role!r}")
            continue
        names_by_role.setdefault(role, set()).add(name)
        for fld, ref in (("temporal_domain", TEMPORAL_ENUM),
                         ("aggregation", AGGREGATION_ENUM),
                         ("issue_time_semantics", ISSUE_ENUM),
                         ("predictor_access", ACCESS_ENUM)):
            if sig.get(fld) not in ref:
                err(f"{tag} {fld} 非法: {sig.get(fld)!r}")
        # role → predictor_access 绑定
        if role in ROLE_ACCESS_BINDING and sig.get("predictor_access") != ROLE_ACCESS_BINDING[role]:
            err(f"{tag} predictor_access={sig.get('predictor_access')!r} 违反 role 绑定（应为 {ROLE_ACCESS_BINDING[role]}）")
        # source_signal 前缀
        src = sig.get("source_signal")
        if not isinstance(src, str) or not any(src.startswith(p) for p in SOURCE_PREFIXES):
            err(f"{tag} source_signal 前缀非法: {src!r}")
        elif src.startswith("fixture:") and role not in ("INPUT_K", "INPUT_F"):
            err(f"{tag} fixture 来源仅允许 INPUT_K/INPUT_F，实际 role={role}")
        # issue-time 约束：K/F 必须 issue_snapshot，其余必须 none
        if role in ("INPUT_K", "INPUT_F"):
            if sig.get("issue_time_semantics") != "issue_snapshot":
                err(f"{tag} K/F 必须 issue_time_semantics=issue_snapshot")
            if sig.get("temporal_domain") != "future":
                err(f"{tag} K/F 必须 temporal_domain=future")
        elif sig.get("issue_time_semantics") != "none":
            err(f"{tag} 非K/F 必须 issue_time_semantics=none")

    # FR-461 H1/H2 清单一致性（集合相等，双向）
    got_h1 = names_by_role.get("INPUT_H1", set())
    if got_h1 != FR461_H1_NAMES:
        err(f"INPUT_H1 集合与 FR-461 不一致: 缺={sorted(FR461_H1_NAMES - got_h1)} 多={sorted(got_h1 - FR461_H1_NAMES)}")
    got_h2 = names_by_role.get("INPUT_H2", set())
    if got_h2 != FR461_H2_NAMES:
        err(f"INPUT_H2 集合与 FR-461 不一致: 缺={sorted(FR461_H2_NAMES - got_h2)} 多={sorted(got_h2 - FR461_H2_NAMES)}")

    # TARGET 派生源一致性：TARGET 条目 source_signal 必须与同名 INPUT_H1 条目一致
    h1_src = {s["name"]: s.get("source_signal") for s in reg["signals"] if s.get("role") == "INPUT_H1"}
    for s in reg["signals"]:
        if s.get("role") == "TARGET":
            if h1_src.get(s.get("name")) != s.get("source_signal"):
                err(f"TARGET 条目 {s.get('name')} source_signal 与 INPUT_H1 同名条目不一致")

    # self_hash（存在时校验；PENDING 视为未写入——仅在 --write-hash 时允许）
    sh = reg.get("self_hash")
    if not isinstance(sh, dict) or sh.get("algorithm") != "sha256":
        err("self_hash.algorithm 必须为 sha256")
    else:
        want = compute_self_hash(reg)
        if sh.get("value") != want:
            if sh.get("value") == "PENDING":
                err(f"self_hash 未写入（PENDING）；运行 --write-hash 写入（应为 {want[:16]}…）")
            else:
                err(f"self_hash 不匹配: 文件={sh.get('value')} 计算={want}")
    return errs


def validate_producer_schema(schema: dict) -> list:
    """C-producer interface schema 校验（FR-495/DGN-03），返回违规清单。"""
    errs = []
    if schema.get("schema") != PRODUCER_SCHEMA_ID:
        errs.append(f"schema 字段必须为 {PRODUCER_SCHEMA_ID}，实际 {schema.get('schema')!r}")
    fams = schema.get("capability_families", {})
    for fam in PRODUCER_CAPABILITY_FAMILIES:
        if fam not in fams:
            errs.append(f"缺能力族 {fam}（FR-495 十一项）")
    for fam, spec in fams.items():
        if fam not in PRODUCER_CAPABILITY_FAMILIES:
            errs.append(f"未知能力族 {fam}")
            continue
        fields = spec.get("fields") if isinstance(spec, dict) else None
        if not isinstance(fields, list) or not fields:
            errs.append(f"能力族 {fam} fields 必须为非空数组")
            continue
        for j, fld in enumerate(fields):
            tag = f"{fam}.fields[{j}]"
            for rf in PRODUCER_FIELD_FIELDS:
                if rf not in fld:
                    errs.append(f"{tag} 缺字段 {rf}")
            if fld.get("required") not in (True, False):
                errs.append(f"{tag} required 必须为布尔")
            if not isinstance(fld.get("causality_rule"), str) or not fld.get("causality_rule"):
                errs.append(f"{tag} causality_rule 必须为非空字符串")
    # forecast provenance 六字段必须出现在 forecast_provenance 族
    fp = fams.get("forecast_provenance", {})
    names = {f.get("name") for f in fp.get("fields", [])} if isinstance(fp, dict) else set()
    for pf in PROVENANCE_FIELDS:
        if pf not in names:
            errs.append(f"forecast_provenance 缺六字段之一: {pf}")
    # self_hash（存在时校验；PENDING 视为未写入——仅在 --write-hash 时允许）
    sh = schema.get("self_hash")
    if not isinstance(sh, dict) or sh.get("algorithm") != "sha256":
        errs.append("self_hash.algorithm 必须为 sha256")
    else:
        body = {k: v for k, v in schema.items() if k != "self_hash"}
        want = hashlib.sha256(canonical_json(body)).hexdigest()
        if sh.get("value") != want:
            if sh.get("value") == "PENDING":
                errs.append(f"self_hash 未写入（PENDING）；运行 --write-hash 写入（应为 {want[:16]}…）")
            else:
                errs.append(f"self_hash 不匹配: 文件={sh.get('value')} 计算={want}")
    return errs


def validate_forecast_provenance(rec: dict, stage: str) -> list:
    """单条 forecast provenance 记录校验（design R6）。

    - 六字段必须齐备且类型正确；
    - 不变量 source_cutoff_time_s ≤ issue_time_s；
    - provenance class 白名单：formal 仅 CAUSAL_GENERATOR；pilot 另允许 FIXTURE_CAUSAL_STUB；
    - ORACLE_DERIVED 永远拒收（FR-467：禁止未来真值后处理冒充预报）。
    """
    errs = []
    if stage not in ("pilot", "formal"):
        return [f"stage 非法: {stage!r}（须 pilot|formal）"]
    for pf in PROVENANCE_FIELDS:
        if pf not in rec:
            errs.append(f"provenance 缺字段 {pf}")
    if errs:
        return errs
    it, ct = rec["issue_time_s"], rec["source_cutoff_time_s"]
    if isinstance(it, bool) or isinstance(ct, bool):
        errs.append("issue_time_s / source_cutoff_time_s 不得为布尔")
    elif not isinstance(it, (int, float)) or not isinstance(ct, (int, float)):
        errs.append("issue_time_s / source_cutoff_time_s 必须为数值")
    elif ct > it:
        errs.append(f"不变量违反: source_cutoff_time_s({ct}) > issue_time_s({it})")
    cls = rec["forecast_provenance_class"]
    if cls in PROVENANCE_CLASS_FORBIDDEN:
        errs.append(f"provenance class 拒收: {cls}（ORACLE-derived 不得作为 forecast）")
    elif stage == "formal" and cls not in PROVENANCE_CLASS_FORMAL:
        errs.append(f"formal 阶段仅允许 {PROVENANCE_CLASS_FORMAL}，实际 {cls!r}")
    elif stage == "pilot" and cls not in PROVENANCE_CLASS_PILOT:
        errs.append(f"pilot 阶段允许 {PROVENANCE_CLASS_PILOT}，实际 {cls!r}")
    for sf in ("forecast_generator_id", "forecast_generator_version", "forecast_generator_hash"):
        if not isinstance(rec.get(sf), str) or not rec.get(sf):
            errs.append(f"{sf} 必须为非空字符串")
    return errs


def write_self_hash(path: str) -> str:
    """重算 self_hash 并写回文件（保持 2 空格缩进、ensure_ascii=False）。"""
    reg = _load_json(path)
    reg.setdefault("self_hash", {})
    reg["self_hash"]["algorithm"] = "sha256"
    reg["self_hash"]["value"] = compute_self_hash(reg)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(reg, f, ensure_ascii=False, indent=2)
        f.write("\n")
    return reg["self_hash"]["value"]


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Dataset v3 机器契约校验器（fail-fast）")
    ap.add_argument("--registry", action="store_true", help="校验 signal/role registry")
    ap.add_argument("--producer-schema", action="store_true", help="校验 C-producer interface schema")
    ap.add_argument("--forecast-provenance", metavar="JSON", help="校验单条 forecast provenance 记录")
    ap.add_argument("--stage", choices=("pilot", "formal"), default="pilot")
    ap.add_argument("--write-hash", action="store_true",
                    help="重算 registry self_hash 并写回（其余校验先行，全过才写）")
    args = ap.parse_args(argv)
    if not (args.registry or args.producer_schema or args.forecast_provenance):
        ap.error("至少指定 --registry / --producer-schema / --forecast-provenance 之一")

    failed = False
    if args.registry:
        reg = _load_json(REGISTRY_PATH)
        errs = validate_registry(reg)
        if args.write_hash:
            # 仅允许 self_hash 相关项（未写入/内容变更后的预期不匹配）时写 hash；
            # 其余任何违规拒绝写（fail-fast）
            blocking = [e for e in errs
                        if not e.startswith("self_hash 未写入")
                        and not e.startswith("self_hash 不匹配")]
            if blocking:
                print("[registry] 校验未过，拒绝写入 self_hash", file=sys.stderr)
                for e in blocking:
                    print(f"  - {e}", file=sys.stderr)
                failed = True
            else:
                val = write_self_hash(REGISTRY_PATH)
                print(f"[registry] self_hash 已写入: {val}")
                reg = _load_json(REGISTRY_PATH)
                errs = validate_registry(reg)
        if errs:
            failed = True
            print(f"[registry] FAIL（{len(errs)} 项违规）:")
            for e in errs:
                print(f"  - {e}")
        else:
            print(f"[registry] PASS（{len(reg['signals'])} 条信号，"
                  f"self_hash={reg.get('self_hash', {}).get('value', '')[:16]}…）")
    if args.producer_schema:
        errs = validate_producer_schema(_load_json(PRODUCER_SCHEMA_PATH))
        if args.write_hash:
            blocking = [e for e in errs if not e.startswith("self_hash 不匹配")
                        and not e.startswith("self_hash 未写入")]
            if blocking:
                print("[producer-schema] 校验未过，拒绝写入 self_hash", file=sys.stderr)
                for e in blocking:
                    print(f"  - {e}", file=sys.stderr)
                failed = True
            else:
                schema = _load_json(PRODUCER_SCHEMA_PATH)
                schema.setdefault("self_hash", {})
                schema["self_hash"]["algorithm"] = "sha256"
                schema["self_hash"]["value"] = compute_self_hash(schema)
                with open(PRODUCER_SCHEMA_PATH, "w", encoding="utf-8") as f:
                    json.dump(schema, f, ensure_ascii=False, indent=2)
                    f.write("\n")
                print(f"[producer-schema] self_hash 已写入: {schema['self_hash']['value']}")
                errs = validate_producer_schema(_load_json(PRODUCER_SCHEMA_PATH))
        if errs:
            failed = True
            print(f"[producer-schema] FAIL（{len(errs)} 项违规）:")
            for e in errs:
                print(f"  - {e}")
        else:
            print("[producer-schema] PASS")
    if args.forecast_provenance:
        rec = _load_json(args.forecast_provenance)
        errs = validate_forecast_provenance(rec, args.stage)
        if errs:
            failed = True
            print(f"[forecast-provenance] FAIL（stage={args.stage}）:")
            for e in errs:
                print(f"  - {e}")
        else:
            print(f"[forecast-provenance] PASS（stage={args.stage}）")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
