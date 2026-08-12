#!/usr/bin/env python3
"""Prepare and build an ALIENTEK STM32N6 CMake project.

Keep this script in SoftwarePackage/Tools and pass the project directory:

    cd SoftwarePackage
    python3 Tools/atk_stm32n6_cmake_prepare.py Projects/03_Button2

With no action option it performs repeatable project preparation:

1. If Secure_nsclib/secure_nsc.h defines IAC_ERROR_CB_ID but not
   GTZC_ERROR_CB_ID, replace the stale GTZC token in Appli C sources.
2. Detect ALIENTEK BSP components referenced by Appli sources and add their
   include directories and top-level C sources to Appli/CMakeLists.txt.
   Flat ALIENTEK middleware (for example Middlewares/USMART) referenced by
   quoted includes is added the same way, together with a matching
   project-local Appli/<NAME> configuration directory when one exists.
3. Configure the ALIENTEK Debug and Release linker scripts and memory maps.
4. Generate address-bearing Release HEX/BIN files under Appli/build/flash.
5. Generate VS Code settings, build tasks and STM32 launch configurations.

It can also build a selected configuration and verify its load address.  The
generated Release HEX is intended to be programmed manually with the
STM32CubeProgrammer application.

The generated CMake block is marker-delimited and replaced on later runs, so
running the script repeatedly does not duplicate CMake entries.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple


BLOCK_BEGIN = "# >>> ATK STM32N6 AUTO BSP BEGIN >>>"
BLOCK_END = "# <<< ATK STM32N6 AUTO BSP END <<<"
MW_BLOCK_BEGIN = "# >>> ATK STM32N6 AUTO MIDDLEWARE BEGIN >>>"
MW_BLOCK_END = "# <<< ATK STM32N6 AUTO MIDDLEWARE END <<<"
LINKER_BEGIN = "# >>> ATK STM32N6 AUTO LINKER BEGIN >>>"
LINKER_END = "# <<< ATK STM32N6 AUTO LINKER END <<<"
ARTIFACT_BEGIN = "# >>> ATK STM32N6 AUTO ARTIFACTS BEGIN >>>"
ARTIFACT_END = "# <<< ATK STM32N6 AUTO ARTIFACTS END <<<"
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
QUOTED_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.MULTILINE)
DEVICE_NAME = "STM32N647X0H3Q"
DEVICE_CORE = "Cortex-M55"
SCRIPT_VERSION = "3.4.0"
DEBUG_ORIGIN = 0x34000400
RELEASE_ORIGIN = 0x70100400

DEBUG_MEMORY = """MEMORY
{
  RAM    (xrw) : ORIGIN = 0x34000400, LENGTH = 2047K
  EXTRAM (rw)  : ORIGIN = 0x90000000, LENGTH = 32M
}"""

RELEASE_MEMORY = """MEMORY
{
  ROM    (xrw) : ORIGIN = 0x70100400, LENGTH = 31743K
  RAM    (xrw) : ORIGIN = 0x34000000, LENGTH = 2048K
  EXTRAM (rw)  : ORIGIN = 0x90000000, LENGTH = 32M
}"""


class PrepareError(RuntimeError):
    """A user-actionable project preparation error."""


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Patch CubeMX STM32N6 CMake projects for ALIENTEK BSP use."
    )
    parser.add_argument(
        "--version", action="version", version=f"%(prog)s {SCRIPT_VERSION}"
    )
    parser.add_argument(
        "project",
        nargs="?",
        default=".",
        help="Project root containing Appli/ and an .ioc file (default: current directory).",
    )
    parser.add_argument(
        "--add",
        nargs="*",
        default=[],
        metavar="COMPONENT",
        help="Additional BSP components to include, for example: --add KEY LED SYS.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show planned changes without writing files.",
    )
    parser.add_argument(
        "--no-secure-fix",
        action="store_true",
        help="Do not patch GTZC_ERROR_CB_ID in Appli sources.",
    )
    parser.add_argument(
        "--no-linker-fix",
        action="store_true",
        help="Do not patch/select the ALIENTEK Debug and Release linker scripts.",
    )
    parser.add_argument(
        "--no-artifacts",
        action="store_true",
        help="Do not add Release HEX/BIN post-build generation.",
    )
    parser.add_argument(
        "--no-vscode",
        action="store_true",
        help="Do not create or merge .vscode settings/tasks/launch files.",
    )
    parser.add_argument(
        "--build",
        choices=("Debug", "Release", "all"),
        help="Prepare, then build and verify the selected configuration(s).",
    )
    return parser.parse_args(argv)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return path.read_text(encoding="utf-8", errors="replace")


def backup_once(path: Path, dry_run: bool) -> None:
    backup = path.with_name(path.name + ".atk-backup")
    if backup.exists() or dry_run:
        return
    shutil.copy2(path, backup)


def write_if_changed(path: Path, content: str, dry_run: bool) -> bool:
    original = read_text(path)
    if original == content:
        return False
    if not dry_run:
        backup_once(path, dry_run=False)
        path.write_text(content, encoding="utf-8")
    return True


def validate_project(project: Path) -> Path:
    if not project.is_dir():
        raise PrepareError(f"Project directory does not exist: {project}")
    appli = project / "Appli"
    cmake = appli / "CMakeLists.txt"
    if not appli.is_dir():
        raise PrepareError(f"Missing directory: {appli}")
    if not cmake.is_file():
        raise PrepareError(
            f"Missing {cmake}. Generate the project as CMake in CubeMX first."
        )
    if not list(project.glob("*.ioc")):
        print("Warning: no .ioc file found in the project root.", file=sys.stderr)
    return appli


def find_bsp_root(project: Path) -> Path:
    for base in (project, *project.parents):
        candidate = base / "Drivers" / "BSP"
        if candidate.is_dir():
            return candidate.resolve()
    raise PrepareError(
        "Could not find Drivers/BSP while walking upward from the project. "
        "Keep the project under the ALIENTEK SoftwarePackage tree."
    )


def component_map(bsp_root: Path) -> Dict[str, Path]:
    result: Dict[str, Path] = {}
    for item in sorted(bsp_root.iterdir(), key=lambda p: p.name.casefold()):
        if item.is_dir() and not item.name.startswith("."):
            result[item.name.casefold()] = item
    return result


def source_files(root: Path) -> Iterable[Path]:
    for suffix in ("*.c", "*.h", "*.cpp", "*.hpp"):
        yield from root.rglob(suffix)


def included_components(files: Iterable[Path], components: Dict[str, Path]) -> Set[str]:
    found: Set[str] = set()
    for path in files:
        text = read_text(path)
        for include in INCLUDE_RE.findall(text):
            parts = [part for part in include.replace("\\", "/").split("/") if part not in ("", ".", "..")]
            for part in parts[:-1]:
                key = part.casefold()
                if key in components:
                    found.add(key)
                    break
    return found


def resolve_components(
    appli: Path,
    components: Dict[str, Path],
    explicitly_added: Sequence[str],
    extra_files: Iterable[Path] = (),
) -> List[Path]:
    selected = included_components(
        list(source_files(appli)) + list(extra_files), components
    )

    for name in explicitly_added:
        key = name.casefold()
        if key not in components:
            available = ", ".join(path.name for path in components.values())
            raise PrepareError(f"Unknown BSP component '{name}'. Available: {available}")
        selected.add(key)

    # Follow includes between selected BSP components until no new dependency appears.
    while True:
        dependency_files: List[Path] = list(extra_files)
        for key in selected:
            dependency_files.extend(source_files(components[key]))
        expanded = selected | included_components(dependency_files, components)
        if expanded == selected:
            break
        selected = expanded

    return sorted((components[key] for key in selected), key=lambda p: p.name.casefold())


def find_middlewares_root(project: Path) -> Optional[Path]:
    for base in (project, *project.parents):
        candidate = base / "Middlewares"
        if candidate.is_dir():
            return candidate.resolve()
    return None


def flat_middleware_map(mw_root: Path) -> Dict[str, Path]:
    """ALIENTEK flat middleware: no subdirectories, top-level C sources.

    ST-style middleware with port subdirectories (FATFS, THREADX, USBX, ...)
    needs per-project integration and is deliberately left out.
    """
    result: Dict[str, Path] = {}
    for item in sorted(mw_root.iterdir(), key=lambda p: p.name.casefold()):
        if not item.is_dir() or item.name.startswith("."):
            continue
        if any(child.is_dir() for child in item.iterdir()):
            continue
        if not list(item.glob("*.c")):
            continue
        result[item.name.casefold()] = item
    return result


def quoted_include_basenames(files: Iterable[Path]) -> Set[str]:
    names: Set[str] = set()
    for path in files:
        for include in QUOTED_INCLUDE_RE.findall(read_text(path)):
            names.add(include.replace("\\", "/").split("/")[-1].casefold())
    return names


def matched_middlewares(
    files: Iterable[Path], components: Dict[str, Path]
) -> Set[str]:
    """Select middleware whose top-level headers are quoted-included.

    Only quoted includes count: angle-bracket <malloc.h> and friends must not
    pull in the ALIENTEK MALLOC component by accident.
    """
    names = quoted_include_basenames(files)
    found: Set[str] = set()
    for key, path in components.items():
        headers = {header.name.casefold() for header in path.glob("*.h")}
        if names & headers:
            found.add(key)
    return found


def resolve_middlewares(appli: Path, components: Dict[str, Path]) -> List[Path]:
    files = list(source_files(appli))
    selected = matched_middlewares(files, components) | included_components(
        files, components
    )
    while True:
        dependency_files: List[Path] = []
        for key in selected:
            dependency_files.extend(source_files(components[key]))
        expanded = (
            selected
            | included_components(dependency_files, components)
            | matched_middlewares(dependency_files, components)
        )
        if expanded == selected:
            break
        selected = expanded
    return sorted(
        (components[key] for key in selected), key=lambda p: p.name.casefold()
    )


def local_middleware_dirs(
    appli: Path, selected: Sequence[Path]
) -> Dict[str, Path]:
    """Project-local config dirs such as Appli/USMART matching a component."""
    result: Dict[str, Path] = {}
    if not selected:
        return result
    names = {path.name.casefold(): path.name for path in selected}
    for item in sorted(appli.iterdir(), key=lambda p: p.name.casefold()):
        key = item.name.casefold()
        if not item.is_dir() or item.name.startswith(".") or key not in names:
            continue
        if list(item.glob("*.c")) or list(item.glob("*.h")):
            result[names[key]] = item
    return result


def patch_secure_sources(project: Path, appli: Path, dry_run: bool) -> List[Path]:
    header = project / "Secure_nsclib" / "secure_nsc.h"
    if not header.is_file():
        return []

    header_text = read_text(header)
    has_iac = re.search(r"\bIAC_ERROR_CB_ID\b", header_text) is not None
    has_gtzc = re.search(r"\bGTZC_ERROR_CB_ID\b", header_text) is not None
    if not has_iac or has_gtzc:
        return []

    changed: List[Path] = []
    for path in appli.rglob("*.c"):
        original = read_text(path)
        updated = re.sub(r"\bGTZC_ERROR_CB_ID\b", "IAC_ERROR_CB_ID", original)
        if updated != original:
            if write_if_changed(path, updated, dry_run):
                changed.append(path)
    return changed


def choose_linker_script(appli: Path, pattern: str) -> Path:
    candidates = sorted(
        appli.glob(pattern),
        key=lambda path: (not path.name.startswith("ATK_"), path.name.casefold()),
    )
    if not candidates:
        raise PrepareError(
            f"No linker script matching '{pattern}' was found in {appli}. "
            "Regenerate the Appli context in CubeMX or use --no-linker-fix."
        )
    return candidates[0]


def replace_memory_block(text: str, memory: str, path: Path) -> str:
    pattern = re.compile(r"\bMEMORY\s*\{[^{}]*\}", re.DOTALL)
    if not pattern.search(text):
        raise PrepareError(f"Could not locate a simple MEMORY block in {path}")
    return pattern.sub(memory, text, count=1)


def sections_closing_brace(text: str, path: Path) -> int:
    match = re.search(r"\bSECTIONS\s*\{", text)
    if not match:
        raise PrepareError(f"Could not locate SECTIONS in {path}")

    opening = text.find("{", match.start())
    depth = 0
    for index in range(opening, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    raise PrepareError(f"SECTIONS has no matching closing brace in {path}")


def ensure_external_ram_section(text: str, path: Path) -> str:
    if re.search(r"^\s*\.EXTRAM\b", text, re.MULTILINE):
        return text

    section = """

  /* External RAM: generated by atk_stm32n6_cmake_prepare.py */
  .EXTRAM (NOLOAD) :
  {
    . = ALIGN(8);
    *(.EXTRAM)
    . = ALIGN(8);
  } >EXTRAM
