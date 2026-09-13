"""Phase 4-E predictor-v3：sealed 数据访问层与访问门（DGN-E-02）。

纪律（FR-572/575/577/606；NFR-444）：
- fail-closed：`load_split` 只开放 ``train``/``val``；IID test 与正式 OOD
  只能经 `load_terminal`，且必须通过六项核验（DGN-E-02 ①~⑥）；
- OOD 访问需 ``ood=True`` 显式形参 + 已登记 campaign；
- 访问日志 per-run 分片 jsonl（开发期 campaign="dev"），sealed 纪律审计
  （AC-455）的机器证据；
- 路径一律 ``__file__`` 推导（NFR-442），AutoDL 侧经环境变量
  ``EMS_PREDICTOR_V3_ROOT`` 注入并优雅降级（DGN-E-01）；
- 本模块不做 transform / 特征装配（T-004/T-005 职责），只返回 raw 窗口。
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import traceback
import uuid
from datetime import datetime, timezone

import numpy as np

# ---------------------------------------------------------------- 路径布局

_HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(_HERE)


def _repo_root() -> str:
    candidate = os.path.dirname(MODELE_ROOT)
    if os.path.isdir(os.path.join(candidate, ".git")):
        return candidate
    return os.environ.get("EMS_PREDICTOR_V3_REPO_ROOT", candidate)


REPO_ROOT = _repo_root()

IID_NPZ_PATH = os.path.join(
    MODELE_ROOT, "datasets_v3", "formal", "windows_v3_formal.npz")
OOD_NPZ_PATH = os.path.join(
    MODELE_ROOT, "artifacts", "predictor_v3", "data", "ood_windows_v3_formal.npz")
RESULTS_ROOT = os.path.join(MODELE_ROOT, "results", "predictor_v3")
DATA_ACCESS_ROOT = os.path.join(RESULTS_ROOT, "data_access")
CAMPAIGN_DIR = os.path.join(RESULTS_ROOT, "terminal_campaigns")
WINNER_DIR = os.path.join(RESULTS_ROOT, "winner")
EVAL_CONFIG_PATH = os.path.join(
    MODELE_ROOT, "config", "predictor_v3", "eval_config.json")

OPEN_SPLITS = ("train", "val")
TERMINAL_SETS = ("test", "ood")

# DGN-E-16(6) 生命周期：DRAFT → REGISTERED_LOCKED → UNSEAL_AUTHORIZED →
# RUNNING → COMPLETE；FR-608 事故协议允许 INTERRUPTED。
LIFECYCLE_ORDER = (
    "DRAFT", "REGISTERED_LOCKED", "UNSEAL_AUTHORIZED", "RUNNING", "COMPLETE")
TERMINAL_ACCESS_STATES = ("UNSEAL_AUTHORIZED", "RUNNING", "COMPLETE")


class SealedAccessError(PermissionError):
    """sealed 数据越权访问（fail-closed；DGN-E-02 Error Handling）。"""


# ------------------------------------------------------------ 通用工具

def canonical_json(obj) -> bytes:
    """与 dataset_v3.validate_contracts 同配方：sort_keys + 紧凑分隔符 + utf-8。"""
    return json.dumps(obj, sort_keys=True, separators=(",", ":"),
                      ensure_ascii=False).encode("utf-8")


def self_hash(obj: dict, key: str = "artifact_sha256") -> str:
    """SHA256(canonical JSON 排除自引用字段)。"""
    body = {k: v for k, v in obj.items() if k != key}
    return hashlib.sha256(canonical_json(body)).hexdigest()


def file_sha256(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def current_git_commit() -> str:
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO_ROOT,
            stderr=subprocess.DEVNULL).decode().strip()
    except Exception:  # noqa: BLE001
        return "GIT_UNAVAILABLE"


def _utcnow() -> str:
    return datetime.now(timezone.utc).isoformat()


# ------------------------------------------------------------ 访问日志

class AccessLogger:
    """per-run 分片访问日志（DGN-E-02 R9：每 run 一个 jsonl，避免共享文件并发写）。"""

    def __init__(self, campaign_id: str, run_id: str | None = None,
                 root: str | None = None):
        self.campaign_id = campaign_id
        self.run_id = run_id or f"{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}_{uuid.uuid4().hex[:8]}"
        self.root = root or DATA_ACCESS_ROOT
        self.path = os.path.join(self.root, campaign_id, f"{self.run_id}.jsonl")

    def log(self, *, split: str, caller_summary: str, note: str = "") -> None:
        entry = {
            "timestamp_utc": _utcnow(),
            "split": split,
            "campaign_id": self.campaign_id,
            "run_id": self.run_id,
            "caller": caller_summary,
            "git_commit": current_git_commit(),
        }
        if note:
            entry["note"] = note
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(entry, ensure_ascii=False,
                                sort_keys=True) + "\n")


def _caller_summary() -> str:
    """调用方摘要：栈上第一个非本模块帧的 module.function。"""
    for frame in traceback.extract_stack()[:-1]:
        if not frame.filename.endswith("data_access.py"):
            return f"{os.path.basename(frame.filename)}:{frame.name}:{frame.lineno}"
    return "unknown"


# ------------------------------------------------------------ DatasetView

_H1_PREFIX = "X_H1::"
_KF_PREFIX = "X_KF::"
_H2_PREFIX = "X_H2::"


class DatasetView:
    """懒加载窗口视图（IID npz 按 split 过滤 / OOD npz 全量）。

    只暴露 raw 窗口与窗口元数据；归一化 transform 属 T-004/T-005，
    本类不做任何数值变换，也不提供任何统计/指标接口（sealed 纪律）。
    """

    def __init__(self, npz_path: str, *, kind: str, split: str | None = None):
        self.npz_path = npz_path
        self.kind = kind          # "iid_split" | "ood"
        self.split = split
        self._npz = np.load(npz_path, allow_pickle=False)  # lazy：取列时才解压
        names = set(self._npz.files)
        for required in ("Y", "E300", "voyage_id", "anchor_t0_s",
                         "window_index_in_voyage"):
            if required not in names:
                raise ValueError(f"npz 缺必需列 {required}: {npz_path}")
        if kind == "iid_split":
            if split not in ("train", "val", "test"):
                raise ValueError(f"非法 split: {split}")
            self._index = np.flatnonzero(self._npz["splits"] == split)
        elif kind == "ood":
            if "ood_condition" not in names:
                raise ValueError("OOD npz 缺 ood_condition 列")
            self._index = np.arange(self._npz["Y"].shape[0])
        else:
            raise ValueError(f"非法 kind: {kind}")
        self.h1_channels = sorted(
            n[len(_H1_PREFIX):] for n in names if n.startswith(_H1_PREFIX))
        self.kf_channels = sorted(
            n[len(_KF_PREFIX):] for n in names if n.startswith(_KF_PREFIX))
        self.h2_channels = sorted(
            n[len(_H2_PREFIX):] for n in names if n.startswith(_H2_PREFIX))

    # -- 属性

    @property
    def n_windows(self) -> int:
        return int(self._index.shape[0])

    @property
    def voyage_ids(self) -> np.ndarray:
        return self._npz["voyage_id"][self._index]

    @property
    def ood_conditions(self) -> np.ndarray | None:
        if self.kind != "ood":
            return None
        return self._npz["ood_condition"][self._index]

    # -- 数据访问（raw）

    def window(self, i: int) -> dict:
        """第 i 个（视图内）窗口 → raw dict（hist [300,11] / kf [30,6] / y [30] ...）。"""
        if i < 0 or i >= self.n_windows:
            raise IndexError(i)
        row = int(self._index[i])
        hist = np.stack(
            [self._npz[f"{_H1_PREFIX}{c}"][row] for c in self.h1_channels],
            axis=-1).astype(np.float32)
        kf = np.stack(
            [self._npz[f"{_KF_PREFIX}{c}"][row] for c in self.kf_channels],
            axis=-1).astype(np.float32)
        return {
            "hist_raw": hist,
            "kf_raw": kf,
            "y_raw": self._npz["Y"][row].astype(np.float32),
            "e300_raw": float(self._npz["E300"][row]),
            "voyage_id": str(self._npz["voyage_id"][row]),
            "anchor_t0_s": float(self._npz["anchor_t0_s"][row]),
            "window_index_in_voyage": int(self._npz["window_index_in_voyage"][row]),
            **({"ood_condition": str(self._npz["ood_condition"][row])}
               if self.kind == "ood" else {}),
        }

    def raw_arrays(self, include_h2: bool = False) -> dict:
        """视图内全量 raw arrays（训练/评估侧物化；仍无 transform）。

        注意：H2 仅在显式 include_h2=True 时返回（v1 模型不消费，FR-582）。
        """
        out = {"y_raw": self._npz["Y"][self._index].astype(np.float32),
               "e300_raw": self._npz["E300"][self._index].astype(np.float32),
               "voyage_id": self._npz["voyage_id"][self._index],
               "anchor_t0_s": self._npz["anchor_t0_s"][self._index],
               "window_index_in_voyage": self._npz["window_index_in_voyage"][self._index]}
        out["hist_raw"] = np.stack(
            [self._npz[f"{_H1_PREFIX}{c}"][self._index] for c in self.h1_channels],
            axis=-1).astype(np.float32)
        out["kf_raw"] = np.stack(
            [self._npz[f"{_KF_PREFIX}{c}"][self._index] for c in self.kf_channels],
            axis=-1).astype(np.float32)
        if include_h2:
            out["h2_raw"] = np.stack(
                [self._npz[f"{_H2_PREFIX}{c}"][self._index] for c in self.h2_channels],
                axis=-1).astype(np.float32)
        if self.kind == "ood":
            out["ood_condition"] = self._npz["ood_condition"][self._index]
        return out


# ------------------------------------------------------------ campaign 六项核验

def _load_campaign(campaign_id: str, campaign_dir: str | None) -> dict:
    path = os.path.join(campaign_dir or CAMPAIGN_DIR, f"{campaign_id}.json")
    if not os.path.isfile(path):
        raise SealedAccessError(
            f"[核验①] campaign 未登记: {campaign_id}（{path} 不存在）")
    with open(path, encoding="utf-8") as fh:
        campaign = json.load(fh)
    if campaign.get("campaign_id") != campaign_id:
        raise SealedAccessError("[核验①] campaign_id 字段与登记名不一致")
    expected = campaign.get("artifact_sha256")
    if not expected or self_hash(campaign) != expected:
        raise SealedAccessError("[核验①] campaign 工件自哈希不一致")
    return campaign


def _check_preterminal_artifact(campaign: dict,
                                modele_root: str | None = None) -> dict:
    """核验②③：PRETERMINAL winner 包在位 + hash 匹配 + Winner 身份匹配。"""
    root = modele_root or MODELE_ROOT
    rel = campaign.get("preterminal_winner_manifest_path")
    want = campaign.get("preterminal_winner_artifact_sha256")
    if not rel or not want:
        raise SealedAccessError("[核验②] campaign 缺 PRETERMINAL 工件引用")
    manifest_path = os.path.join(root, rel)
    if not os.path.isfile(manifest_path):
        raise SealedAccessError(
            f"[核验②] PRETERMINAL winner 包 manifest 不存在: {rel}")
    got = file_sha256(manifest_path)
    if got != want:
        raise SealedAccessError("[核验②] PRETERMINAL 工件 hash 不匹配")
    with open(manifest_path, encoding="utf-8") as fh:
        manifest = json.load(fh)
    winner_identity = manifest.get("winner_identity_sha256")
    if not winner_identity or \
            winner_identity != campaign.get("winner_identity_sha256"):
        raise SealedAccessError("[核验③] 冻结 Winner 身份不匹配")
    return manifest


def _check_timestamp_order(campaign: dict, manifest: dict) -> None:
    """核验④：Winner 选择时间戳严格早于终评授权时间戳（tasks R6）。"""
    selected = manifest.get("winner_selected_utc")
    authorized = campaign.get("authorized_utc")
    if not selected or not authorized:
        raise SealedAccessError("[核验④] 缺 winner_selected_utc / authorized_utc")
    if datetime.fromisoformat(selected) >= datetime.fromisoformat(authorized):
        raise SealedAccessError(
            "[核验④] winner_selected_utc 不早于 authorized_utc（时间戳序违规）")


def _check_code_and_eval_config(campaign: dict,
                                eval_config_path: str | None = None) -> None:
    """核验⑤：代码 commit / eval config 身份与 campaign 冻结一致。"""
    if current_git_commit() != campaign.get("code_git_commit"):
        raise SealedAccessError("[核验⑤] 代码 commit 与 campaign 冻结不一致")
    ec_path = eval_config_path or EVAL_CONFIG_PATH
    if not os.path.isfile(ec_path):
        raise SealedAccessError("[核验⑤] eval_config.json 不存在")
    if file_sha256(ec_path) != campaign.get("eval_config_sha256"):
        raise SealedAccessError("[核验⑤] eval config 身份与 campaign 冻结不一致")


def _check_status(campaign: dict) -> None:
    """核验⑥：campaign 状态机允许终评访问（fail-closed：INTERRUPTED 拒绝，
    须显式恢复到 UNSEAL_AUTHORIZED 后方可再访问）。"""
    status = campaign.get("status")
    if status not in TERMINAL_ACCESS_STATES:
        raise SealedAccessError(
            f"[核验⑥] campaign 状态 {status} 不允许终评访问"
            f"（允许态 = {TERMINAL_ACCESS_STATES}）")


# ------------------------------------------------------------ 公开 API

def load_split(split: str, *, log: bool = True,
               iid_npz_path: str | None = None,
               access_root: str | None = None) -> DatasetView:
    """训练/HPO/val 评估默认入口：只开放 train/val（fail-closed）。

    传 ``test`` 或其他任何值一律 raise SealedAccessError。
    """
    if split not in OPEN_SPLITS:
        raise SealedAccessError(
            f"load_split 拒绝 split={split!r}（仅开放 {OPEN_SPLITS}；"
            "test/OOD 须经 load_terminal 六项核验）")
    view = DatasetView(iid_npz_path or IID_NPZ_PATH,
                       kind="iid_split", split=split)
    if log:
        AccessLogger("dev", root=access_root).log(
            split=split, caller_summary=_caller_summary())
    return view


def load_terminal(set_name: str, campaign_id: str, *, ood: bool = False,
                  iid_npz_path: str | None = None,
                  ood_npz_path: str | None = None,
                  campaign_dir: str | None = None,
                  access_root: str | None = None,
                  modele_root: str | None = None,
                  eval_config_path: str | None = None) -> DatasetView:
    """终评入口：IID test 或正式 OOD；六项核验全过才返回 DatasetView。

    set_name="ood" 必须显式 ``ood=True``（fail-closed 形参确认）。
    """
    if set_name not in TERMINAL_SETS:
        raise SealedAccessError(f"非法终评集合: {set_name!r}")
    if set_name == "ood" and not ood:
        raise SealedAccessError(
            "OOD 访问缺少显式 ood=True 确认形参（fail-closed）")
    campaign = _load_campaign(campaign_id, campaign_dir)          # ①
    manifest = _check_preterminal_artifact(campaign, modele_root)  # ②③
    _check_timestamp_order(campaign, manifest)                    # ④
    _check_code_and_eval_config(campaign, eval_config_path)       # ⑤
    _check_status(campaign)                                       # ⑥
    if set_name == "test":
        view = DatasetView(iid_npz_path or IID_NPZ_PATH,
                           kind="iid_split", split="test")
    else:
        path = ood_npz_path or OOD_NPZ_PATH
        if not os.path.isfile(path):
            raise SealedAccessError(f"OOD 窗口工件不存在: {path}")
        view = DatasetView(path, kind="ood")
    AccessLogger(campaign_id, root=access_root).log(
        split=set_name, caller_summary=_caller_summary(),
        note="terminal_six_checks_passed")
    return view


def campaign_lifecycle_next(status: str) -> str:
    """生命周期推进合法性（DRAFT→…→COMPLETE 单向）；返回唯一合法下一态。"""
    if status not in LIFECYCLE_ORDER:
        raise ValueError(f"未知状态: {status}")
    i = LIFECYCLE_ORDER.index(status)
    if i == len(LIFECYCLE_ORDER) - 1:
        raise ValueError("COMPLETE 为终态，不可再推进")
    return LIFECYCLE_ORDER[i + 1]
