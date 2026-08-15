#!/usr/bin/env python3
"""fw_gate.py — STM32N6 固件提交质量门（Agent commit 前置条件）

对单个 AlienTek STM32N6 CubeMX/CMake 项目执行机器可判定的检查，
全部通过才允许提交。任何 FAIL → exit 1。

检查项（对应 SoftwarePackage/AGENTS.md 条款 + prepare skill 静态检查能力）：
  ①  debug_build       Debug 构建成功（经 atk_stm32n6_cmake_prepare.py --build，
                        含 0x34000400 ELF 最低地址校验）
  ②  release_build     Release 构建成功（含 0x70100400 ELF/HEX 地址校验）
  ③  debug_layout      Debug ELF 全部 ALLOC 段（section，含 NOBITS）在 SRAM 契约区间
                        [0x34000400, 0x34200000)
  ④  release_layout    Release ELF 全部 ALLOC 段（section，含 NOBITS）在外部 Flash / RAM 契约区间
                        [0x70100400, 0x72000000) ∪ [0x34000000, 0x34200000)
  ⑤  ioc_unchanged     .ioc 相对 HEAD 未被修改（授权时设 FW_GATE_ALLOW_IOC=1）
  ⑥  no_forbidden_copy 项目内无 prepare 脚本 / BSP 的违规副本
  ⑦  warnings          构建警告计数（默认只记录；--max-warnings N 时超限 FAIL）

输出: fw_gate_result.json（机器裁决，默认写入本脚本所在目录）
用法: python3 fw_gate.py <project_path> [--skip-build] [--max-warnings N]
退出码: 0 = 全部通过；1 = 存在 FAIL；2 = 用法/环境错误
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

# ---- 地址契约（当前 AlienTek profile 的已验证期望值，见 prepare skill） ----
# Debug（LRUN，纯 SRAM 运行）：全部段落在 RAM 契约区间，向量表精确位于 0x34000400
DEBUG_REGIONS = [(0x34000400, 0x34200000)]
DEBUG_VECTOR = 0x34000400
# Release（外部 Flash + RAM）：代码在 Flash 区间，数据/堆栈在 RAM 区间
RELEASE_REGIONS = [(0x70100400, 0x72000000), (0x34000000, 0x34200000)]
RELEASE_VECTOR = 0x70100400

SCRIPT_DIR = Path(__file__).resolve().parent
PREPARE_SCRIPT_REL = Path(
    "firmware/stm32n6/04_SourceCode/SoftwarePackage/Tools/atk_stm32n6_cmake_prepare.py"
)


def build_env() -> dict:
    """构建环境：cube-cmake 需要 `cube` CLI（ST VSCode 扩展捆绑）。
    pi/终端的 PATH 通常不含它，这里按已知扩展路径自动补齐。"""
    env = dict(os.environ)
    import glob
    import platform

    sysname = {"Darwin": "darwin", "Linux": "linux", "Windows": "windows"}.get(
        platform.system(), "darwin"
    )
    machine = {"arm64": "aarch64", "x86_64": "x86_64", "AMD64": "x86_64"}.get(
        platform.machine(), platform.machine()
    )
    if not _which("cube", env):
        pattern = os.path.expanduser(
            f"~/.vscode/extensions/stmicroelectronics.stm32cube-ide-core-*/"
            f"resources/binaries/{sysname}/{machine}"
        )
        for d in sorted(glob.glob(pattern)):
            if (Path(d) / "cube").exists() or (Path(d) / "cube.exe").exists():
                env["PATH"] = d + os.pathsep + env.get("PATH", "")
                break
    return env


def _which(name: str, env) -> bool:
    for d in env.get("PATH", "").split(os.pathsep):
        if d and ((Path(d) / name).exists() or (Path(d) / f"{name}.exe").exists()):
            return True
    return False


def find_repo_root(start: Path) -> Path:
    r = subprocess.run(
        ["git", "-C", str(start), "rev-parse", "--show-toplevel"],
        capture_output=True, text=True,
    )
    if r.returncode != 0:
        raise SystemExit(f"错误：{start} 不在 git 仓库内")
    return Path(r.stdout.strip())


def elf_alloc_sections(elf: Path):
    """解析 32 位小端 ELF 的 ALLOC 段（含 NOBITS），返回 [(name, addr, end)]。
    按段（section）而非 PT_LOAD 段判定：段起始地址可能被链接器按对齐下取整，
    属于正常填充，不代表内容越界。"""
    data = elf.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise ValueError(f"不是 ELF 文件: {elf}")
    if data[4] != 1 or data[5] != 1:
        raise ValueError(f"期望 32 位小端 ELF: {elf}")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data, 0)
    shoff, shentsize, shnum, shstrndx = header[6], header[11], header[12], header[13]
    if shoff == 0 or shnum == 0:
        raise ValueError(f"ELF 无段表（可能被 strip）: {elf}")
    sections = []
    for i in range(shnum):
        off = shoff + i * shentsize
        if off + 40 > len(data):
            raise ValueError(f"ELF 段表截断: {elf}")
        sh = struct.unpack_from("<IIIIIIIIII", data, off)
        sections.append(sh)  # (name, type, flags, addr, offset, size, ...)
    # 段名字符串表
    str_off = sections[shstrndx][4]

    def name_of(sh):
        start = str_off + sh[0]
        end = data.find(b"\x00", start)
        return data[start:end].decode("ascii", "replace")

    result = []
    for sh in sections:
        SHF_ALLOC = 0x2
        if sh[2] & SHF_ALLOC and sh[5] > 0:
            result.append((name_of(sh), sh[3], sh[3] + sh[5]))
    return result


class Gate:
    def __init__(self, project: Path, repo: Path, skip_build: bool, max_warnings):
        self.project = project
        self.repo = repo
        self.skip_build = skip_build
        self.max_warnings = max_warnings
        self.results = []
        self.warning_count = 0

    def check(self, name: str, passed: bool, detail: str):
        self.results.append({"check": name, "pass": bool(passed), "detail": detail})
        tag = "PASS" if passed else "FAIL"
        print(f"[fw-gate] [{tag}] {name}: {detail}")

    # ---- ①② 构建（复用项目既定入口，含脚本自带地址校验） ----
    def run_builds(self) -> bool:
        prepare = self.repo / PREPARE_SCRIPT_REL
        if not prepare.is_file():
            self.check("prepare_script", False, f"未找到 {prepare}")
            return False
        ok = True
        for cfg in ("Debug", "Release"):
            code, out = self._build_once(prepare, cfg)
            # 陈旧缓存自愈：仅当输出为“目录迁移导致缓存失配”特征时，
            # 删除对应的 Appli/build/<cfg> 并重试一次（目标唯一、可再生成）
            if code != 0 and (
                "does not match the source" in out or "is different than the directory" in out
            ):
                import shutil
                stale = self.project / "Appli" / "build" / cfg
                print(f"[fw-gate] 检测到陈旧 CMake 缓存，清除后重试: {stale}")
                shutil.rmtree(stale, ignore_errors=True)
                code, out = self._build_once(prepare, cfg)
            self.warning_count += len(re.findall(r"\bwarning:", out, re.IGNORECASE))
            tail = " / ".join(out.strip().splitlines()[-2:])[:200]
            self.check(
                f"{cfg.lower()}_build",
                code == 0,
                f"exit={code}; {tail}",
            )
            ok = ok and code == 0
        return ok

    def _build_once(self, prepare: Path, cfg: str):
        r = subprocess.run(
            ["python3", str(prepare), str(self.project), "--build", cfg],
            capture_output=True, text=True, env=build_env(),
        )
        return r.returncode, r.stdout + r.stderr

    # ---- ③④ ELF 段布局契约（按 ALLOC 段判定） ----
    def check_layout(self, cfg: str, regions, vector_addr: int):
        build_dir = self.project / "Appli" / "build" / cfg
        elfs = sorted(build_dir.rglob("*.elf")) if build_dir.is_dir() else []
        name = f"{cfg.lower()}_layout"
        if len(elfs) != 1:
            self.check(name, False, f"{build_dir} 下找到 {len(elfs)} 个 ELF（期望 1 个）")
            return
        elf = elfs[0]
        try:
            sections = elf_alloc_sections(elf)
        except ValueError as e:
            self.check(name, False, str(e))
            return
        bad = []
        for nm, s, e in sections:
            if not any(lo <= s and e <= hi for lo, hi in regions):
                bad.append(f"{nm}@0x{s:08X}-0x{e:08X}")
        vec = [s for nm, s, _e in sections if nm == ".isr_vector"]
        vec_ok = len(vec) == 1 and vec[0] == vector_addr
        region_txt = " ∪ ".join(f"[0x{lo:08X},0x{hi:08X})" for lo, hi in regions)
        if bad or not vec_ok:
            parts = []
            if bad:
                parts.append(f"越界段: {', '.join(bad[:4])}")
            if not vec_ok:
                got = f"0x{vec[0]:08X}" if vec else "缺失"
                parts.append(f".isr_vector={got}（期望 0x{vector_addr:08X}）")
            self.check(name, False, f"{elf.name} {'; '.join(parts)}（契约 {region_txt}）")
        else:
            self.check(
                name, True,
                f"{elf.name} {len(sections)} 个 ALLOC 段均在 {region_txt} 内，"
                f".isr_vector=0x{vector_addr:08X}",
            )

    # ---- ⑤ .ioc 保护 ----
    def check_ioc(self):
        if os.environ.get("FW_GATE_ALLOW_IOC") == "1":
            self.check("ioc_unchanged", True, "FW_GATE_ALLOW_IOC=1，用户已授权 .ioc 修改")
            return
        r = subprocess.run(
            ["git", "-C", str(self.repo), "status", "--porcelain",
             "--", str(self.project.relative_to(self.repo))],
            capture_output=True, text=True,
        )
        if r.returncode != 0:
            self.check("ioc_unchanged", False, f"git status 失败: {r.stderr.strip() or r.stdout.strip()}")
            return
        touched = [ln for ln in r.stdout.splitlines() if ln[3:].strip().endswith(".ioc")]
        if touched:
            self.check(
                "ioc_unchanged", False,
                f".ioc 被修改且未授权（{len(touched)} 个）；"
                "如已获用户授权，设 FW_GATE_ALLOW_IOC=1 重跑",
            )
        else:
            self.check("ioc_unchanged", True, ".ioc 与 HEAD 一致")

    # ---- ⑥ 违规副本 ----
    def check_forbidden_copies(self):
        bad = []
        for p in self.project.rglob("atk_stm32n6_cmake_prepare.py"):
            if ".git" not in p.parts:
                bad.append(str(p.relative_to(self.project)))
        bsp = self.project / "Drivers" / "BSP"
        if bsp.is_dir():
            bad.append("Drivers/BSP")
        if bad:
            self.check("no_forbidden_copy", False, f"项目内违规副本: {', '.join(bad)}")
        else:
            self.check("no_forbidden_copy", True, "无 prepare 脚本 / BSP 副本")

    # ---- ⑦ 警告计数 ----
    def check_warnings(self):
        if self.max_warnings is None:
            self.check("warnings", True, f"{self.warning_count} 个警告（仅记录，未设上限）")
        else:
            self.check(
                "warnings",
                self.warning_count <= self.max_warnings,
                f"{self.warning_count} 个警告（上限 {self.max_warnings}）",
            )

    def run(self) -> int:
        builds_ok = True
        build_attempted = {}
        if not self.skip_build:
            builds_ok = self.run_builds()
            build_attempted = {r["check"]: r["pass"] for r in self.results}
        # 布局检查依赖构建产物；构建失败的配置跳过（避免陈旧产物误报）
        for cfg, regions, vec in (
            ("Debug", DEBUG_REGIONS, DEBUG_VECTOR),
            ("Release", RELEASE_REGIONS, RELEASE_VECTOR),
        ):
            if build_attempted and not build_attempted.get(f"{cfg.lower()}_build", False):
                self.check(f"{cfg.lower()}_layout", False, "构建失败，跳过布局检查（产物可能陈旧）")
            else:
                self.check_layout(cfg, regions, vec)
        self.check_ioc()
        self.check_forbidden_copies()
        self.check_warnings()

        overall = all(r["pass"] for r in self.results) and builds_ok
        verdict = {
            "overall": overall,
            "project": str(self.project),
            "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "checks": self.results,
        }
        out_path = SCRIPT_DIR / "fw_gate_result.json"
        out_path.write_text(json.dumps(verdict, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"[fw-gate] 裁决: {out_path}")
        print(f"[fw-gate] 总体: {'全部通过' if overall else '存在失败项，禁止提交'}")
        return 0 if overall else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="STM32N6 固件提交质量门")
    ap.add_argument("project", help="项目根目录（含 Appli/ 与 .ioc）")
    ap.add_argument("--skip-build", action="store_true", help="跳过构建，仅做静态检查（用现有产物）")
    ap.add_argument("--max-warnings", type=int, default=None, help="构建警告上限（默认只记录）")
    args = ap.parse_args()

    project = Path(args.project).expanduser().resolve()
    if not (project / "Appli").is_dir():
        print(f"错误：{project} 不含 Appli/，不是有效的 STM32N6 项目", file=sys.stderr)
        return 2
    repo = find_repo_root(project)
    gate = Gate(project, repo, args.skip_build, args.max_warnings)
    return gate.run()


if __name__ == "__main__":
    sys.exit(main())
