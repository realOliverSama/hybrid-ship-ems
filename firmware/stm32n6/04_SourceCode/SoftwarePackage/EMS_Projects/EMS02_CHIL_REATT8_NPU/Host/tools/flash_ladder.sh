#!/usr/bin/env bash
# flash_ladder.sh — phase4-g 晨间判别实验一键烧录（Dev boot 下执行）。
#
# 烧录清单（全部带 -v 校验；外部 loader + UR 连接）：
#   appli                       0x70100400  （含阶梯执行器 + golden，ROM ≈2.55MB）
#   network-data-reattn2n.hex   0x70A00000  （服务 N2 权重，池已迁避让 appli）
#   network-data-dl1.hex        0x70410000  （L1 kf→fut_proj+位置）
#   network-data-dl2.hex        0x70440000  （L2 单块 attention）
#   network-data-dl4.hex        0x70600000  （L4 fusion+decoder）
#   network-data-dlf1.hex       0x70800000  （F1 整图 --Ono-clone-dma 消融）
#   network-data-reattself.hex  0x703E0000  （平凡自检图，重烧无害）
# 不触碰：FSBL 0x70000000、workclf 0x70400000、mlpq/tcnq 旧区。
#
# 用法:  STEDGEAI 无关；bash flash_ladder.sh
set -euo pipefail
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ=$(cd ../.. && pwd)   # Host/tools → 工程根
CLI="${STM32_CLI:-$HOME/Library/Application Support/stm32cube/bundles/programmer/2.23.0/bin/STM32_Programmer_CLI}"
LDR="$PROJ/../../External_Loader/MX25UM25645G_ATK-CNN647B/Binary/MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr"

APPLI="$PROJ/Appli/build/flash/EMS01_CHIL_ECMS_Appli.hex"
B="$PROJ/Binary"

echo "== 烧录清单 sha256 前 16 =="
for f in "$APPLI" "$B"/network-data-reattn2n.hex "$B"/network-data-dl1.hex \
         "$B"/network-data-dl2.hex "$B"/network-data-dl4.hex \
         "$B"/network-data-dlf1.hex "$B"/network-data-reattself.hex; do
  printf '  %-46s %s\n' "$(basename "$f")" "$(shasum -a 256 "$f" | cut -c1-16)"
done

"$CLI" -c port=SWD mode=UR -el "$LDR" \
  -w "$APPLI" -v \
  -w "$B/network-data-reattn2n.hex" -v \
  -w "$B/network-data-dl1.hex" -v \
  -w "$B/network-data-dl2.hex" -v \
  -w "$B/network-data-dl4.hex" -v \
  -w "$B/network-data-dlf1.hex" -v \
  -w "$B/network-data-reattself.hex" -v

echo "ALL_FLASHED_VERIFY_OK — 请切回 Flash boot 并完整断电重上电"