"""
    closing = sections_closing_brace(text, path)
    return text[:closing].rstrip() + section + "\n" + text[closing:]


def ensure_origin_assert(text: str, path: Path, region: str, address: str) -> str:
    marker = f"ATK_{region}_ORIGIN_ASSERT"
    if marker in text:
        return text

    assertion = f"""

  /* {marker} */
  ASSERT(ORIGIN({region}) == {address},
         "ATK: {region} origin must be {address}")
"""
    closing = sections_closing_brace(text, path)
    return text[:closing].rstrip() + assertion + "\n" + text[closing:]


def redirect_undeclared_debug_rom_to_ram(text: str, path: Path) -> str:
    """Map stale >ROM references to RAM in an LRUN linker script.

    Some STM32N6 CubeMX linker templates retain a ROM destination (often for
    .gnu.sgstubs) even though the LRUN MEMORY block contains only RAM and
    EXTRAM.  GNU ld then invents an invalid ROM region and can emit a load
    segment at address zero.
    """
    memory_match = re.search(r"\bMEMORY\s*\{([^{}]*)\}", text, re.DOTALL)
    if not memory_match:
        raise PrepareError(f"Could not inspect MEMORY regions in {path}")

    memory_body = memory_match.group(1)
    declared = {
        match.group(1)
        for match in re.finditer(
            r"^[ \t]*([A-Za-z_][A-Za-z0-9_]*)\s*(?:\([^\n)]*\))?\s*:",
            memory_body,
            re.MULTILINE,
        )
    }
    if "ROM" in declared:
        return text

    updated = re.sub(r">\s*ROM\b", ">RAM", text)
    updated = re.sub(r"\bORIGIN\s*\(\s*ROM\s*\)", "ORIGIN(RAM)", updated)
    updated = re.sub(r"\bLENGTH\s*\(\s*ROM\s*\)", "LENGTH(RAM)", updated)

    if re.search(r">\s*ROM\b", updated):
        raise PrepareError(
            f"Debug linker still refers to undeclared ROM after patching: {path}"
        )
    return updated


def patch_linker_script(
    path: Path,
    memory: str,
    region: str,
    address: str,
    dry_run: bool,
    debug_lrun: bool = False,
) -> bool:
    original = read_text(path)
    updated = replace_memory_block(original, memory, path)
    if debug_lrun:
        updated = redirect_undeclared_debug_rom_to_ram(updated, path)
    updated = ensure_external_ram_section(updated, path)
    updated = ensure_origin_assert(updated, path, region, address)
    return write_if_changed(path, updated, dry_run)


def cmake_quote(path: Path) -> str:
    return path.as_posix().replace('"', '\\"')


def make_cmake_block(cmake_dir: Path, bsp_root: Path, selected: Sequence[Path]) -> str:
    relative_bsp = Path(".") / Path(os.path.relpath(str(bsp_root), str(cmake_dir)))
    relative_bsp_text = relative_bsp.as_posix()
    if relative_bsp_text.startswith("./"):
        relative_bsp_text = relative_bsp_text[2:]

    include_lines = ['    "${ATK_BSP_DIR}"']
    source_lines: List[str] = []
    for component in selected:
        include_lines.append(f'    "${{ATK_BSP_DIR}}/{cmake_quote(Path(component.name))}"')
        for c_file in sorted(component.glob("*.c"), key=lambda p: p.name.casefold()):
            source_lines.append(
                f'    "${{ATK_BSP_DIR}}/{cmake_quote(Path(component.name) / c_file.name)}"'
            )

    if source_lines:
        sources = "\n".join(source_lines)
        sources_section = f"""
