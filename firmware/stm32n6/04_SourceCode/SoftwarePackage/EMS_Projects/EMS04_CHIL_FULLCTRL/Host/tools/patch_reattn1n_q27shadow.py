#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EMS04 S3-C Q27 影子拷贝诊断：reattn1n.c 确定性补丁（qualification-only）。

用途（用户裁决 §1-§4 APPROVED，2026-09-26）：
    对生成码 Model/reatt8_q_npu/npu_n1/gen/reattn1n.c 做**单一锚点插入**，
    在 Q27 张量（int8 [1,300,11]，物理 0x34350000+28800）的
    LL_ATON_Cache_MCU_Clean_Range 之后插入一次
    ems04_s3c_q27_shadow_capture(...) 调用，使 Q27 在被后续 epoch 复用覆盖前
    被影子拷贝取证（Q27 是否 == HOST_B 决定 S3-C 走 FAIL 还是
    N1_TARGET_EXECUTION 分支）。同时补 include。

非侵入性论证：
    插入行为 = 一次 memcpy 调用（对响应 Q27 生命周期仅读）；无数学改动、
    无控制流改动、无 NPU 输入改动、无图字节（EC blobs/权重）改动。原
    Clean_Range 行原样保留。生成码原文件备份为 reattn1n.c.orig_q27，
    全部 SHA256 记入 q27shadow_provenance.json，可 --restore 精确还原。

用法（在本工程根 EMS04_CHIL_FULLCTRL/ 下，相对路径，禁止绝对路径）：
    补丁:   python3 Host/tools/patch_reattn1n_q27shadow.py
    还原:   python3 Host/tools/patch_reattn1n_q27shadow.py --restore

重跑行为（幂等口径，二选一之"报告已补丁"）：
    目标文件已含插入调用且备份存在时，打印 "already patched" 并以 0 退出，
    不改写任何文件；已补丁但备份缺失（无法溯源）时报错非零。

备份覆盖策略：
    .orig_q27 已存在且与当前目标文件内容一致（即目标处于原始态）→ 复用既有
    备份（字节全同，无覆盖发生）；已存在但内容不一致 → 拒绝执行（防止以未知
    内容覆盖原始态备份），退出非零。

构建流程（诊断构建）：
    1) python3 Host/tools/patch_reattn1n_q27shadow.py
    2) cmake -S Appli -B Appli/build/Q27Shadow -G Ninja \\
           -DCMAKE_TOOLCHAIN_FILE=$PWD/gcc-arm-none-eabi.cmake \\
           -DCMAKE_BUILD_TYPE=Release -DEMS04_PRED_PARITY=1 -DEMS04_Q27_SHADOW=1
    3) cmake --build Appli/build/Q27Shadow
    注意：reattn1n.c 的插入调用不条件编译——若生成码已补丁而构建未带
    -DEMS04_Q27_SHADOW=1（或未编 ems04_s3c_q27_shadow.c），链接将报
    undefined reference to ems04_s3c_q27_shadow_capture；此时先 --restore
    或改用影子构建。Appli/CMakeLists.txt 末尾块在 EMS04_Q27_SHADOW=0 时
    有对应 STATUS 提示。
