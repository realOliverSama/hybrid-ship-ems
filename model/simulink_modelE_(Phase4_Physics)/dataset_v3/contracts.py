#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""contracts.py — Dataset v3 机器契约访问层（主 Agent 冻结接口，T-001 配套）.

包内模块统一经此消费 registry/producer schema，禁止各自 json.load + 自行解释
（FR-465：机器契约唯一事实源，禁多脚本独立 hard-coded list）。
"""

from __future__ import annotations

import json
import os

from .validate_contracts import (MODELE_ROOT, REGISTRY_PATH, PRODUCER_SCHEMA_PATH,
                                 validate_registry, validate_producer_schema)

REGISTRY_SCHEMA = "dataset_v3_signal_registry_v1"
PRODUCER_SCHEMA_ID = "dataset_v3_producer_schema_v1"


def load_registry(validate: bool = True) -> dict:
    """读取 registry；默认先跑结构校验（fail-fast，契约错误不降级）。"""
    with open(REGISTRY_PATH, "r", encoding="utf-8") as f:
        reg = json.load(f)
    if validate:
        errs = validate_registry(reg)
        if errs:
            raise ValueError("registry 校验失败: " + "; ".join(errs[:5]))
    return reg


def load_producer_schema(validate: bool = True) -> dict:
    with open(PRODUCER_SCHEMA_PATH, "r", encoding="utf-8") as f:
        schema = json.load(f)
    if validate:
        errs = validate_producer_schema(schema)
        if errs:
            raise ValueError("producer schema 校验失败: " + "; ".join(errs[:5]))
    return schema


def signals_of_role(reg: dict, role: str) -> list:
    """按 role 取信号条目列表（保持 registry 文件顺序）。"""
    return [s for s in reg["signals"] if s["role"] == role]


def signal_names(reg: dict, role: str) -> list:
    return [s["name"] for s in signals_of_role(reg, role)]


def aggregation_map(reg: dict, roles=None) -> dict:
    """name → aggregation 规则映射（roles 缺省=全部输入类+TARGET）。

    同名多 role（如 P_load_total_kW 的 INPUT_H1/TARGET）必须聚合规则一致，
    不一致直接 ValueError（fail-fast——静默取先会掩盖单边契约漂移）。
    """
    roles = roles or ("INPUT_H1", "INPUT_H2", "INPUT_K", "INPUT_F",
                      "ORACLE", "METADATA", "FORBIDDEN", "TARGET")
    out: dict = {}
    for s in reg["signals"]:
        if s["role"] in roles:
            nm, agg = s["name"], s["aggregation"]
            if nm in out and out[nm] != agg:
                raise ValueError(
                    f"registry 同名多 role 聚合规则不一致: {nm} = {out[nm]} vs {agg}")
            out[nm] = agg
    return out


def predictor_access_map(reg: dict) -> dict:
    out = {}
    for s in reg["signals"]:
        out.setdefault(s["name"], set()).add(s["predictor_access"])
    return out


def source_map(reg: dict) -> dict:
    """(name, role) → source_signal（同名多 role 时区分）。"""
    return {(s["name"], s["role"]): s["source_signal"] for s in reg["signals"]}


__all__ = ["load_registry", "load_producer_schema", "signals_of_role", "signal_names",
           "aggregation_map", "predictor_access_map", "source_map",
           "REGISTRY_SCHEMA", "PRODUCER_SCHEMA_ID", "MODELE_ROOT"]