target_sources(${{CMAKE_PROJECT_NAME}} PRIVATE
{sources}
)
"""
    else:
        sources_section = "\n# No BSP C sources were detected. Use --add COMPONENT if needed.\n"

    includes = "\n".join(include_lines)
    component_names = " ".join(component.name for component in selected) or "(none)"
    return f"""{BLOCK_BEGIN}
# Generated by atk_stm32n6_cmake_prepare.py; safe to regenerate.
# Detected BSP components: {component_names}
set(ATK_BSP_DIR
    "${{CMAKE_CURRENT_LIST_DIR}}/{relative_bsp_text}"
)

target_include_directories(${{CMAKE_PROJECT_NAME}} PRIVATE
{includes}
)
{sources_section}
# CubeMX passes the linker script through -T but does not always register the
# .ld file as a Ninja dependency.  Without LINK_DEPENDS, editing the memory map
# can leave an old ELF in place while Ninja reports "no work to do".
set_property(TARGET ${{CMAKE_PROJECT_NAME}} APPEND PROPERTY LINK_DEPENDS
    "${{CMAKE_CURRENT_LIST_DIR}}/${{STM32_LINKER_SCRIPT}}"
)
{BLOCK_END}"""


def make_middleware_block(
    cmake_dir: Path,
    mw_root: Path,
    selected: Sequence[Path],
    local_dirs: Dict[str, Path],
) -> str:
    relative_mw = Path(".") / Path(os.path.relpath(str(mw_root), str(cmake_dir)))
    relative_mw_text = relative_mw.as_posix()
    if relative_mw_text.startswith("./"):
        relative_mw_text = relative_mw_text[2:]

    include_lines: List[str] = []
    source_lines: List[str] = []
    for component in selected:
        include_lines.append(
            f'    "${{ATK_MW_DIR}}/{cmake_quote(Path(component.name))}"'
        )
        for c_file in sorted(component.glob("*.c"), key=lambda p: p.name.casefold()):
            source_lines.append(
                f'    "${{ATK_MW_DIR}}/{cmake_quote(Path(component.name) / c_file.name)}"'
            )
        local = local_dirs.get(component.name)
        if local is not None:
            include_lines.append(
                f'    "${{CMAKE_CURRENT_LIST_DIR}}/{cmake_quote(Path(local.name))}"'
            )
            for c_file in sorted(local.glob("*.c"), key=lambda p: p.name.casefold()):
                source_lines.append(
                    f'    "${{CMAKE_CURRENT_LIST_DIR}}/'
                    f'{cmake_quote(Path(local.name) / c_file.name)}"'
                )

    component_names = " ".join(component.name for component in selected)
    local_names = " ".join(local_dirs) or "(none)"
    includes = "\n".join(include_lines)
    sources = "\n".join(source_lines)
    return f"""{MW_BLOCK_BEGIN}
