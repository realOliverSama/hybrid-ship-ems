#!/bin/bash
# EMS01_CHIL_ECMS — MLP-Q int8 N6 部署包再生成（FR-502 / FR-511 / NFR-503）
#
# 用法:  cd <本目录> && ./generate-n6-model.sh
# 前置:  环境变量 STEDGEAI_CLI 指向锁定的 ST Edge AI Core v2.2.0 CLI
#        （版本锁定依据: vendor NetworkRuntime1000 ↔ st-neural-art 10.x，禁用其它版本）
# 产物:  network.c / network.h / network_ecblobs.h / network_atonbuf.xSPI2.raw（本目录）
#        ../../Binary/network-data-mlpq.hex（权重 @0x70200000，与 my_mpools/stm32n6.mpool xSPI2 池一致）
# 源模型: mlp_q.tflite（源 = WS/results/ecms_phase25/models/mlp_h64_32_lr3e-4_q/
#        mlp_h64_32_lr3e-4_q_int8.tflite 副本，sha256 56fc3488…0c32，只读）
set -euo pipefail

MODEL="mlp_q"
WEIGHTS_BASE="0x70200000"   # 必须与 my_mpools/stm32n6.mpool 的 xSPI2 池基址一致

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

STEDGEAI="${STEDGEAI_CLI:-}"
if [ -z "$STEDGEAI" ] || [ ! -x "$STEDGEAI" ]; then
  echo "ERROR: STEDGEAI_CLI 未设置或不可执行。" >&2
  echo "  请指向 ST Edge AI Core v2.2.0 CLI（锁定版本），例如:" >&2
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
  echo "# model: $MODEL  weights-base: $WEIGHTS_BASE"
} > generate.log

"$STEDGEAI" generate --no-inputs-allocation --no-outputs-allocation \
  --model "$MODEL.tflite" --target stm32n6 \
  --st-neural-art default@user_neuralart.json 2>&1 | tee -a generate.log

cp st_ai_output/network_ecblobs.h .
cp st_ai_output/network.c .
cp st_ai_output/network.h .
cp st_ai_output/network_atonbuf.xSPI2.raw network_data.xSPI2.bin
"$OBJCOPY" -I binary network_data.xSPI2.bin --change-addresses "$WEIGHTS_BASE" \
  -O ihex ../../Binary/network-data-mlpq.hex

echo "OK: network.c / network.h / network_ecblobs.h / network_atonbuf.xSPI2.raw / generate.log"
echo "OK: ../../Binary/network-data-mlpq.hex (weights @ $WEIGHTS_BASE)"
