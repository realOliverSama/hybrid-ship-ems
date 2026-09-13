#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""write_reat_t_arch.py — T-012：DGN-E-07(e) REAT-T 架构机器可读化（自哈希落盘）。

非第二设计门（tasks T-012 措辞）：本 JSON 是已批准设计（design.md DGN-E-07(e)，
R1~R9 修正后）的操作化转录，逐字段对应设计条款；实质语义偏差必须停止升级。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
OUT = os.path.join(MODELE_ROOT, "config", "predictor_v3", "reat_t_arch.json")


def canonical_json(obj) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


arch = {
    "schema": "predictor_v3_reat_t_arch_v1",
    "task": "T-012",
    "design_source": "design.md DGN-E-07(e)（R1~R9 修正后；本文件为操作化转录，非第二设计门）",

    "tensor_interface": {
        "inputs": {
            "hist_input": {"shape": [300, 11], "dtype": "float32",
                           "semantics": "H1 历史 300s@1Hz（formal_norm_v3 transform 后）"},
            "kf_input": {"shape": [30, 6], "dtype": "float32",
                         "semantics": "KF issue 时刻合法计划/预报（predictor_norm_ext_v1 transform 后；lead 升序 k=1..30）"},
        },
        "output": {"shape": [30, 3], "dtype": "float32",
                   "semantics": "末维 [q10,q50,q90]，kW；E300 派生非输出（FR-574）"},
        "kf_visibility": "R3：future 分支 30 位合法 K/F 全互见（无因果掩码）；padding mask 允许；future realized target/ORACLE 禁止",
    },

    "past_branch": {
        "type": "TCN",
        "input": "hist_input [B,300,11]",
        "input_proj": {"layer": "Conv1D", "filters": "Cp", "kernel": 1,
                       "padding": "same", "out": "[B,300,Cp]"},
        "residual_blocks": {
            "count": 6,
            "per_block": ["Conv1D(Cp, k=3, padding=causal, dilation=d)",
                          "LayerNorm", "GELU", "Dropout(p)", "Add(skip)"],
            "dilations": [1, 2, 4, 8, 16, 32],
            "note": "causal padding = TCN 标准时间因果语义（历史序列内部）；"
                    "与 future 分支的 K/F 可见域无关（R3 只约束 30 位未来）",
        },
        "attention_pooling": {"layer": "LearnedQueryAttentionPooling",
                              "out": "h_past [B,Cp]",
                              "definition": "score=Dense(1,use_bias=False)→softmax(时间位)→加权和"},
    },

    "future_branch": {
        "type": "Transformer",
        "input_proj": {"layer": "Dense", "units": "df", "out": "[B,30,df]"},
        "lead_positional_encoding": {"layer": "LearnedPositionalEncoding",
                                     "n": 30, "d": "df",
                                     "out": "[B,30,df]（输入投影 + pos）"},
        "encoder_blocks": {
            "count": "Nf",
            "per_block": ["LayerNorm", "self-MHA(h heads, 全 30 位互见，无因果掩码)",
                          "Dropout(p)", "Add", "LayerNorm", "FFN 4df(GELU→Linear)",
                          "Dropout(p)", "Add"],
            "preln": True,
            "ffn_width": "4*df（与 Transformer baseline 的 2d 不同——DGN-E-07(e) 原文）",
        },
        "out": "fut_seq [B,30,df]",
    },

    "fusion": {
        "steps": [
            "h_past [B,Cp] → broadcast/tile 至 [B,30,Cp]",
            "concat(fut_seq, h_past_tiled) → [B,30,Cp+df]",
            "LayerNorm",
            "Dense(df, GELU)",
            "Dropout(p)",
        ],
        "out": "fused [B,30,df]",
    },

    "decoder": {
        "per_lead_shared": True,
        "steps": ["Dense(d_dec, GELU)", "Dropout(p)", "Dense(3) → (m,a,b)"],
        "quantile_output": {
            "parameterization": "共用 MonotonicQuantileTransform（DGN-E-06；q50=m、q10=m−softplus(a)、q90=m+softplus(b)）",
            "raw_bias_init": "a/b 位 = softplus_inv(s0)，m 位 = 0；s0 = train 全局 y_std（eval_config 冻结值）",
            "module_constraint": "模块内禁二次投影（R4）；全体候选共用同一实现",
        },
        "out": "[B,30,3] 末维 [q10,q50,q90]",
    },

    "norm_residual_dropout_policy": {
        "norm": "LayerNorm（past blocks / future encoder PreLN / fusion / decoder 前无）",
        "residual": "past blocks Add(skip)（输入投影后）；future encoder Add×2/block；fusion/decoder 无 skip（维度不符）",
        "dropout": "统一标量 p（搜索空间维度）；past blocks / future MHA+FFN / fusion / decoder 各 Dropout(p)",
    },

    "capacity_envelope": {
        "param_range": [500000, 3000000],
        "unit": "model.count_params()（机器证据）",
        "obligation": "搜索空间须含 ≥1 个 ≤1.5M 有效配置（FR-587）；超界配置在 trial list 生成期过滤（DGN-E-08 ③）",
    },

    "anchor": {
        "Cp": 96, "df": 128, "Nf": 3, "h": 4, "d_dec": 128,
        "dropout": 0.1, "lr": 3e-4, "batch": 128,
        "estimated_params_note": "设计估计 ≈0.78M（TCN ≈333K + future ≈394K + fusion/decoder ≈45K）；以 count_params 机器证据为准",
        "note": "DGN-E-08 ⑤ 注入义务仅在 Cp/df/Nf/d_dec 四维；dropout/lr 取搜索空间代表值（dropout=0.1、lr=3e-4）作完整可执行配置，在此冻结",
    },

    "search_space": {
        "Cp": [64, 96, 128], "df": [64, 128], "Nf": [2, 3, 4],
        "d_dec": [64, 128], "h": [4], "dropout": [0.0, 0.1],
        "lr": [1e-4, 3e-4, 1e-3], "batch": [128],
        "budget_cap": 20,
        "generation": "DGN-E-08 七步算法（枚举→canonicalize→容量过滤→audit→锚点注入→HPO 种子无放回抽样→冻结）",
    },

    "ablation_arms": {
        "mechanism": "R2 架构保持的归一化零通道替换（同拓扑同参数量）；零 = transform 后 0 值（train 均值语义）",
        "faces": {
            "reatt_abl_h1": {"zero_channels": "全部 6 个 KF 通道"},
            "reatt_abl_h1k": {"zero_channels": "2 个 F 通道（u_fcst_mps/wl_fcst_m 之外的 K 通道保留——"
                              "K=计划量 v_plan/s_plan/h_chart/P_hotel_plan；F=预报量 u_fcst/wl_fcst）"},
            "reatt_abl_h1kf": {"reuse": "复用正式 REAT-T 候选运行，不重复训练"},
        },
        "mask_recording": "消融 config/contract 记录 active-information-channel mask；禁止新增运行时 mask 张量",
        "winner_pool": "消融臂不进 winner pool（E-Q9-2）",
    },

    "utc": datetime.now(timezone.utc).isoformat(),
}
arch["artifact_sha256"] = hashlib.sha256(
    canonical_json({k: v for k, v in arch.items() if k != "artifact_sha256"})).hexdigest()

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(arch, fh, ensure_ascii=False, indent=2, sort_keys=True)
    fh.write("\n")
print(f"[reat_t_arch] written {os.path.relpath(OUT, MODELE_ROOT)} "
      f"self_hash={arch['artifact_sha256'][:16]}…")
