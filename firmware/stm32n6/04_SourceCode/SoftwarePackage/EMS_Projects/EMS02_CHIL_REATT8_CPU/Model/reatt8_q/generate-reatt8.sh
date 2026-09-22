#!/bin/bash
# EMS01_CHIL_REATT8 — REAT-T int8 全 CPU 编译形态部署包再生成脚本
#（phase4-g，spec .specs/phase4-g-reatt-board-deployment，D3）。
#
# 用法:  cd <本目录> && STEDGEAI_CLI=<stedgeai 4.0.1 CLI> ./generate-reatt8.sh
# 前置:  环境变量 STEDGEAI_CLI 指向 STEdgeAI 4.0.1 CLI（版本锁定 4.0.1，
#        与 ../tcn_q_v2_cpu/ 同代：NetworkRuntime1201 / atonn 1.1.3-275）；
#        reatt_raw_int8.tflite 已由 Host/tools/quantize_reatt.py 生成（只读副本，
#        sha256 见 NOTE.md，禁手改）。
# 产物:  reatt8.c / reatt8.h / reatt8_data.c / reatt8_data.h / reatt8_details.h
#        （覆盖本目录，--name reatt8）；generate_401.log（全量报告）。
# 形态:  全 CPU 编译形态（无 --st-neural-art）——权重 1,432,556 B 编入 ELF
#        （Debug SRAM .rodata / Release NOR .rodata），激活 332,928 B 由固件
#        静态数组提供；不产生 NOR 权重 hex。
set -euo pipefail

STEDGEAI="${STEDGEAI_CLI:-}"
if [ -z "$STEDGEAI" ] || [ ! -x "$STEDGEAI" ]; then
  echo "ERROR: STEDGEAI_CLI 未设置或不可执行（需 STEdgeAI 4.0.1 CLI）。" >&2
  exit 1
fi

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

{
  echo "# stedgeai version: $("$STEDGEAI" --version)"
  echo "# model: reatt_raw_int8.tflite  form: all-CPU (no --st-neural-art)"
} > generate_401.log

"$STEDGEAI" generate --no-inputs-allocation --no-outputs-allocation \
  --model reatt_raw_int8.tflite --target stm32n6 --name reatt8 2>&1 | tee -a generate_401.log

mv st_ai_output/reatt8.c st_ai_output/reatt8.h st_ai_output/reatt8_data.c \
   st_ai_output/reatt8_data.h st_ai_output/reatt8_details.h .
echo "OK: reatt8.c/.h + reatt8_data.c/.h + reatt8_details.h + generate_401.log"
