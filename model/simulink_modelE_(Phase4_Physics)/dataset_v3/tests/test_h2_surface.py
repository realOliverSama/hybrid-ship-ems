#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_h2_surface.py — T432_ACTIVATION_AMENDMENT_02 H2/Dataset 导出面不变量测试.

消除双事实源缺陷（formal 导出面 = schema required 派生；pilot 导出面 = registry
全集派生）的回归锚：

  P   真实 registry + producer schema → 0 违规（两个事实源钉死）；
  P   AMENDMENT_02 clarification：HT_ratio/domain_warning 为设计内 pilot-only
      stream 列（METADATA/metadata_only），不构成导出面漂移；
  N-A tmp schema 副本删任一 H2 required 字段（SOC）→ 违规且信息含 'SOC'
      （FR-461 H2 Dataset 保留义务破坏即 formal 导出面缺列）；
  N-B tmp schema 副本加一个 registry 外列 → 导出面漂移检出（schema 面多列）；
  N-C registry INPUT_H2 集被削减（tmp registry 副本）→ validator 冻结副本
      FR-461 集校验 + H2 面校验双重检出；
  锚  真实 pilot mat（datasets_v3/pilot/ 任一）stream 列集 ⊇ schema required
      Dataset 面（pilot 语料与 schema 导出面一致；当前修正后应过）。
