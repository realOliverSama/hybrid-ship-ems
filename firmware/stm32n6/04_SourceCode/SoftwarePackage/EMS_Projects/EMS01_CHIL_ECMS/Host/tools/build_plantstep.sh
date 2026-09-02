#!/usr/bin/env bash
# ==========================================================================
# build_plantstep.sh — T-604（spec: ems01-chil-v3-closedloop, FR-602 / AC-602）
#
# 用途: 把 Simulink Coder（ERT，GenCodeOnly）为 HybridShip_ModelD_PlantStep
#       生成的 C 代码（scripts/23_codegen_plantstep.m 产物）与 tools/
#       plantstep_shim.c 一起编译为 macOS 共享库，供 plantstep_advancer.py
#       经 ctypes 加载（design §4.1）。
#
# 用法:  ./build_plantstep.sh <S20|voyage002>
#   输入  Host/plantstep/<scenario>/HybridShip_ModelD_PlantStep_{ert|grt}_rtw/*.c|.h
#   输出  Host/plantstep/<scenario>/libplantstep_<scenario>.dylib
#   shim  Host/tools/plantstep_shim.c（-DPS_BAKED_SCENARIO="<scenario>" 烘焙
#         场景名，shim init 据此拒绝错配 dylib）
#
# manifest 分工（与 scripts/23_codegen_plantstep.m 头注释互为镜像）:
#   build_manifest.json 由 23 号 MATLAB 脚本全责生成（生成时间/MATLAB 版本/
#   模型与参数文件 sha256/场景变量指纹/生成产物逐文件 sha256/ert-grt 标志）。
#   本脚本【不写 manifest】——仅编译，并打印 dylib 的 sha256 供人工核对；
#   dylib 可由 manifest 记录的输入复现。
#
# 纪律: set -euo pipefail（编译错误非零退出）；路径全部相对本脚本位置推导，
#       禁绝对路径（AGENTS §20.2）；只编译不运行（Build 与验证独立阶段）。
# ==========================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # .../Host/tools
HOST_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"                   # .../Host

SCN="${1:-}"
case "${SCN}" in
  S20|voyage002) ;;
  *) echo "ERROR: usage: $0 <S20|voyage002>" >&2; exit 2 ;;
esac

MDL="HybridShip_ModelD_PlantStep"
# 产物目录跟随实际生成目标（ert 主案 / grt 回退——23 号 manifest 记录 target）
RTW_DIR="${HOST_DIR}/plantstep/${SCN}/${MDL}_ert_rtw"
if [ ! -d "${RTW_DIR}" ]; then
  RTW_DIR="${HOST_DIR}/plantstep/${SCN}/${MDL}_grt_rtw"
fi
OUT_LIB="${HOST_DIR}/plantstep/${SCN}/libplantstep_${SCN}.dylib"
SHIM="${SCRIPT_DIR}/plantstep_shim.c"
CC="${CC:-gcc}"
# MATLAB 外部头/源（仓库外依赖，§20.2 环境变量注入；GRT 模板含 logging 挂钩，
# 需链接 MATLAB rtw/c/src/rt_logging.c）。tmwtypes.h 已由 23 号复制自包含，
# 其余 GRT 基建头经 -I 提供。缺失时优雅降级：报错并给出注入示例。
MATLAB_ROOT="${EMS_MATLAB_ROOT:-}"
INC_EXTRA=""
RT_LOGGING_SRC=""
if [ -n "${MATLAB_ROOT}" ] && [ -d "${MATLAB_ROOT}" ]; then
  INC_EXTRA="-I${MATLAB_ROOT}/extern/include -I${MATLAB_ROOT}/simulink/include -I${MATLAB_ROOT}/rtw/c/src"
  RT_LOGGING_SRC="${MATLAB_ROOT}/rtw/c/src/rt_logging.c"
  [ -f "${RT_LOGGING_SRC}" ] || { echo "ERROR: rt_logging.c 缺失: ${RT_LOGGING_SRC}" >&2; exit 2; }
else
  echo "ERROR: 未设置 EMS_MATLAB_ROOT（MATLAB 安装根，GRT 基建头/logging 源需要）。" >&2
  echo "       示例: export EMS_MATLAB_ROOT=\"\$(matlabroot)\" 或 /Applications/MATLAB_R2026a.app" >&2
  exit 2
fi

if [ ! -d "${RTW_DIR}" ]; then
  echo "ERROR: codegen 目录不存在: ${RTW_DIR}" >&2
  echo "       先在模型工作区执行 scripts/23_codegen_plantstep.m（T-604）" >&2
  exit 2
fi
if [ ! -f "${SHIM}" ]; then
  echo "ERROR: shim 缺失: ${SHIM}" >&2
  exit 2
fi

# 收集生成源文件（ERT 可能有多个 .c：model.c + roll 拆分）
SRCS=()
for f in "${RTW_DIR}"/*.c; do
  [ -f "${f}" ] && SRCS+=("${f}")
done
if [ "${#SRCS[@]}" -eq 0 ]; then
  echo "ERROR: ${RTW_DIR} 下无 .c 生成产物" >&2
  exit 2
fi
echo "[build] scenario=${SCN} | src=${#SRCS[@]} 个 .c | shim=plantstep_shim.c"

# FD4: gcc -shared -fPIC（macOS clang 别名等价）；-O2；-I 指向生成目录取头文件。
# 若链接报数学函数未定义（模型含 exp/pow 等），在下方追加 -lm。
"${CC}" -O2 -fPIC -shared \
  -I"${RTW_DIR}" \
  ${INC_EXTRA} \
  -DPS_BAKED_SCENARIO="\"${SCN}\"" \
  -o "${OUT_LIB}" \
  "${SRCS[@]}" "${RT_LOGGING_SRC}" "${SHIM}"

echo "[build] dylib -> ${OUT_LIB}"
# 供核对的指纹（不写 manifest——分工见头注释）
if command -v shasum >/dev/null 2>&1; then
  shasum -a 256 "${OUT_LIB}"
fi
echo "[build] OK: libplantstep_${SCN}.dylib"