# Generated by atk_stm32n6_cmake_prepare.py; safe to regenerate.
# Detected middleware components: {component_names}
# Project-local middleware config: {local_names}
set(ATK_MW_DIR
    "${{CMAKE_CURRENT_LIST_DIR}}/{relative_mw_text}"
)

target_include_directories(${{CMAKE_PROJECT_NAME}} PRIVATE
{includes}
)

target_sources(${{CMAKE_PROJECT_NAME}} PRIVATE
{sources}
)
{MW_BLOCK_END}"""


def remove_marked_block(text: str, begin: str, end: str) -> str:
    pattern = re.compile(
        r"\n*" + re.escape(begin) + r".*?" + re.escape(end) + r"\n*", re.DOTALL
    )
    if pattern.search(text):
        return pattern.sub("\n", text)
    return text


def replace_marked_block(
    text: str,
    begin: str,
    end: str,
    block: str,
) -> Tuple[str, bool]:
    pattern = re.compile(
        re.escape(begin) + r".*?" + re.escape(end), re.DOTALL
    )
    if pattern.search(text):
        return pattern.sub(block, text), True
    return text, False


def make_linker_block(debug_linker: Path, release_linker: Path) -> str:
    return f"""{LINKER_BEGIN}
# Debug runs from internal SRAM; Release is linked for XSPI2 NOR Flash.
string(TOLOWER "${{CMAKE_BUILD_TYPE}}" ATK_BUILD_TYPE_LOWER)
if(ATK_BUILD_TYPE_LOWER STREQUAL "release")
    set(STM32_LINKER_SCRIPT "{cmake_quote(Path(release_linker.name))}")
else()
    set(STM32_LINKER_SCRIPT "{cmake_quote(Path(debug_linker.name))}")
endif()
{LINKER_END}"""


def install_linker_block(text: str, block: str) -> str:
    updated, found = replace_marked_block(text, LINKER_BEGIN, LINKER_END, block)
    if found:
        return updated

    linker_set = re.compile(
        r"^[ \t]*set\s*\(\s*STM32_LINKER_SCRIPT\b.*?\)[ \t]*$",
        re.MULTILINE | re.DOTALL,
    )
    if linker_set.search(text):
        return linker_set.sub(block, text, count=1)

    project_call = re.search(r"^[ \t]*project\s*\(", text, re.MULTILINE)
    if project_call:
        return text[: project_call.start()] + block + "\n\n" + text[project_call.start() :]
    raise PrepareError(
        "Could not find set(STM32_LINKER_SCRIPT ...) or project(...) in Appli/CMakeLists.txt"
    )


def make_artifact_block() -> str:
    return f"""{ARTIFACT_BEGIN}
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Keep manual-programming files at Appli/build/flash regardless of whether the
# selected preset uses Appli/build/Debug or Appli/build/Release.
set(ATK_FLASH_OUTPUT_DIR "${{CMAKE_CURRENT_LIST_DIR}}/build/flash")
string(TOLOWER "${{CMAKE_BUILD_TYPE}}" ATK_ARTIFACT_BUILD_TYPE_LOWER)

# An ALL custom target is intentional: unlike POST_BUILD, it recreates a
# missing HEX even when the ELF target itself is already up to date.
if(ATK_ARTIFACT_BUILD_TYPE_LOWER STREQUAL "release")
    add_custom_target(atk_release_flash_files ALL
        COMMAND "${{CMAKE_COMMAND}}" -E make_directory
                "${{ATK_FLASH_OUTPUT_DIR}}"
        COMMAND "${{CMAKE_OBJCOPY}}" -O ihex
                "$<TARGET_FILE:${{CMAKE_PROJECT_NAME}}>"
                "${{ATK_FLASH_OUTPUT_DIR}}/${{CMAKE_PROJECT_NAME}}.hex"
        COMMAND "${{CMAKE_OBJCOPY}}" -O binary
                "$<TARGET_FILE:${{CMAKE_PROJECT_NAME}}>"
                "${{ATK_FLASH_OUTPUT_DIR}}/${{CMAKE_PROJECT_NAME}}.bin"
        DEPENDS ${{CMAKE_PROJECT_NAME}}
        BYPRODUCTS
                "${{ATK_FLASH_OUTPUT_DIR}}/${{CMAKE_PROJECT_NAME}}.hex"
                "${{ATK_FLASH_OUTPUT_DIR}}/${{CMAKE_PROJECT_NAME}}.bin"
        COMMENT "Generating Release files in Appli/build/flash (0x70100400)"
        VERBATIM
    )
