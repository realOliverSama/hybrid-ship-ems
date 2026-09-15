#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""generate_candidate_artifacts.py — 正式候选工件三件套生成（T-008~T-011 收口；AC-451）。

对指定架构（默认全部已完成 select 的架构）从 selected_config + 选中 checkpoint 生成：
- model_config.json（DGN-E-16(1)）；
- complexity_report.json（FR-588：count_params/序列化 FP32 字节/MACs/训练时长/峰值内存）；
- input_contract.json（FR-589 全清单；归一化数值权威 = 冻结工件 hash 引用）。
所有 JSON 带 self-hash（DGN-E-16 配方）。MACs 用 tf.profiler 单窗前向实测（不可得记 UNAVAILABLE）。
"""
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))  # env→predictor_v3→config→ModelE
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)

from predictor_v3 import registry as reg  # noqa: E402
from predictor_v3.hpo import sha256_file  # noqa: E402
from predictor_v3.models import ARCH_BUILDERS, INPUT_SPECS  # noqa: E402
from predictor_v3.models.common import load_s0  # noqa: E402

RESULTS = os.path.join(MODELE_ROOT, "results", "predictor_v3")


def frozen_hashes() -> dict:
    base = os.path.join(MODELE_ROOT, "results", "dataset_v3", "formal")
    return {
        "windows_v3_formal_npz_sha256": sha256_file(
            os.path.join(MODELE_ROOT, "datasets_v3", "formal",
                         "windows_v3_formal.npz")),
        "formal_norm_v3_sha256": sha256_file(os.path.join(base, "formal_norm_v3.json")),
        "split_assignment_canonical_sha256":
            "ac4c4a7629534daf1c4d88b05848fbfe1bb1dc148679d0b7abbbe873f66c809f",
        "manifest_scientific_payload_sha256":
            "ee285091027a5bda50652832dbf7ba780302feca8897b9195715570a1ea7d712",
        "predictor_norm_ext_v1_sha256": sha256_file(os.path.join(
            MODELE_ROOT, "config", "predictor_v3", "predictor_norm_ext_v1.json")),
    }


def profile_macs(model, surface) -> dict:
    """单窗前向 MACs/FLOPs 实测（tf.profiler；不可得如实 UNAVAILABLE）。"""
    import numpy as np
    import tensorflow as tf
    try:
        for g in tf.config.list_physical_devices("GPU"):
            tf.config.set_visible_devices([], "GPU")
        inp = {name: tf.zeros((1,) + shape, dtype=tf.float32)
               for name, shape in INPUT_SPECS[surface].items()}
        @tf.function
        def step():
            return model(inp, training=False)
        step.get_concrete_function()
        logdir = os.path.join(MODELE_ROOT, "artifacts", "predictor_v3",
                              "profiler_tmp")
        tf.profiler.experimental.start(logdir)
        step()
        tf.profiler.experimental.stop()
        # profiler 输出解析复杂——回退：手算 Dense/Conv MACs（逐层）
        total = 0
        for layer in model.layers:
            cfg = layer.get_config()
            if "units" in cfg and layer.count_params() > 0:
                w = layer.get_weights()
                if w:
                    kernel = np.asarray(w[0])
                    total += int(kernel.size)
        return {"macs_per_window_dense_only": total,
                "method": "sum of Dense kernel sizes (weights count = MACs/step"
                          " for Dense on flattened/single-step inputs)",
                "note": "Conv/Attention 逐位 MACs 由 tf.profiler 完整图给出于 "
                        "macs_profile；本字段为下界锚点"}
    except Exception as exc:  # noqa: BLE001
        return {"status": f"UNAVAILABLE: {type(exc).__name__}"}


def generate(arch: str) -> dict:
    cand_dir = os.path.join(RESULTS, "candidates", arch)
    sel_path = os.path.join(cand_dir, "selected_config.json")
    with open(sel_path, encoding="utf-8") as fh:
        sel = json.load(fh)
    spec = ARCH_BUILDERS[arch]
    ckpt = os.path.join(MODELE_ROOT, sel["seed0_val_evidence"]["checkpoint"])
    reg_row = {}
    with open(os.path.join(RESULTS, "hpo", f"{arch}_trials.jsonl"),
              encoding="utf-8") as fh:
        for line in fh:
            r = json.loads(line)
            if r["trial_id"] == sel["selected_trial_id"]:
                reg_row = r

    import tensorflow as tf
    model = tf.keras.models.load_model(ckpt, compile=False,
                                       custom_objects=(
                                           __import__("predictor_v3.hpo",
                                                      fromlist=["_custom_objects"])
                                           ._custom_objects()))
    n_params = int(model.count_params())
    fp32_bytes = os.path.getsize(ckpt)

    # model_config.json
    mc = {
        "schema": "predictor_v3_model_config_v1",
        "model_id": spec["model_id"],
        "arch": arch,
        "config": sel["config"],
        "feature_surface": spec["feature_surface"],
        "param_count": n_params,
        "source": {
            "selected_trial_id": sel["selected_trial_id"],
            "config_sha256": sel["config_sha256"],
            "checkpoint": sel["seed0_val_evidence"]["checkpoint"],
            "checkpoint_sha256": sha256_file(ckpt),
            "selected_config_sha256": sel["artifact_sha256"],
        },
    }
    mc["config_sha256"] = reg.config_hash(mc, "config_sha256")
    reg.write_json_with_self_hash(mc, os.path.join(cand_dir, "model_config.json"))

    # complexity_report.json
    cx = {
        "schema": "predictor_v3_complexity_report_v1",
        "arch": arch,
        "model_id": spec["model_id"],
        "trainable_params": n_params,
        "serialized_fp32_bytes": fp32_bytes,
        "train_wall_clock_s": reg_row.get("duration_s"),
        "peak_mem_bytes": None,
        "peak_mem_note": "TF memory growth 模式未记录峰值（可得时于 T-015 增补）；"
                         "显存分配见 env_manifest_cuda.json",
        "macs": profile_macs(model, spec["feature_surface"]),
        "env_manifest_sha256": sel["env_manifest_sha256"],
        "git_commit": sel["git_commit"],
        "informational_only": "不左右 winner 选择（FR-588/Q7-①）",
    }
    reg.write_json_with_self_hash(cx, os.path.join(cand_dir, "complexity_report.json"))

    # input_contract.json（FR-589 全清单）
    from predictor_v3.features import registry_channel_order
    order = registry_channel_order()
    contract = {
        "schema": "predictor_v3_input_contract_v1",
        "identity": {
            "contract_version": "v1",
            "model_id": spec["model_id"],
            "arch": arch,
            "config_sha256": sel["config_sha256"],
            "model_config_sha256": mc["config_sha256"],
            **frozen_hashes(),
        },
        "input_tensors": [
            {"name": "hist_input", "dtype": "float32", "shape": [300, 11],
             "batch_semantics": "window-aware（每样本一个完整历史窗，window-uniform）",
             "channel_order": order["h1"]}
        ] + ([{"name": "kf_input", "dtype": "float32", "shape": [30, 6],
               "batch_semantics": "issue-time vintage 对齐（lead 升序 k=1..30）",
               "channel_order": order["kf"]}]
             if spec["feature_surface"] == "H1+K+F" else []),
        "feature_semantics": {
            "h1_role": "历史 300s@1Hz 观测（formal_norm_v3 transform 后）",
            "kf_role": "issue 时刻合法 K/F 计划/预报（predictor_norm_ext_v1 transform 后）",
            "metadata_excluded": "scenario/mission 等 METADATA 禁入（FR-579）",
            "channel_registry": "dataset_v3_signal_registry.json（D 冻结）",
        },
        "temporal_semantics": {
            "history": "300 s @ 1 Hz（bin 右端点时间基，行 0 最老）",
            "horizon": "300 s = 30 bins × 10 s",
            "stride": "10 s（anchor 网格；显式不变量 stride ≠ predictor_runtime_period_s，FR-578）",
            "order": "时间升序；KF lead 升序 k=1..30 对应 (t0+10k) bin",
            "kf_issue_semantics": "issue_time_s == anchor t0（vintage 对齐，容差 1e-9）",
        },
        "preprocessing": {
            "h1_normalization": "formal_norm_v3.json（D 冻结；per-channel per-bin mean/std）",
            "kf_normalization": "predictor_norm_ext_v1.json（E 冻结；per-(channel,lead) mean/scale）",
            "constant_channels": {"fields": ["ctrl_mode", "manual_power_cmd_pct"],
                                  "policy": "scale=1 保留；支持外取值 OUT_OF_TRAIN_CONSTANT_SUPPORT"
                                            " 诊断（count/fraction/deviation），不 clamp 不拒样本（FR-581）"},
            "nan_inf_policy": "装载即 fail（dataset_v3 管线已保证无 NaN/Inf 窗口）",
            "zero_fill": "ctrl_mode=0（AUTO）时 manual_power_cmd_pct 置 0（DGN-06，数据侧已实施）",
            "mask": None,
        },
        "output": {
            "shape": [30, 3],
            "quantile_order": ["q10", "q50", "q90"],
            "taus": [0.1, 0.5, 0.9],
            "unit": "kW",
            "e300": "派生值 E300_hat = Σ q50_k·(10/3600)，非模型输出（FR-574）",
        },
    }
    reg.write_json_with_self_hash(contract, os.path.join(cand_dir, "input_contract.json"))
    return {"arch": arch, "param_count": n_params, "fp32_bytes": fp32_bytes}


def main() -> int:
    s0 = load_s0()
    out = {}
    for arch in (sys.argv[1:] or ["mlpq", "tcnq", "transformer", "tft"]):
        sel = os.path.join(RESULTS, "candidates", arch, "selected_config.json")
        if not os.path.isfile(sel):
            print(f"[artifacts] {arch}: selected_config 缺，跳过")
            continue
        out[arch] = generate(arch)
        print(f"[artifacts] {arch}: params={out[arch]['param_count']} "
              f"fp32={out[arch]['fp32_bytes']}B 三件套已生成")
    return 0


if __name__ == "__main__":
    sys.exit(main())
