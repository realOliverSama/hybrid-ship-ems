#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_split.py — deterministic group-aware split 单元测试（T-011；DGN-09+R6）.

覆盖：顺序无关 / 同输入同 assignment（连跑两次 hash 一致）/ tie-breaking /
group→stratum 唯一性 fail-fast / 小 stratum n=1..5 全覆盖（三不变量 + 计数规则）/
artifact hash 确定性 / verify_partition 检出篡改。
"""

import copy
import json

import pytest

from dataset_v3 import split as split_mod
from dataset_v3.split import (assign_splits, compute_artifact_hash, fixup_counts,
                              target_counts, verify_partition, write_artifact)

# 冻结计数规则独立手算表（n_tr=(7n+5)//10, n_va=n_te=(3n+10)//20, 再 fixup）：
# n=1: 1/0/0（加 val 前 train 已有 1，sum<n 不触发）
# n=2: 1/1/0（sum=1<2 → +1 给最小计数，平手 train→val→test 之外的 val 优先于 test）
# n=3: 2/1/0
# n=4: 2/1/1（sum=5>4 → −1 给最大计数 train）
# n=5: 3/1/1
EXPECTED_COUNTS_N1_TO_N10 = {
    1: {"train": 1, "val": 0, "test": 0},
    2: {"train": 1, "val": 1, "test": 0},
    3: {"train": 2, "val": 1, "test": 0},
    4: {"train": 2, "val": 1, "test": 1},
    5: {"train": 3, "val": 1, "test": 1},
    6: {"train": 4, "val": 1, "test": 1},
    7: {"train": 5, "val": 1, "test": 1},
    8: {"train": 6, "val": 1, "test": 1},
    9: {"train": 6, "val": 2, "test": 1},
    10: {"train": 6, "val": 2, "test": 2},
}


def _groups(prefix: str, n: int) -> list:
    return [f"{prefix}{i:03d}" for i in range(n)]


# ---------------------------------------------------------------------------
# 确定性
# ---------------------------------------------------------------------------
def test_same_input_same_assignment_twice():
    groups = _groups("V", 40)
    strata = {"fam_A": groups[:13], "fam_B": groups[13:]}
    a1 = assign_splits(list(groups), copy.deepcopy(strata), split_seed="seed-2026")
    a2 = assign_splits(list(groups), copy.deepcopy(strata), split_seed="seed-2026")
    assert a1 == a2
    assert a1["artifact_sha256"] == a2["artifact_sha256"]
    assert a1["schema"] == "split_assignment_v3"
    assert verify_partition(a1) == []


def test_order_independence():
    groups = _groups("G", 40)
    strata = {"fam_A": groups[:13], "fam_B": groups[13:]}
    base = assign_splits(groups, strata, split_seed="s1")

    rev_groups = list(reversed(groups))
    rev_strata = {"fam_B": list(reversed(strata["fam_B"])),
                  "fam_A": list(reversed(strata["fam_A"]))}
    shuffled = assign_splits(rev_groups, rev_strata, split_seed="s1")
    assert shuffled == base
    assert shuffled["artifact_sha256"] == base["artifact_sha256"]


def test_seed_changes_artifact():
    groups = _groups("V", 30)
    strata = {"fam": groups}
    a1 = assign_splits(groups, strata, split_seed="seed-A")
    a2 = assign_splits(groups, strata, split_seed="seed-B")
    assert a1["split_seed"] == "seed-A"
    assert a2["split_seed"] == "seed-B"
    assert a1["artifact_sha256"] != a2["artifact_sha256"]


# ---------------------------------------------------------------------------
# 计数规则 / tie-breaking / fix-up
# ---------------------------------------------------------------------------
def test_target_counts_rule_n1_to_n10():
    for n, expect in EXPECTED_COUNTS_N1_TO_N10.items():
        got = fixup_counts(target_counts(n), n)
        assert got == expect, f"n={n}: {got} != {expect}"
        assert sum(got.values()) == n  # 完备性


def test_fixup_subtract_prefers_test_then_val_then_train():
    # sum>n 平手：test→val→train 优先
    got = fixup_counts({"train": 2, "val": 2, "test": 2}, 5)
    assert got == {"train": 2, "val": 2, "test": 1}


def test_fixup_add_prefers_train_then_val_then_test():
    # sum<n 平手：train→val→test 优先
    assert fixup_counts({"train": 1, "val": 0, "test": 0}, 2) == {"train": 1, "val": 1, "test": 0}
    assert fixup_counts({"train": 2, "val": 0, "test": 0}, 3) == {"train": 2, "val": 1, "test": 0}
    assert fixup_counts({"train": 0, "val": 0, "test": 0}, 0) == {"train": 0, "val": 0, "test": 0}


def test_tie_break_lexicographic_on_equal_u(monkeypatch):
    # u 全相同（工程上不可能，防御路径）→ 按 group_key 字典序切分
    monkeypatch.setattr(split_mod, "group_u", lambda seed, g: 0.5)
    groups = ["g03", "g01", "g02"]
    art = assign_splits(groups, {"fam": groups}, split_seed="s")
    # n=3 → 2/1/0：字典序前两个进 train
    assert art["strata"]["fam"]["train"] == ["g01", "g02"]
    assert art["strata"]["fam"]["val"] == ["g03"]
    assert art["strata"]["fam"]["test"] == []
    assert art["assignment"]["g01"]["split"] == "train"
    assert art["assignment"]["g03"]["split"] == "val"


# ---------------------------------------------------------------------------
# group→stratum 唯一性 fail-fast
# ---------------------------------------------------------------------------
def test_group_in_two_strata_raises():
    groups = ["a", "b", "c"]
    strata = {"s1": ["a", "b"], "s2": ["c", "a"]}  # a 跨两个 stratum
    with pytest.raises(ValueError, match="s1.*s2|同时出现"):
        assign_splits(groups, strata, split_seed="s")


def test_duplicate_within_stratum_raises():
    with pytest.raises(ValueError, match="重复"):
        assign_splits(["a", "b"], {"s1": ["a", "a"]}, split_seed="s")


def test_missing_or_extra_group_raises():
    with pytest.raises(ValueError, match="未分配"):
        assign_splits(["a", "b", "c"], {"s1": ["a", "b"]}, split_seed="s")
    with pytest.raises(ValueError, match="之外"):
        assign_splits(["a", "b"], {"s1": ["a", "b", "c"]}, split_seed="s")


def test_duplicate_in_groups_list_raises():
    with pytest.raises(ValueError, match="重复"):
        assign_splits(["a", "a"], {"s1": ["a", "a"]}, split_seed="s")


# ---------------------------------------------------------------------------
# 小 stratum n=1..5 全覆盖：三不变量 + 计数规则
# ---------------------------------------------------------------------------
@pytest.mark.parametrize("n", [1, 2, 3, 4, 5])
def test_small_stratum_full_coverage(n):
    groups = _groups("S", n)
    art = assign_splits(groups, {"fam": groups}, split_seed="tiny")
    expect = EXPECTED_COUNTS_N1_TO_N10[n]
    view = art["strata"]["fam"]
    got = {s: len(view[s]) for s in ("train", "val", "test")}
    assert got == expect
    assert sum(got.values()) == n
    # 三不变量（机器可读路径）：两两不交 / 并集=全部 / 计数一致
    assert verify_partition(art) == []
    tr = set(view["train"]); va = set(view["val"]); te = set(view["test"])
    assert not (tr & va) and not (tr & te) and not (va & te)          # ①
    assert tr | va | te == set(groups)                                 # ②
    assert art["counts"] == {"train": len(tr), "val": len(va), "test": len(te)}


def test_multi_stratum_invariants():
    groups = _groups("M", 45)
    strata = {"fam1": groups[:7], "fam2": groups[7:12], "fam3": groups[12:]}
    art = assign_splits(groups, strata, split_seed="multi")
    assert verify_partition(art) == []
    # 逐 stratum 计数规则
    for name, members in strata.items():
        got = {s: len(art["strata"][name][s]) for s in ("train", "val", "test")}
        assert got == fixup_counts(target_counts(len(members)), len(members))
    # 总 counts = 各 stratum 之和
    assert art["counts"]["train"] == sum(len(art["strata"][k]["train"]) for k in strata)
    # assignment 与 strata 视图双向一致
    assert len(art["assignment"]) == 45
    for g, info in art["assignment"].items():
        assert g in art["strata"][info["stratum"]][info["split"]]


# ---------------------------------------------------------------------------
# artifact 落盘 + hash 确定性 + 篡改检出
# ---------------------------------------------------------------------------
def test_artifact_write_roundtrip_and_hash(tmp_path):
    groups = _groups("W", 20)
    art = assign_splits(groups, {"fam": groups}, split_seed="io")
    path = tmp_path / "split_assignment_v3.json"
    write_artifact(art, str(path))
    loaded = json.loads(path.read_text(encoding="utf-8"))
    assert loaded == art
    assert loaded["artifact_sha256"] == compute_artifact_hash(loaded)
    assert verify_partition(loaded) == []
    # 字节级确定性：连写两次内容一致
    path2 = tmp_path / "split_assignment_v3_again.json"
    write_artifact(art, str(path2))
    assert path.read_bytes() == path2.read_bytes()


def test_verify_partition_detects_tamper():
    groups = _groups("T", 12)
    art = assign_splits(groups, {"fam": groups}, split_seed="tamper")

    # 篡改 1：assignment 中翻转一个 split
    bad = copy.deepcopy(art)
    g0 = bad["strata"]["fam"]["train"][0]
    bad["assignment"][g0]["split"] = "test"
    errs = verify_partition(bad)
    assert errs and any(g0 in e for e in errs)

    # 篡改 2：strata 视图中移除一个 group（完备性）
    bad2 = copy.deepcopy(art)
    g1 = bad2["strata"]["fam"]["val"][0]
    bad2["strata"]["fam"]["val"].remove(g1)
    errs2 = verify_partition(bad2)
    assert errs2 and any(g1 in e for e in errs2)

    # 篡改 3：artifact hash 不自洽
    bad3 = copy.deepcopy(art)
    bad3["artifact_sha256"] = "0" * 64
    errs3 = verify_partition(bad3)
    assert errs3 and any("artifact_sha256" in e for e in errs3)

    # 篡改 4：stratum 计数违反冻结规则（把一个 test 组塞进 val）
    bad4 = copy.deepcopy(art)
    if bad4["strata"]["fam"]["test"]:
        g2 = bad4["strata"]["fam"]["test"][0]
        bad4["strata"]["fam"]["test"].remove(g2)
        bad4["strata"]["fam"]["val"].append(g2)
        bad4["strata"]["fam"]["val"].sort()
        errs4 = verify_partition(bad4)
        assert errs4 and any("计数" in e for e in errs4)