"""

import copy
import glob
import json
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from dataset_v3 import validate_contracts as vc

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(HERE))
REGISTRY_PATH = os.path.join(MODELE_ROOT, "config", "dataset_v3_signal_registry.json")
SCHEMA_PATH = os.path.join(MODELE_ROOT, "config", "dataset_v3_producer_schema.json")


def _required_face(schema: dict, registry: dict) -> set:
    """schema required Dataset 面（与 validate_h2_surface 同口径）：
    truth_stream required 减 time_s 减 registry 非 Dataset 保留角色的字段。"""
    role_of = {s["name"]: s.get("role") for s in registry["signals"]
               if isinstance(s.get("name"), str)}
    face = {f["name"] for f in schema["capability_families"]["truth_stream"]["fields"]
            if f.get("required") is True}
    face.discard(vc.TIME_BASE_FIELD)
    return {n for n in face
            if role_of.get(n) is None or role_of[n] in vc.DATASET_RETENTION_ROLES}


class TestH2SurfacePositive:
    def test_real_files_zero_violations(self):
        reg = vc._load_json(REGISTRY_PATH)
        schema = vc._load_json(SCHEMA_PATH)
        errs = vc.validate_h2_surface(reg, schema)
        assert errs == [], f"真实契约文件存在导出面漂移: {errs}"

    def test_metadata_pilot_only_stream_columns_not_drift(self):
        """AMENDMENT_02 clarification：HT_ratio/domain_warning 为设计内
        pilot-only stream 列（registry METADATA / predictor_access=metadata_only），
        剔除出 Dataset 保留面 scope，不构成导出面漂移（FAIL 语义只针对
        INPUT_H1/INPUT_H2/TARGET 面）。"""
        reg = vc._load_json(REGISTRY_PATH)
        by_name = {s["name"]: s for s in reg["signals"]}
        for col in ("HT_ratio", "domain_warning"):
            assert by_name[col]["role"] == "METADATA", col
            assert by_name[col]["predictor_access"] == "metadata_only", col
        schema = vc._load_json(SCHEMA_PATH)
        errs = vc.validate_h2_surface(reg, schema)
        assert not any(("HT_ratio" in e) or ("domain_warning" in e) for e in errs), \
            f"METADATA pilot-only stream 列不得被判为漂移: {errs}"


class TestH2SurfaceNegative:
    def _tampered_schema(self, tmp_path, mutate):
        schema = copy.deepcopy(vc._load_json(SCHEMA_PATH))
        mutate(schema)
        p = tmp_path / "schema_tampered.json"
        p.write_text(json.dumps(schema, ensure_ascii=False, indent=2), encoding="utf-8")
        return vc._load_json(str(p))

    def test_schema_missing_soc(self, tmp_path):
        """N-A：schema 副本删 SOC（H2 required）→ 违规且信息含 'SOC'。"""
        def drop_soc(s):
            ts = s["capability_families"]["truth_stream"]
            ts["fields"] = [f for f in ts["fields"] if f.get("name") != "SOC"]
        schema = self._tampered_schema(tmp_path, drop_soc)
        reg = vc._load_json(REGISTRY_PATH)
        errs = vc.validate_h2_surface(reg, schema)
        assert errs, "删除 H2 required 字段必须被检出"
        assert any("SOC" in e for e in errs), f"违规信息必须点名 SOC: {errs}"

    def test_schema_extra_column_outside_registry(self, tmp_path):
        """N-B：schema 副本加一个 registry 外列 → 导出面漂移（多列）检出。"""
        def add_col(s):
            ts = s["capability_families"]["truth_stream"]
            ts["fields"].append({
                "name": "X_extra_not_in_registry",
                "type": "float64[N]",
                "shape_semantics": "per-sample",
                "required": True,
                "causality_rule": "realization-time",
            })
        schema = self._tampered_schema(tmp_path, add_col)
        reg = vc._load_json(REGISTRY_PATH)
        errs = vc.validate_h2_surface(reg, schema)
        assert errs, "schema 面多出 registry 外列必须被检出"
        assert any("X_extra_not_in_registry" in e for e in errs), \
            f"违规信息必须点名漂移列: {errs}"

    def test_registry_h2_set_reduced(self, tmp_path):
        """N-C：registry INPUT_H2 削减（SOC 条目降为 FORBIDDEN）→ fail-closed。

        分层防御：registry 侧 H2 集变动由 validate_registry 的 FR-461 冻结集
        双向校验检出；validate_h2_surface 的 H2 保留面是 registry⊆schema 方向
        （schema 缺列即 formal 缺列），registry 自身削减不属其判定面。
        """
        def drop_h2(r):
            for s in r["signals"]:
                if s.get("name") == "SOC":
                    s["role"] = "FORBIDDEN"
        reg = copy.deepcopy(vc._load_json(REGISTRY_PATH))
        drop_h2(reg)
        p = tmp_path / "registry_tampered.json"
        p.write_text(json.dumps(reg, ensure_ascii=False, indent=2), encoding="utf-8")
        reg = vc._load_json(str(p))
        errs = vc.validate_registry(reg)
        assert errs, "registry INPUT_H2 削减必须被 FR-461 冻结集校验检出"
        assert any("SOC" in e and "INPUT_H2" in e for e in errs), \
            f"FR-461 违规信息必须点名 SOC/INPUT_H2: {errs}"


class TestPilotAnchor:
    def test_pilot_stream_superset_of_schema_face(self):
        """回归锚：真实 pilot mat stream 列集 ⊇ schema required Dataset 面。

        pilot 导出面按 registry 全集派生、formal 按 schema required 派生；
        pilot 语料缺任何 schema required Dataset 列即说明两事实源仍漂移。
        """
        import scipy.io as sio

        schema = vc._load_json(SCHEMA_PATH)
        reg = vc._load_json(REGISTRY_PATH)
        face = _required_face(schema, reg)
        assert face, "schema required Dataset 面不得为空"

        mats = sorted(glob.glob(os.path.join(MODELE_ROOT, "datasets_v3",
                                             "pilot", "*.mat")))
        assert mats, "datasets_v3/pilot/ 下必须存在 pilot mat 语料"
        mat = sio.loadmat(mats[0], squeeze_me=True)
        assert "stream" in mat, f"{mats[0]} 缺 stream 表"
        stream_cols = set(mat["stream"].dtype.names)

        missing = sorted(face - stream_cols)
        assert not missing, (
            f"pilot 语料 {os.path.basename(mats[0])} stream 缺 schema required "
            f"Dataset 列: {missing}（pilot/formal 导出面漂移）"
        )
