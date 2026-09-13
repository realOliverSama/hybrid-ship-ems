#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""build_voyage_group_map.py — IID voyage_id→split_group_id 映射提取（T-008 辅助）。

背景（DGV-E-02，design.md 偏差区）：windows_v3_formal.npz 无 split_group_id 列，
split_assignment_v3.json 只有 group→split——voyage→split_group 闭包是 D 侧生成期
信息，仅存于 raw .mat meta（DGN-14 family_expand 并查集产物）。
本驱动只读 raw IID 语料 meta（D 冻结数据零改动），落 E 侧元数据工件
results/predictor_v3/iid_voyage_group_map.json（self-hash，入库）。
元数据 = 身份映射，非科学分布值；split 归属以 split_assignment_v3.json 为权威
（本工件只做 voyage→group join，不重复 split 语义）。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

import numpy as np
from scipy.io import loadmat

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
IID_DIR = os.path.join(MODELE_ROOT, "datasets_v3", "formal", "iid")
OUT_PATH = os.path.join(MODELE_ROOT, "results", "predictor_v3",
                        "iid_voyage_group_map.json")


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    files = sorted(f for f in os.listdir(IID_DIR) if f.endswith(".mat"))
    print(f"[vgroup] scanning {len(files)} IID voyage meta …", flush=True)
    mapping = {}
    for i, fn in enumerate(files):
        m = loadmat(os.path.join(IID_DIR, fn), simplify_cells=True)
        meta = m["meta"]
        vid = str(meta["voyage_id"])
        if vid in mapping:
            raise ValueError(f"重复 voyage_id: {vid}")
        mapping[vid] = str(meta["split_group_id"])
        if (i + 1) % 100 == 0:
            print(f"  {i + 1}/{len(files)}", flush=True)
    n_groups = len(set(mapping.values()))
    doc = {
        "schema": "predictor_v3_iid_voyage_group_map_v1",
        "task": "T-008",
        "utc": datetime.now(timezone.utc).isoformat(),
        "n_voyages": len(mapping),
        "n_split_groups": n_groups,
        "mapping": mapping,
        "provenance": {
            "source": "datasets_v3/formal/iid/*.mat meta.split_group_id"
                      "（D 冻结数据只读；DGN-14 family closure 并查集产物）",
            "iid_dir_manifest_sha256": hashlib.sha256(canonical_json(
                {f: sha256_file(os.path.join(IID_DIR, f)) for f in files}
            )).hexdigest(),
            "note": "元数据工件（身份映射）；split 归属权威 = split_assignment_v3.json；"
                    "层级聚合（FR-599）与 split_group bootstrap（DGN-E-12）消费本映射",
        },
    }
    doc["artifact_sha256"] = hashlib.sha256(
        canonical_json({k: v for k, v in doc.items()
                        if k != "artifact_sha256"})).hexdigest()
    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    print(f"[vgroup] {len(mapping)} voyages / {n_groups} groups → "
          f"{os.path.relpath(OUT_PATH, MODELE_ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
