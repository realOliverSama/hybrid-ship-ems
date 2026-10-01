#!/bin/bash
# EMS 演示一键启动——电脑跑被控对象模型（Simulink PlantStepE），N6 板跑烧录的
# EMS 控制器（flash 自启，无需调试器）。双击本文件或终端执行。
# 用法: start_demo.command [拍数]   默认 60 拍 ≈ 12 分钟（每拍 10s 决策 + 1Hz 执行）
set -u
TICKS="${1:-60}"

ROOT="$(cd "$(dirname "$0")" && pwd)"
while [ "$ROOT" != "/" ] && [ ! -x "$ROOT/.venv_tf/bin/python" ]; do ROOT="$(dirname "$ROOT")"; done
PORT="/dev/cu.usbserial-1120"
PY="$ROOT/.venv_tf/bin/python"
TOOL="$ROOT/firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS04_CHIL_FULLCTRL/Host/tools/s4_c3_closedloop.py"

echo "=============================================="
echo " EMS 软硬件在环演示（HIL）"
echo "   PC 侧: Simulink PlantStepE 被控对象模型"
echo "   板侧: N6 ThreadX EMS 控制器（flash 自启）"
echo "   链路: UART 115200（TICK 10s / EXEC 1Hz）"
echo "=============================================="

if [ ! -x "$PY" ]; then echo "[X] 未找到 python: $PY"; read -n 1 -s; exit 1; fi
if [ ! -e "$PORT" ]; then
  echo "[X] 未找到串口 $PORT（CH340）"
  echo "    请确认: 板子上电（串口 USB 供电）、USB 线插好"
  read -n 1 -s; exit 1
fi
echo "[1/2] 板卡串口: $PORT ✓"

echo "[2/2] 启动闭环（$TICKS 拍 ≈ $((TICKS / 6 + 2)) 分钟；Ctrl+C 可随时停止）"
echo "      LCD 同步显示: 功率流 / 负荷预测对比曲线 / 实时数字"
"$PY" "$TOOL" --port "$PORT" --baud 115200 --formal-ticks "$TICKS" --exec-period 1
RC=$?
echo "=============================================="
echo " 演示结束 (rc=$RC)；结果: firmware/.../Host/results/s4_c3/"
read -n 1 -s -r -p "按任意键关闭..."
exit $RC