"""

import hashlib
import json
import re
import sys
import time
from pathlib import Path

# 相对路径纪律（AGENTS.md §20.2）：全部路径由脚本自身位置推导
SCRIPT_PATH = Path(__file__).resolve()
PROJECT_ROOT = SCRIPT_PATH.parent.parent.parent          # Host/tools/ -> 工程根
TARGET_REL = Path("Model/reatt8_q_npu/npu_n1/gen/reattn1n.c")
BACKUP_REL = TARGET_REL.with_name(TARGET_REL.name + ".orig_q27")
PROVENANCE_REL = TARGET_REL.with_name("q27shadow_provenance.json")

TARGET = PROJECT_ROOT / TARGET_REL
BACKUP = PROJECT_ROOT / BACKUP_REL
PROVENANCE = PROJECT_ROOT / PROVENANCE_REL

# 单一锚点（原文行，含前导两空格）：Q27 clean 到物理的 cache 维护调用。
# 要求精确子串在文件中恰好出现 1 次，否则报错退出，不写任何文件。
ANCHOR_LINE = (
    "  LL_ATON_Cache_MCU_Clean_Range(((uintptr_t)(ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR("
    "0x34350000UL + 28800))) /* Equivalent hex address = 0x34357080UL */, 3328);"
)

# 锚点后插入的调用行（逐字，见 patch 后 git diff）
INSERT_LINE = (
    "  ems04_s3c_q27_shadow_capture((const void *)"
    "(ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(0x34350000UL + 28800)));"
    " /* S3-C Q27 diagnostic shadow (qualification-only, see"
    " Host/tools/patch_reattn1n_q27shadow.py) */"
)

# include 插入锚：生成码 include 块内唯一的一行 #include "ll_aton.h"
INCLUDE_ANCHOR_RE = re.compile(r'^#include "ll_aton\.h"\r?$', re.M)
INCLUDE_INSERT_LINE = '#include "ems04_s3c_q27_shadow.h"'


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fail(msg: str) -> "None":
    print(f"[q27shadow] ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def read_target() -> bytes:
    if not TARGET.is_file():
        fail(f"target not found: {TARGET_REL} (run from EMS04 project root)")
    return TARGET.read_bytes()


def require_pristine_or_existing_backup(original: bytes) -> None:
    """备份存在性检查：存在且内容与当前文件一致 → 复用；存在但不一致 → 拒绝。"""
    if BACKUP.exists():
        existing = BACKUP.read_bytes()
        if existing == original:
            print("[q27shadow] backup exists and matches current file; reusing it")
        else:
            fail(
                f"refusing to overwrite existing backup {BACKUP_REL} "
                "(content differs from current target; unknown original state)"
            )


def do_patch() -> None:
    data = read_target()

    if INSERT_LINE.encode("utf-8") in data:
        if BACKUP.is_file():
            print(f"[q27shadow] already patched: {TARGET_REL} contains the "
                  f"shadow capture call (provenance: {PROVENANCE_REL}); "
                  "nothing to do")
            return
        fail(f"{TARGET_REL} already patched but backup {BACKUP_REL} is missing; "
             "cannot verify provenance")

    text = data.decode("utf-8")

    # --- 锚点唯一性校验（精确子串）---
    if text.count(ANCHOR_LINE) != 1:
        fail(f"anchor line found {text.count(ANCHOR_LINE)} times (expected 1); "
             "generated code drift - aborting without writing any file")

    # --- include 锚唯一性校验（整行正则）---
    if len(INCLUDE_ANCHOR_RE.findall(text)) != 1:
        fail('include anchor (#include "ll_aton.h") not unique/missing; '
             "aborting without writing any file")

    # --- 备份（拒绝覆盖未知内容，见模块 docstring）---
    require_pristine_or_existing_backup(data)
    if not BACKUP.exists():
        BACKUP.write_bytes(data)
        print(f"[q27shadow] backup written: {BACKUP_REL}")

    # --- 单一锚点插入：锚点行后插入 capture 调用 ---
    patched_text = text.replace(
        ANCHOR_LINE, ANCHOR_LINE + "\n" + INSERT_LINE, 1
    )
    # --- include 插入：#include "ll_aton.h" 行后 ---
    patched_text = INCLUDE_ANCHOR_RE.sub(
        lambda m: m.group(0) + "\n" + INCLUDE_INSERT_LINE, patched_text, count=1
    )

    patched = patched_text.encode("utf-8")

    # --- 落盘 ---
    TARGET.write_bytes(patched)

    # --- provenance ---
    provenance = {
        "ORIGINAL_REATTN1N_SHA": sha256_bytes(data),
        "PATCH_SCRIPT_SHA": sha256_bytes(SCRIPT_PATH.read_bytes()),
        "PATCHED_REATTN1N_SHA": sha256_bytes(patched),
        "TARGET_FILE": TARGET_REL.as_posix(),
        "BACKUP_FILE": BACKUP_REL.as_posix(),
        "ANCHOR_LINE": ANCHOR_LINE,
        "INSERT_LINE": INSERT_LINE,
        "INCLUDE_ANCHOR_LINE": '#include "ll_aton.h"',
        "INCLUDE_INSERT_LINE": INCLUDE_INSERT_LINE,
        "TIMESTAMP_UTC": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "NOTE": "S3-C Q27 shadow capture (qualification-only); "
                "restore with: python3 Host/tools/patch_reattn1n_q27shadow.py --restore",
    }
    PROVENANCE.write_text(json.dumps(provenance, indent=2, ensure_ascii=False) + "\n")

    print(f"[q27shadow] patched: {TARGET_REL}")
    print(f"[q27shadow] provenance: {PROVENANCE_REL}")
    print("[q27shadow] next: cmake -DEMS04_Q27_SHADOW=1 (see script docstring)")


def do_restore() -> None:
    data = read_target()
    if not BACKUP.is_file():
        fail(f"backup not found: {BACKUP_REL}; nothing to restore")
    backup = BACKUP.read_bytes()
    if data == backup:
        print(f"[q27shadow] already restored: {TARGET_REL} == {BACKUP_REL}")
        return
    TARGET.write_bytes(backup)
    print(f"[q27shadow] restored: {TARGET_REL} from {BACKUP_REL}")
    print(f"[q27shadow] note: {PROVENANCE_REL} left untouched "
          f"(PATCHED_REATTN1N_SHA now intentionally mismatched)")
    # 完整性自证
    if TARGET.read_bytes() == backup:
        print("[q27shadow] restore verified byte-exact")


def main() -> None:
    if "--restore" in sys.argv[1:]:
        do_restore()
    else:
        do_patch()


if __name__ == "__main__":
    main()
