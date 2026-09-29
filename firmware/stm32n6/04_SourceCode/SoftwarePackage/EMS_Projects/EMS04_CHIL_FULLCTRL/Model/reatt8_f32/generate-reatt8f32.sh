#!/bin/bash
# EMS04 — REAT-T fp32 全 CPU 编译形态部署包再生成脚本（偏差④修复，2026-09-29）。
#
# 用法:  cd <本目录> && STEDGEAI_CLI=<stedgeai 4.0.1 CLI> ./generate-reatt8f32.sh
# 前置:  reatt_raw_fp32.tflite 只读副本（源 = ../reatt8_q/reatt_raw_fp32.tflite，
#        sha256 见 NOTE.md；经 host 对拍 == keras fp32 逐位级一致，锚 M2@300
#        q50 mean30 = 163.958 kW，见 S4_PRED_PHYSICS_RCA.md §2）。
# 产物:  reatt8.c / reatt8.h / reatt8_data.c / reatt8_data.h / reatt8_details.h
#        （覆盖本目录，--name reatt8：与 reatt8_q 同符号命名空间 stai_reatt8_*，
#        二者互斥编入，由 EMS_REATT_FORM=CPU/CPUF32 选择）；generate_401.log。
# 形态:  全 CPU 编译形态（无 --st-neural-art）——权重 4,052,912 B 编入 ELF
#        （Release NOR .rodata），激活 375,432 B 由固件静态数组提供。
set -euo pipefail

STEDGEAI="${STEDGEAI_CLI:-}"
if [ -z "$STEDGEAI" ] || [ ! -x "$STEDGEAI" ]; then
  echo "ERROR: STEDGEAI_CLI 未设置或不可执行（需 STEdgeAI 4.0.1 CLI）。" >&2
  exit 1
fi

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

{
  echo "# stedgeai version: $("$STEDGEAI" --version)"
  echo "# model: reatt_raw_fp32.tflite  form: all-CPU fp32 (no --st-neural-art)"
} > generate_401.log

"$STEDGEAI" generate --no-inputs-allocation --no-outputs-allocation \
  --model reatt_raw_fp32.tflite --target stm32n6 --name reatt8 2>&1 | tee -a generate_401.log

mv st_ai_output/reatt8.c st_ai_output/reatt8.h st_ai_output/reatt8_data.c \
   st_ai_output/reatt8_data.h st_ai_output/reatt8_details.h .
echo "OK: reatt8.c/.h + reatt8_data.c/.h + reatt8_details.h + generate_401.log"
