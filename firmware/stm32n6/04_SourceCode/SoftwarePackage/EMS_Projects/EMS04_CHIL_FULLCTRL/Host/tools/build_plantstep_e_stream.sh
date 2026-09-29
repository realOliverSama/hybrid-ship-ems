#!/usr/bin/env bash
# =============================================================================
# build_plantstep_e_stream.sh — S4-P3 B2：编译 libplantstepE_stream.dylib
# （plantstep_e_stream_shim.c + 23S 生成的 GRT 代码；build_plantstep_e.sh 同法）
#
# 单份生成码 4 航次共用（stream bin 运行期由 shim 经 dladdr 同目录读取）；
# 每航次目录一份 dylib 拷贝（同一二进制，sha256 一致）。
#
# 用法:
#   tools/build_plantstep_e_stream.sh <voyage_id>   # = SCENARIOS_E_STREAM 成员
#   tools/build_plantstep_e_stream.sh all           # 冻结 4 航次逐一构建
# 前置:
#   Host/plantstep_e_stream/codegen/HybridShip_ModelE_PlantStepE_Stream_grt_rtw/
#     （23S 产物，build_manifest.json 为前置）
#   Host/plantstep_e_stream/<vid>/<vid>.stream.f64.bin
#     （tools/s4_p3_pack_stream.py 产物）
# 产物:
#   Host/plantstep_e_stream/<vid>/libplantstepE_stream.dylib
# 依赖: 系统 clang；EMS_MATLAB_ROOT（GRT 基建头/logging 源，§20.2 环境变量注入）。
# 注意: 本脚本不写 manifest（manifest 由 23S 全责；dylib sha256 打印供核对）。
# =============================================================================
set -euo pipefail

VID="${1:?usage: build_plantstep_e_stream.sh <voyage_id>|all}"
HERE="$(cd "$(dirname "$0")" && pwd)"
STREAM_ROOT="$HERE/../plantstep_e_stream"
CODEGEN="$STREAM_ROOT/codegen"
RTW="$(ls -d "$CODEGEN"/HybridShip_ModelE_PlantStepE_Stream_*_rtw 2>/dev/null | head -1)"
[ -n "$RTW" ] || { echo "GATE:BUILD_PS_STREAM:no_rtw — $CODEGEN 下无 *_rtw（先跑 23S）" >&2; exit 2; }
[ -f "$CODEGEN/build_manifest.json" ] || { echo "GATE:BUILD_PS_STREAM:no_manifest" >&2; exit 2; }

FROZEN_VIDS="FLTA_S2_D01006_M1 FLTA_S2_D01006_M2 FLTA_S6_D05003_M1 FLTA_S6_D05003_M2"
if [ "$VID" = "all" ]; then
  # shellcheck disable=SC2086
  for v in $FROZEN_VIDS; do
    "$0" "$v"
  done
  exit 0
fi

SCENDIR="$STREAM_ROOT/$VID"
[ -d "$SCENDIR" ] || { echo "GATE:BUILD_PS_STREAM:no_voyage_dir — $SCENDIR" >&2; exit 2; }
[ -f "$SCENDIR/$VID.stream.f64.bin" ] || {
  echo "GATE:BUILD_PS_STREAM:no_stream_bin — $SCENDIR/$VID.stream.f64.bin（先跑 tools/s4_p3_pack_stream.py）" >&2; exit 2; }

OUT="$SCENDIR/libplantstepE_stream.dylib"

# MATLAB 外部头（仓库外依赖，§20.2 环境变量注入；rt_logging 源随 GRT 模板一并
# 编译——build_plantstep_e.sh 同法；MAT_FILE=0 使 logging 编译为 no-op 分支并
# 绕开 builtin_typeid_types.h）
MATLAB_ROOT="${EMS_MATLAB_ROOT:-}"
if [ -n "$MATLAB_ROOT" ] && [ -d "$MATLAB_ROOT" ]; then
  INC_EXTRA="-I$MATLAB_ROOT/extern/include -I$MATLAB_ROOT/simulink/include -I$MATLAB_ROOT/rtw/c/src"
  RT_LOGGING_SRC="$MATLAB_ROOT/rtw/c/src/rt_logging.c"
else
  echo "ERROR: 未设置 EMS_MATLAB_ROOT（MATLAB 安装根）。示例: export EMS_MATLAB_ROOT=/Applications/MATLAB_R2026a.app" >&2
  exit 2
fi

echo "[build_ps_stream] vid=$VID rtw=$RTW"
# -install_name 固定：默认 clang 会把 -o 绝对路径嵌入 LC_ID_DYLIB，导致同源
# 4 份 dylib 字节互异；固定后 4 航次 dylib 逐字节一致（ctypes dlopen 不依赖
# install name，仅影响链接期依赖解析——本 dylib 无链接期依赖方）。
clang -O2 -dynamiclib -fPIC \
  -DMAT_FILE=0 \
  -Wl,-install_name,libplantstepE_stream.dylib \
  -I"$RTW" $INC_EXTRA \
  "$HERE/plantstep_e_stream_shim.c" \
  "$RTW"/HybridShip_ModelE_PlantStepE_Stream.c \
  "$RTW"/HybridShip_ModelE_PlantStepE_Stream_data.c \
  "$RTW"/rtGetInf.c \
  "$RTW"/rtGetNaN.c \
  "$RTW"/rt_nonfinite.c \
  "$RT_LOGGING_SRC" \
  -o "$OUT"

echo "[build_ps_stream] dylib sha256:"
shasum -a 256 "$OUT"
echo "[build_ps_stream] OK -> $OUT"
