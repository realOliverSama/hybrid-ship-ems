#!/usr/bin/env bash
# =============================================================================
# build_plantstep_e.sh — S4-G2：编译 libplantstepE_<scenario>.dylib
# （plantstep_e_shim.c + 23E 生成的 GRT 代码；Model D build_plantstep.sh 同法）
#
# 用法:
#   tools/build_plantstep_e.sh <scenario>     # scenario = 23E 的场景目录名
# 产物:
#   Host/plantstep_e/<scenario>/libplantstepE_<scenario>.dylib
# 依赖: 生成目录存在（23E 产物，build_manifest.json 为前置）；系统 clang。
# 注意: 本脚本不写 manifest（manifest 由 23E 全责；dylib sha256 打印供核对）。
# =============================================================================
set -euo pipefail

SCEN="${1:?usage: build_plantstep_e.sh <scenario>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
SCENDIR="$HERE/../plantstep_e/$SCEN"
RTW="$(ls -d "$SCENDIR"/HybridShip_ModelE_PlantStepE_*_rtw 2>/dev/null | head -1)"
[ -n "$RTW" ] || { echo "GATE:BUILD_PS_E:no_rtw — $SCENDIR 下无 *_rtw（先跑 23E）" >&2; exit 2; }
[ -f "$SCENDIR/build_manifest.json" ] || { echo "GATE:BUILD_PS_E:no_manifest" >&2; exit 2; }

OUT="$SCENDIR/libplantstepE_${SCEN}.dylib"
# shim 场景校验宏必须与烘焙场景一致
SED_SCEN=$(printf '%s' "$SCEN" | sed 's/["\\]/\\&/g')

# MATLAB 外部头（仓库外依赖，§20.2 环境变量注入；GRT 模板 rtwtypes.h 需要
# tmwtypes.h 等；rt_logging 源随 GRT 模板需要一并编译——Model D build 同法）
MATLAB_ROOT="${EMS_MATLAB_ROOT:-}"
if [ -n "$MATLAB_ROOT" ] && [ -d "$MATLAB_ROOT" ]; then
  INC_EXTRA="-I$MATLAB_ROOT/extern/include -I$MATLAB_ROOT/simulink/include -I$MATLAB_ROOT/rtw/c/src"
  RT_LOGGING_SRC="$MATLAB_ROOT/rtw/c/src/rt_logging.c"
else
  echo "ERROR: 未设置 EMS_MATLAB_ROOT（MATLAB 安装根，GRT 基建头/logging 源需要）。" >&2
  echo "       示例: export EMS_MATLAB_ROOT=/Applications/MATLAB_R2026a.app" >&2
  exit 2
fi

echo "[build_ps_e] rtw=$RTW"
# MAT_FILE=0：生成码零 logging 调用（grep 佐证），rt_logging 编译为 no-op 分支，
# 并绕开 builtin_typeid_types.h（R2026a U4 安装树缺失该头）。
clang -O2 -dynamiclib -fPIC \
  -DMAT_FILE=0 \
  -DPS_BAKED_SCENARIO="\"$SED_SCEN\"" \
  -I"$RTW" $INC_EXTRA \
  "$HERE/plantstep_e_shim.c" \
  "$RTW"/HybridShip_ModelE_PlantStepE.c \
  "$RTW"/HybridShip_ModelE_PlantStepE_data.c \
  "$RTW"/rtGetInf.c \
  "$RTW"/rtGetNaN.c \
  "$RTW"/rt_nonfinite.c \
  "$RT_LOGGING_SRC" \
  -o "$OUT"

echo "[build_ps_e] dylib sha256:"
shasum -a 256 "$OUT"
echo "[build_ps_e] OK -> $OUT"
