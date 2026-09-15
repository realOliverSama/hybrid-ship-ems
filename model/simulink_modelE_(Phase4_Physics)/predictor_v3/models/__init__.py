# -*- coding: utf-8 -*-
"""models/ — Phase 4-E 五候选架构实现（DGN-E-07；E1/E2）。

registry：ARCH_BUILDERS（arch_id → {build, search_space, feature_surface,
model_id, envelope}）。build(cfg, s0) 返回编译前 Keras Model（输入命名
hist_input/kf_input，输出 [B,30,3]=[q10,q50,q90]）。
"""
from __future__ import annotations

from predictor_v3.models import (mlp_q, reat_t, tcn_q, transformer_baseline, tft)

ARCH_BUILDERS = {
    "mlpq": {"build": mlp_q.build, "search_space": mlp_q.SEARCH_SPACE,
             "feature_surface": mlp_q.FEATURE_SURFACE, "model_id": mlp_q.MODEL_ID,
             "envelope": mlp_q.ENVELOPE},
    "tcnq": {"build": tcn_q.build, "search_space": tcn_q.SEARCH_SPACE,
             "feature_surface": tcn_q.FEATURE_SURFACE, "model_id": tcn_q.MODEL_ID,
             "envelope": tcn_q.ENVELOPE},
    "transformer": {"build": transformer_baseline.build,
                    "search_space": transformer_baseline.SEARCH_SPACE,
                    "feature_surface": transformer_baseline.FEATURE_SURFACE,
                    "model_id": transformer_baseline.MODEL_ID,
                    "envelope": transformer_baseline.ENVELOPE},
    "tft": {"build": tft.build, "search_space": tft.SEARCH_SPACE,
            "feature_surface": tft.FEATURE_SURFACE, "model_id": tft.MODEL_ID,
            "envelope": tft.ENVELOPE},
    "reatt": {"build": reat_t.build, "search_space": reat_t.SEARCH_SPACE,
              "feature_surface": reat_t.FEATURE_SURFACE, "model_id": reat_t.MODEL_ID,
              "envelope": reat_t.ENVELOPE},
}

INPUT_SPECS = {          # DGN-E-05 输入契约（模型侧消费面）
    "H1": {"hist_input": (300, 11)},
    "H1+K+F": {"hist_input": (300, 11), "kf_input": (30, 6)},
}
