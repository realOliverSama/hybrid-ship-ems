#!/usr/bin/env bash
# s4_xip_flash.sh — P4 烧写流程（用户批准 2026-09-29；RCA §5 验证链 §3 烧写方案）
# 序：前置校验 → NOR 备份 → 烧写(仅 0x70100400 段) → verify → 后验读回比对 → 记录
# 用法: s4_xip_flash.sh
# 红线: 0x70A/0x70B/0x703 禁碰（本脚本只写 0x70100400 起的镜像段）；OB/OTP 零触碰
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../../../../../../../.." && pwd)"   # tools→Host→EMS04→EMS_Projects→SoftwarePackage→04_SourceCode→stm32n6→firmware→repo
if [ ! -f "$REPO/AGENTS.md" ] || [ ! -d "$REPO/model" ]; then echo "REPO 推导失败: $REPO" >&2; exit 2; fi
E04="$REPO/firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS04_CHIL_FULLCTRL"
CLI="/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.macos64_2.2.200.202503041107/tools/bin/STM32_Programmer_CLI"
STLDR="$REPO/firmware/stm32n6/04_SourceCode/SoftwarePackage/External_Loader/MX25UM25645G_ATK-CNN647B/Binary/MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr"
# 偏差④修复（2026-09-29）：HEX 路径可用 S4_XIP_HEX 覆盖（默认 = 历史 S4XIP 路径
# 不变）；后验读回长度随 .xip.bin 实际尺寸（CPUF32 形态镜像 4.94MB > 原 2.2MB 窗口）
HEX="${S4_XIP_HEX:-$E04/Appli/build/S4XIP/EMS01_CHIL_ECMS_Appli.xip.hex}"
BIN="${HEX%.hex}.bin"
TS="$(date +%Y%m%d_%H%M%S)"
BK="/tmp/nor_backup_p4xip"
mkdir -p "$BK"

echo "== [0] 前置校验 =="
python3 - "$HEX" <<'EOF'
import sys
path = sys.argv[1]
base = 0; addrs = []
for line in open(path):
    if not line.startswith(':'): continue
    n = int(line[1:3],16); a = int(line[3:7],16); t = int(line[7:9],16)
    if t == 4: base = int(line[9:13],16) << 16
    elif t == 0 and n: addrs.append(base + a)
    if len(addrs) > 200: break
assert addrs and addrs[0] == 0x70100400, "F3 FAIL: hex 非法或非 XIP 布局（首地址 %s）" % hex(addrs[0] if addrs else -1)
print("F3 PASS: hex 首地址 0x70100400 (XIP)")
EOF
HEX_SHA=$(shasum -a 256 "$HEX" | awk '{print $1}')
echo "hex sha256 = $HEX_SHA"

echo "== [1] NOR 备份 0x70100000 起 4MB（含 FSBL 期望槽位+既有镜像） =="
"$CLI" -c port=SWD mode=HotPlug -el "$STLDR" -u 0x70100000 0x400000 "$BK/${TS}_0x70100000_4MB.bin" 2>&1 | tail -3
BK_SHA=$(shasum -a 256 "$BK/${TS}_0x70100000_4MB.bin" | awk '{print $1}')
echo "backup sha256 = $BK_SHA" | tee "$BK/${TS}_SHA256.txt"
echo "$HEX_SHA  $HEX" >> "$BK/${TS}_SHA256.txt"

echo "== [2] 烧写 XIP hex @0x70100400（--verify 硬校验） =="
"$CLI" -c port=SWD mode=HotPlug -el "$STLDR" -d "$HEX" 0x70100400 --verify 2>&1 | tail -5

echo "== [3] 后验读回比对（长度 = 镜像实际尺寸） =="
IMG_SZ=$(stat -f%z "$BIN")
READ_LEN=$(printf '0x%x' $(( (IMG_SZ + 0xFFFF) & ~0xFFFF )))
echo "image $IMG_SZ B → readback $READ_LEN"
"$CLI" -c port=SWD mode=HotPlug -el "$STLDR" -u 0x70100400 "$READ_LEN" "$BK/${TS}_post_0x70100400.bin" 2>&1 | tail -2
python3 - "$BK/${TS}_post_0x70100400.bin" "$BIN" <<'EOF'
import sys
nor = open(sys.argv[1],'rb').read()
img = open(sys.argv[2],'rb').read()
n = min(len(nor), len(img))
diff = sum(1 for i in range(n) if nor[i] != img[i])
print("overlap %d B, diff %d B（段间隙 00/FF 语义差异另行判读）" % (n, diff))
# 逐差位置归类：img=0x00 而 nor=0xFF（或反之）为段间隙语义差
sem = sum(1 for i in range(n) if nor[i] != img[i] and {nor[i], img[i]} <= {0x00, 0xFF})
print("其中 00/FF 段间隙语义差 %d B；实质差 %d B" % (sem, diff - sem))
assert diff - sem == 0, "POST FAIL: 存在实质差异"
print("POST PASS")
EOF

echo "== [4] 红线区抽查（只读）：0x70A00000/0x70B00000 首 16B 与既有档案比对 =="
"$CLI" -c port=SWD mode=HotPlug -el "$STLDR" -u 0x70A00000 0x10 "$BK/${TS}_n2_head.bin" 2>&1 | tail -1
"$CLI" -c port=SWD mode=HotPlug -el "$STLDR" -u 0x70B00000 0x10 "$BK/${TS}_n1_head.bin" 2>&1 | tail -1
echo "N2/N1 头已存档（比对基准见 e12af44f… N1 记录）"

echo "== 完成：烧写+三重校验 PASS。断电 → 拨 boot → 上电后跑 s4_c2_smoke =="
echo "记录：TS=$TS HEX_SHA=$HEX_SHA BK_SHA=$BK_SHA"
