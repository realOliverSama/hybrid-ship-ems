#!/bin/bash
# EMS01_CHIL_ECMS_TCN64 — workclf int8 N6 部署包再生成（4.0.1 stai 版）
#
# 用法:  cd <本目录> && STEDGEAI_CLI=<stedgeai 4.0.1 CLI 路径> ./generate-n6-model-stai401.sh
# 前置:  环境变量 STEDGEAI_CLI 指向 ST Edge AI Core **4.0.1** CLI（层②迁移，
#        2026-09-03 用户指令"最新工具链重试层②"；atonn 1.1.3-275，
#        NetworkRuntime1201；生成代码版本自检 LL_ATON_VERSION_* == 1.1.3-275）。
#        绝对路径不入库（AGENTS.md §20.2），CLI 路径一律经环境变量注入。
# 产物:  workclf.c / workclf.h / workclf_ecblobs.h / stai_workclf.c /
#        stai_workclf.h（本目录，--name workclf 符号后缀）+
#        workclf_data.xSPI2.bin（权重原始镜像副本）+
#        ../../Binary/network-data-workclf.hex（权重 @0x70400000，与
#        my_mpools/stm32n6.mpool xSPI2 池一致）
# 源模型: workclf.tflite（源 = WS/results/ecms_phase25/classifier/
#        workclf_v1_int8.tflite 副本，sha256 7c3571e4…f293d，只读；
#        量化口径与 2.2 时代部署一致，Conv1D 预期 SW fallback 属预期）
# 注意:  2.2 生成物（network.{c,h}/network_ecblobs.h/network_data.xSPI2.bin，
#        泛名 Default 符号）保留不动——EMS_PREDICTOR=mlpq 可逆性出口
#        （ems_npu_workclf.c）仍引用；两代产物并存于本目录。
set -euo pipefail

MODEL="workclf"
WEIGHTS_BASE="0x70400000"   # 必须与 my_mpools/stm32n6.mpool 的 xSPI2 池基址一致

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

STEDGEAI="${STEDGEAI_CLI:-}"
if [ -z "$STEDGEAI" ] || [ ! -x "$STEDGEAI" ]; then
  echo "ERROR: STEDGEAI_CLI 未设置或不可执行。" >&2
  echo "  请指向 ST Edge AI Core 4.0.1 CLI（层②迁移锁定版本），例如:" >&2
  echo "  export STEDGEAI_CLI=<安装根>/Utilities/macarm/stedgeai" >&2
  exit 1
fi
OBJCOPY="${OBJCOPY:-arm-none-eabi-objcopy}"
if ! command -v "$OBJCOPY" >/dev/null 2>&1; then
  echo "ERROR: 未找到 $OBJCOPY（arm-none-eabi 工具链未在 PATH）。" >&2
  exit 1
fi
mkdir -p ../../Binary

{
  echo "# stedgeai version: $("$STEDGEAI" --version)"
  echo "# model: $MODEL  weights-base: $WEIGHTS_BASE  name: $MODEL (stai 4.0.1)"
} > generate-stai401.log

"$STEDGEAI" generate --no-inputs-allocation --no-outputs-allocation \
  --model "$MODEL.tflite" --target stm32n6 --name "$MODEL" \
  --st-neural-art default@user_neuralart.json 2>&1 | tee -a generate-stai401.log

cp st_ai_output/workclf_ecblobs.h .
cp st_ai_output/workclf.c .
cp st_ai_output/workclf.h .
cp st_ai_output/stai_workclf.c .
cp st_ai_output/stai_workclf.h .
cp st_ai_output/workclf_atonbuf.xSPI2.raw workclf_data.xSPI2.bin
"$OBJCOPY" -I binary workclf_data.xSPI2.bin --change-addresses "$WEIGHTS_BASE" \
  -O ihex ../../Binary/network-data-"$MODEL".hex

echo "OK: workclf.{c,h} / workclf_ecblobs.h / stai_workclf.{c,h} / workclf_data.xSPI2.bin / generate-stai401.log"
echo "OK: ../../Binary/network-data-$MODEL.hex (weights @ $WEIGHTS_BASE, 4.0.1 版)"
echo "NOTE: 2.2 生成物 network.* 与 network_data.xSPI2.bin 保留（mlpq 可逆性出口）"