endif()
{ARTIFACT_END}"""


def update_cmake(
    cmake: Path,
    bsp_block: str,
    middleware_block: Optional[str],
    debug_linker: Optional[Path],
    release_linker: Optional[Path],
    add_artifacts: bool,
    dry_run: bool,
) -> bool:
    updated = read_text(cmake)

    replaced, found = replace_marked_block(updated, BLOCK_BEGIN, BLOCK_END, bsp_block)
    if found:
        updated = replaced
    else:
        updated = updated.rstrip() + "\n\n" + bsp_block + "\n"

    if middleware_block is not None:
        replaced, found = replace_marked_block(
            updated, MW_BLOCK_BEGIN, MW_BLOCK_END, middleware_block
        )
        if found:
            updated = replaced
        else:
            updated = updated.rstrip() + "\n\n" + middleware_block + "\n"
    else:
        # No middleware in use: drop a stale generated block if one exists.
        updated = remove_marked_block(updated, MW_BLOCK_BEGIN, MW_BLOCK_END)

    if debug_linker is not None and release_linker is not None:
        updated = install_linker_block(
            updated, make_linker_block(debug_linker, release_linker)
        )

    if add_artifacts:
        artifact_block = make_artifact_block()
        replaced, found = replace_marked_block(
            updated, ARTIFACT_BEGIN, ARTIFACT_END, artifact_block
        )
        if found:
            updated = replaced
        else:
            updated = updated.rstrip() + "\n\n" + artifact_block + "\n"

    return write_if_changed(cmake, updated, dry_run)


def project_target_name(cmake: Path, project: Path) -> str:
    text = read_text(cmake)

    # CubeMX commonly emits:
    #   set(CMAKE_PROJECT_NAME "08_Basic_Timer_Appli")
    #   project(${CMAKE_PROJECT_NAME})
    # Resolve the set() value first; never copy the ${...} token to launch.json.
    name_set = re.search(
        r"^[ \t]*set\s*\(\s*CMAKE_PROJECT_NAME\s+"
        r"(?:\"([^\"]+)\"|'([^']+)'|([^\s\)]+))",
        text,
        re.MULTILINE,
    )
    if name_set:
        value = next(group for group in name_set.groups() if group is not None)
        if value and "${" not in value and "$<" not in value:
            return value

    project_call = re.search(
        r"^[ \t]*project\s*\(\s*([^\s\)]+)", text, re.MULTILINE
    )
    if project_call:
        value = project_call.group(1).strip('"\'')
        if value and "${" not in value and "$<" not in value:
            return value

    # ALIENTEK examples consistently name the application target after the
    # project directory.  This is safer than emitting an unresolved CMake token.
    return f"{project.name}_Appli"


def load_json_object(path: Path, default: dict) -> dict:
    if not path.exists():
        return dict(default)
    try:
        value = json.loads(read_text(path))
    except json.JSONDecodeError as error:
        raise PrepareError(
            f"{path} is not strict JSON ({error}). Remove JSON comments or use --no-vscode."
        ) from error
    if not isinstance(value, dict):
        raise PrepareError(f"Expected a JSON object in {path}")
    return value


def write_json_if_changed(path: Path, value: dict, dry_run: bool) -> bool:
    content = json.dumps(value, ensure_ascii=False, indent=4) + "\n"
    if path.exists():
        return write_if_changed(path, content, dry_run)
    if not dry_run:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
    return True


def merge_generated_entries(
    existing: object,
    generated: Sequence[dict],
    key: str,
    prefix: str,
) -> List[dict]:
    kept: List[dict] = []
    if isinstance(existing, list):
        for item in existing:
            if not isinstance(item, dict) or not str(item.get(key, "")).startswith(prefix):
                kept.append(item)
    return kept + list(generated)


def vscode_workspace_path(project: Path, path: Path) -> str:
    try:
        relative = path.resolve().relative_to(project.resolve()).as_posix()
    except ValueError:
        return path.resolve().as_posix()
    return "${workspaceFolder}/" + relative


def vscode_settings(project: Path, dry_run: bool) -> bool:
    path = project / ".vscode" / "settings.json"
    data = load_json_object(path, {})
    data.setdefault("cmake.useCMakePresets", "always")
    # Build the real application directly.  The generated outer ExternalProject
    # shares Appli/build between configurations and can retain a stale stamp.
    data["cmake.sourceDirectory"] = "${workspaceFolder}/Appli"
    data.setdefault("cmake.cmakePath", "cube-cmake")
    data.setdefault("cmake.configureArgs", ["-DCMAKE_COMMAND=cube-cmake"])
    data.setdefault("cmake.preferredGenerators", ["Ninja"])
    data.setdefault("stm32cube-ide-clangd.path", "cube")

    arguments = data.get("stm32cube-ide-clangd.arguments")
    if arguments is None:
        arguments = ["starm-clangd"]
    if not isinstance(arguments, list):
        raise PrepareError(
            f"stm32cube-ide-clangd.arguments in {path} must be a JSON array"
        )
    arguments = [
        argument
        for argument in arguments
        if not (
            isinstance(argument, str)
            and argument.startswith("--compile-commands-dir=")
        )
    ]
    debug_binary_dir = configure_binary_dir(project / "Appli", "Debug")
    for argument in (
        "--background-index",
        "--header-insertion=never",
        "--compile-commands-dir="
        + vscode_workspace_path(project, debug_binary_dir),
    ):
        if argument not in arguments:
            arguments.append(argument)
    data["stm32cube-ide-clangd.arguments"] = arguments
    return write_json_if_changed(path, data, dry_run)


def vscode_script_reference(project: Path) -> str:
    script = Path(__file__).resolve()
    try:
        relative = Path(os.path.relpath(str(script), str(project))).as_posix()
    except ValueError:
        return script.as_posix()
    return "${workspaceFolder}/" + relative


def vscode_tasks(project: Path, dry_run: bool) -> bool:
    path = project / ".vscode" / "tasks.json"
    data = load_json_object(path, {"version": "2.0.0", "tasks": []})
    data.setdefault("version", "2.0.0")
    script = vscode_script_reference(project)
    generated = [
        {
            "label": "STM32N6: Build Debug",
            "type": "process",
            "command": "python3",
            "args": [script, "${workspaceFolder}", "--build", "Debug"],
            "problemMatcher": ["$gcc"],
            "group": {"kind": "build", "isDefault": True},
        },
        {
            "label": "STM32N6: Build Release",
            "type": "process",
            "command": "python3",
            "args": [script, "${workspaceFolder}", "--build", "Release"],
            "problemMatcher": ["$gcc"],
            "group": "build",
        },
    ]
    data["tasks"] = merge_generated_entries(
        data.get("tasks"), generated, "label", "STM32N6:"
    )
    return write_json_if_changed(path, data, dry_run)


def vscode_launch(
    project: Path,
    target: str,
    dry_run: bool,
) -> bool:
    if "${" in target or "$<" in target:
        raise PrepareError(
            f"Refusing to write unresolved CMake target '{target}' to launch.json"
        )
    path = project / ".vscode" / "launch.json"
    data = load_json_object(path, {"version": "0.2.0", "configurations": []})
    data.setdefault("version", "0.2.0")
    debug_binary_dir = configure_binary_dir(project / "Appli", "Debug")
    debug_elf = vscode_workspace_path(
        project, debug_binary_dir / f"{target}.elf"
    )
    common = {
        "type": "stlinkgdbtarget",
        "request": "launch",
        "origin": "snippet",
        "cwd": "${workspaceFolder}",
        "deviceName": DEVICE_NAME,
        "deviceCore": DEVICE_CORE,
        "serverInterface": "SWD",
        "runEntry": "main",
    }
    debug = dict(common)
    debug.update(
        {
            "name": "STM32N6: Debug SRAM (F5 / Ctrl+F5)",
            "preLaunchTask": "STM32N6: Build Debug",
            "imagesAndSymbols": [{"imageFileName": debug_elf}],
        }
    )
    # Release programming deliberately is a VS Code task, not a GDB launch.
    # After download the application must be started by the board's FSBL in
    # Flash boot; asking GDB to continue directly can make the target vanish.
    generated = [debug]

    data["configurations"] = merge_generated_entries(
        data.get("configurations"), generated, "name", "STM32N6"
    )
    return write_json_if_changed(path, data, dry_run)


def prepare_vscode(
    project: Path,
    target: str,
    dry_run: bool,
) -> List[Path]:
    changed: List[Path] = []
    operations = (
        (project / ".vscode" / "settings.json", vscode_settings(project, dry_run)),
        (project / ".vscode" / "tasks.json", vscode_tasks(project, dry_run)),
        (
            project / ".vscode" / "launch.json",
            vscode_launch(project, target, dry_run),
        ),
    )
    changed.extend(path for path, did_change in operations if did_change)
    return changed


def find_executable(name: str, candidates: Sequence[Path]) -> Path:
    found = shutil.which(name)
    if found:
        return Path(found).resolve()
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    raise PrepareError(
        f"Could not find {name}. Open VS Code from the STM32 workspace or add it to PATH."
    )


def find_cube_cmake() -> Path:
    home = Path.home()
    candidates = sorted(
        (home / ".vscode" / "extensions").glob(
            "stmicroelectronics.stm32cube-ide-build-cmake-*/resources/"
            "cube-cmake/darwin/*/cube-cmake"
        ),
        reverse=True,
    )
    return find_executable("cube-cmake", candidates)


def run_checked(command: Sequence[str], cwd: Path) -> None:
    print("+ " + " ".join(command))
    try:
        subprocess.run(list(command), cwd=cwd, check=True)
    except subprocess.CalledProcessError as error:
        raise PrepareError(
            f"Command failed with exit code {error.returncode}: {' '.join(command)}"
        ) from error


def cmake_presets(appli: Path) -> dict:
    preset_file = appli / "CMakePresets.json"
    if not preset_file.is_file():
        raise PrepareError(
            f"Missing {preset_file}. Regenerate the CMake project with CubeMX."
        )
    return load_json_object(preset_file, {})


def configure_preset(appli: Path, configuration: str) -> Tuple[str, dict, dict]:
    preset_file = appli / "CMakePresets.json"
    data = cmake_presets(appli)
    presets = {
        item["name"]: item
        for item in data.get("configurePresets", [])
        if isinstance(item, dict) and isinstance(item.get("name"), str)
    }
    names = [
        name for name in presets
    ]
    for name in names:
        if name.casefold() == configuration.casefold():
            return name, presets[name], presets
    raise PrepareError(
        f"No {configuration} configure preset in {preset_file}; found: {', '.join(names)}"
    )


def inherited_preset_value(
    name: str,
    key: str,
    presets: Dict[str, dict],
    visited: Optional[Set[str]] = None,
) -> Optional[object]:
    visited = set() if visited is None else set(visited)
    if name in visited:
        raise PrepareError(f"CMake preset inheritance cycle involving '{name}'")
    visited.add(name)
    preset = presets.get(name)
    if preset is None:
        raise PrepareError(f"CMake preset '{name}' inherits from a missing preset")
    if key in preset:
        return preset[key]
    parents = preset.get("inherits", [])
    if isinstance(parents, str):
        parents = [parents]
    if not isinstance(parents, list):
        raise PrepareError(f"Invalid inherits value in CMake preset '{name}'")
    for parent in parents:
        if isinstance(parent, str):
            value = inherited_preset_value(parent, key, presets, visited)
            if value is not None:
                return value
    return None


def configure_binary_dir(appli: Path, configuration: str) -> Path:
    preset_name, _preset, presets = configure_preset(appli, configuration)
    raw = inherited_preset_value(preset_name, "binaryDir", presets)
    if not isinstance(raw, str) or not raw:
        raise PrepareError(
            f"Configure preset '{preset_name}' does not define a binaryDir"
        )
    expanded = raw
    replacements = {
        "${sourceDir}": appli.as_posix(),
        "${sourceParentDir}": appli.parent.as_posix(),
        "${sourceDirName}": appli.name,
        "${presetName}": preset_name,
    }
    for macro, value in replacements.items():
        expanded = expanded.replace(macro, value)
    if "${" in expanded or "$env{" in expanded or "$penv{" in expanded:
        raise PrepareError(
            f"Unsupported macro in binaryDir for preset '{preset_name}': {raw}"
        )
    path = Path(expanded).expanduser()
    if not path.is_absolute():
        path = appli / path
    return path.resolve()


def artifact_path(
    project: Path,
    target: str,
    configuration: str,
    suffix: str,
    binary_dir: Optional[Path] = None,
) -> Path:
    if configuration == "Release" and suffix in ("hex", "bin"):
        return project / "Appli" / "build" / "flash" / f"{target}.{suffix}"
    if binary_dir is None:
        binary_dir = configure_binary_dir(project / "Appli", configuration)
    return binary_dir / f"{target}.{suffix}"


def elf_min_load_address(path: Path) -> int:
    data = path.read_bytes()
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise PrepareError(f"Not an ELF file: {path}")
    if data[4] != 1 or data[5] != 1:
        raise PrepareError(f"Expected a 32-bit little-endian ELF: {path}")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data, 0)
    program_offset = header[5]
    section_offset = header[6]
    program_entry_size = header[9]
    program_entry_count = header[10]
    section_entry_size = header[11]
    section_entry_count = header[12]

    load_segments: List[Tuple[int, int, int]] = []
    for index in range(program_entry_count):
        offset = program_offset + index * program_entry_size
        if offset + 32 > len(data):
            raise PrepareError(f"Truncated ELF program table: {path}")
        segment = struct.unpack_from("<IIIIIIII", data, offset)
        segment_type = segment[0]
        file_offset = segment[1]
        physical_address = segment[3]
        file_size = segment[4]
        if segment_type == 1 and file_size:
            load_segments.append((file_offset, file_size, physical_address))

    # PT_LOAD addresses may be rounded down for alignment and include ELF
    # headers.  Validate the first real SHF_ALLOC, file-backed section instead.
    # Its LMA is derived from its offset inside the containing PT_LOAD segment.
    addresses: List[int] = []
    for index in range(section_entry_count):
        offset = section_offset + index * section_entry_size
        if offset + 40 > len(data):
            raise PrepareError(f"Truncated ELF section table: {path}")
        section = struct.unpack_from("<IIIIIIIIII", data, offset)
        section_type = section[1]
        section_flags = section[2]
        file_offset = section[4]
        section_size = section[5]
        if not (section_flags & 0x2) or section_type == 8 or not section_size:
            continue
        for segment_offset, segment_size, physical_address in load_segments:
            if (
                file_offset >= segment_offset
                and file_offset + section_size <= segment_offset + segment_size
            ):
                addresses.append(physical_address + file_offset - segment_offset)
                break
    if not addresses:
        raise PrepareError(f"ELF contains no loadable allocated section: {path}")
    return min(addresses)


def hex_min_data_address(path: Path) -> int:
    base = 0
    addresses: List[int] = []
    for line_number, raw in enumerate(read_text(path).splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise PrepareError(f"Invalid Intel HEX line {line_number}: {path}")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise PrepareError(f"Invalid Intel HEX line {line_number}: {path}") from error
        if len(record) < 5 or record[0] + 5 != len(record):
            raise PrepareError(f"Bad Intel HEX length at line {line_number}: {path}")
        offset = (record[1] << 8) | record[2]
        record_type = record[3]
        payload = record[4:-1]
        if record_type == 0 and payload:
            addresses.append(base + offset)
        elif record_type == 2 and len(payload) == 2:
            base = int.from_bytes(payload, "big") << 4
        elif record_type == 4 and len(payload) == 2:
            base = int.from_bytes(payload, "big") << 16
    if not addresses:
        raise PrepareError(f"Intel HEX contains no data records: {path}")
    return min(addresses)


def verify_artifact(
    project: Path,
    target: str,
    configuration: str,
    binary_dir: Optional[Path] = None,
) -> Path:
    expected = DEBUG_ORIGIN if configuration == "Debug" else RELEASE_ORIGIN
    elf = artifact_path(
        project, target, configuration, "elf", binary_dir=binary_dir
    )
    if not elf.is_file():
        raise PrepareError(f"Expected artifact was not generated: {elf}")
    actual = elf_min_load_address(elf)
    if actual != expected:
        raise PrepareError(
            f"Refusing to use {elf}: lowest ELF load address is 0x{actual:08X}, "
            f"expected 0x{expected:08X}. Re-run preparation and rebuild {configuration}."
        )
    print(f"Verified {configuration} ELF address: 0x{actual:08X}")

    if configuration == "Release":
        hex_file = artifact_path(project, target, configuration, "hex")
        if not hex_file.is_file():
            raise PrepareError(f"Expected Release HEX was not generated: {hex_file}")
        hex_address = hex_min_data_address(hex_file)
        if hex_address != RELEASE_ORIGIN:
            raise PrepareError(
                f"Refusing to flash {hex_file}: first HEX data address is "
                f"0x{hex_address:08X}, expected 0x{RELEASE_ORIGIN:08X}."
            )
        print(f"Verified Release HEX address: 0x{hex_address:08X}")
    return elf


def build_configuration(project: Path, target: str, configuration: str) -> Path:
    appli = project / "Appli"
    cmake = find_cube_cmake()
    preset, _preset_data, _presets = configure_preset(appli, configuration)
    binary_dir = configure_binary_dir(appli, configuration)
    run_checked([str(cmake), "--preset", preset], appli)
    run_checked([str(cmake), "--build", str(binary_dir)], appli)
    try:
        return verify_artifact(
            project, target, configuration, binary_dir=binary_dir
        )
    except PrepareError as first_error:
        message = str(first_error)
        if "lowest ELF load address" not in message:
            raise
        print(
            "ELF address is stale after linker-script changes; "
            "performing one clean rebuild."
        )
        run_checked(
            [str(cmake), "--build", str(binary_dir), "--clean-first"],
            appli,
        )
        return verify_artifact(
            project, target, configuration, binary_dir=binary_dir
        )


def rel(path: Path, project: Path) -> str:
    try:
        return path.relative_to(project).as_posix()
    except ValueError:
        return path.as_posix()


def main(argv: Sequence[str]) -> int:
    args = parse_args(argv)
    project = Path(args.project).expanduser().resolve()

    print(f"ATK STM32N6 CMake prepare v{SCRIPT_VERSION}")
    print(f"Running script: {Path(__file__).resolve()}")
    print(f"Preparing project: {project}")

    if args.dry_run and args.build:
        print("Error: --dry-run cannot be combined with --build.", file=sys.stderr)
        return 2

    try:
        appli = validate_project(project)
        bsp_root = find_bsp_root(project)
        components = component_map(bsp_root)

        secure_changes: List[Path] = []
        if not args.no_secure_fix:
            secure_changes = patch_secure_sources(project, appli, args.dry_run)

        selected_mw: List[Path] = []
        local_mw: Dict[str, Path] = {}
        mw_root = find_middlewares_root(project)
        if mw_root is not None:
            mw_components = flat_middleware_map(mw_root)
            selected_mw = resolve_middlewares(appli, mw_components)
            local_mw = local_middleware_dirs(appli, selected_mw)

        # Middleware sources may include BSP headers (for example the USMART
        # port uses the UART/SYS BSP), so feed them into BSP dependency
        # resolution as well.
        mw_files: List[Path] = []
        for component in selected_mw:
            mw_files.extend(source_files(component))
        for local in local_mw.values():
            mw_files.extend(source_files(local))

        selected = resolve_components(appli, components, args.add, mw_files)
        cmake = appli / "CMakeLists.txt"
        block = make_cmake_block(cmake.parent, bsp_root, selected)
        mw_block: Optional[str] = None
        if mw_root is not None and selected_mw:
            mw_block = make_middleware_block(
                cmake.parent, mw_root, selected_mw, local_mw
            )

        debug_linker: Optional[Path] = None
        release_linker: Optional[Path] = None
        debug_linker_changed = False
        release_linker_changed = False
        if not args.no_linker_fix:
            debug_linker = choose_linker_script(appli, "*LRUN_RAMxspi1.ld")
            release_linker = choose_linker_script(appli, "*ROMxspi2_RAMxspi1.ld")
            debug_linker_changed = patch_linker_script(
                debug_linker,
                DEBUG_MEMORY,
                "RAM",
                "0x34000400",
                args.dry_run,
                debug_lrun=True,
            )
            release_linker_changed = patch_linker_script(
                release_linker,
                RELEASE_MEMORY,
                "ROM",
                "0x70100400",
                args.dry_run,
            )

        cmake_changed = update_cmake(
            cmake,
            block,
            mw_block,
            debug_linker,
            release_linker,
            add_artifacts=not args.no_artifacts,
            dry_run=args.dry_run,
        )

        target = project_target_name(cmake, project)
        vscode_changes: List[Path] = []
        if not args.no_vscode:
            vscode_changes = prepare_vscode(
                project, target, args.dry_run
            )
    except PrepareError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 2

    prefix = "Would update" if args.dry_run else "Updated"
    if secure_changes:
        for path in secure_changes:
            print(f"{prefix} secure callback token: {rel(path, project)}")
    else:
        print("Secure callback patch: not needed")

    names = ", ".join(component.name for component in selected) or "none"
    print(f"Detected BSP components: {names}")
    mw_names = ", ".join(component.name for component in selected_mw) or "none"
    print(f"Detected middleware components: {mw_names}")
    if local_mw:
        print(f"Project-local middleware config: {', '.join(local_mw)}")
    if debug_linker is not None:
        status = prefix if debug_linker_changed else "Already configured"
        print(f"{status} Debug linker: {rel(debug_linker, project)}")
    if release_linker is not None:
        status = prefix if release_linker_changed else "Already configured"
        print(f"{status} Release linker: {rel(release_linker, project)}")
    if cmake_changed:
        print(f"{prefix}: {rel(cmake, project)}")
    else:
        print("CMake automation blocks: already up to date")

    if not args.no_vscode:
        if vscode_changes:
            for path in vscode_changes:
                print(f"{prefix}: {rel(path, project)}")
        else:
            print("VS Code workflow files: already up to date")
        print("VS Code CMake source: ${workspaceFolder}/Appli")
        print("VS Code Debug pre-launch task: STM32N6: Build Debug")

    if not args.dry_run:
        try:
            if args.build == "all":
                build_configuration(project, target, "Debug")
                build_configuration(project, target, "Release")
            elif args.build:
                build_configuration(project, target, args.build)
        except PrepareError as error:
            print(f"Error: {error}", file=sys.stderr)
            return 2

        if not args.build:
            print("Next: run the VS Code task 'STM32N6: Build Debug' or press F5.")
            print(
                "For Release, select the Release preset and build, or run "
                "the task 'STM32N6: Build Release'."
            )
            print("Release HEX/BIN output: Appli/build/flash/.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
