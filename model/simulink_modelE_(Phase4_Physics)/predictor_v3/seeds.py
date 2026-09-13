# -*- coding: utf-8 -*-
"""seeds.py — 种子派生与 RNG 固定（T-005，DGN-E-10；FR-593/594）。

冻结派生规则：
    payload = f"{seed_root}|{model_id}|{config_hash}|{seed_index}".encode()
    digest  = hashlib.sha256(payload).digest()
    seed_int = int.from_bytes(digest[:8], "big") % (2**31)

- seed_root 冻结 `phase4e-predictor-v3-seed-v1`；
- model_id ∈ {mlpq, tcnq, tft, transformer, reatt}；消融臂 `reatt_abl_<face>`；
- 开发/HPO 只用 seed_index=0；正式确认/消融 = {0,1,2}；
- HPO 引擎种子 = SHA256(seed_root‖"hpo"‖arch_id)（DGN-E-08 ⑥）；
- 固定点：PYTHONHASHSEED（进程启动前设置并记录）、keras.utils.set_random_seed、
  tf.random.set_seed、python random、numpy default_rng、数据 shuffle、随机层。
"""
from __future__ import annotations

import hashlib
import os
import random

import numpy as np

SEED_ROOT = "phase4e-predictor-v3-seed-v1"
SEED_MODULUS = 2 ** 31
CONFIRMATION_SEED_INDICES = (0, 1, 2)
DEV_SEED_INDEX = 0

VALID_MODEL_IDS = ("mlpq", "tcnq", "tft", "transformer", "reatt")


def derive_seed(model_id: str, config_hash: str, seed_index: int) -> int:
    """冻结公式派生稳定整数种子。"""
    payload = f"{SEED_ROOT}|{model_id}|{config_hash}|{seed_index}".encode()
    digest = hashlib.sha256(payload).digest()
    return int.from_bytes(digest[:8], "big") % SEED_MODULUS


def derive_hpo_seed(arch_id: str) -> int:
    """HPO 引擎种子：SHA256(seed_root‖"hpo"‖arch_id) → 同映射（DGN-E-08 ⑥）。"""
    payload = f"{SEED_ROOT}|hpo|{arch_id}".encode()
    digest = hashlib.sha256(payload).digest()
    return int.from_bytes(digest[:8], "big") % SEED_MODULUS


def set_global_seeds(seed_int: int, *, tf_enabled: bool = True) -> dict:
    """固定全部 RNG 固定点；返回记录（FR-594 确定性声明口径）。

    PYTHONHASHSEED 只能在进程启动前生效——本函数记录当前值并要求
    正式运行经 `python -X env PYTHONHASHSEED=<n>` 或环境变量设置（fail-loud）。
    """
    record = {"seed_int": int(seed_int),
              "pythonhashseed": os.environ.get("PYTHONHASHSEED", "NOT_SET"),
              "numpy_version": np.__version__}
    random.seed(seed_int)
    np.random.seed(seed_int % (2 ** 32))
    if tf_enabled:
        try:
            import tensorflow as tf
            tf.random.set_seed(seed_int)
            try:
                import keras
                keras.utils.set_random_seed(seed_int)
                record["keras_set_random_seed"] = True
            except Exception:  # noqa: BLE001
                record["keras_set_random_seed"] = False
            try:
                tf.config.experimental.enable_op_determinism()
                record["op_determinism"] = True
            except Exception:  # noqa: BLE001
                record["op_determinism"] = False
        except ImportError:
            record["tf_enabled"] = False
    return record


def make_rng(seed_int: int) -> np.random.Generator:
    """numpy default_rng（数据 shuffle/合成数据用）。"""
    return np.random.default_rng(seed_int)
