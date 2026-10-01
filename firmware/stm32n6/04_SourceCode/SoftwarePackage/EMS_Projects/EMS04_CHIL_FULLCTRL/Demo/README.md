# Demo — EMS 软硬件在环一键演示

**给导师演示用**：电脑跑被控对象模型（Simulink PlantStepE 船舶动力流），N6 板跑
烧录好的 ThreadX EMS 控制器（flash 自启），UART 115200 实时闭环。

## 运行（一步）

板子上电（flash boot 拨位）+ 串口 USB 线接电脑，然后**双击 `start_demo.command`**
（或终端执行 `./start_demo.command [拍数]`，默认 60 拍 ≈ 12 分钟）。

## 你会看到

- **N6 板 LCD**：功率流图 + 负荷曲线（左半=过去 30 档"实际 vs 当时预测"同轴
  对比，右半=未来 30 拍预测）+ 实时数字（SOC/功率/航速）1Hz 刷新
- **电脑终端**：每 10 秒一个决策拍日志（bb_d1=板决策 / bh_d1=host 参照 / soc），
  每 1 秒一次执行层刷新；跑完输出 G6 门判定与 CSV 曲线数据
  （`Host/results/s4_c3/`）

## 前置条件

| 项 | 要求 |
|---|---|
| 板子 | flash boot 拨位 + 上电（EMS 程序自启，LCD 亮即就绪） |
| 串口 | `/dev/cu.usbserial-1120`（CH340，同时是板子供电） |
| python | 仓库根 `.venv_tf/`（已含 keras/scipy/pyserial） |

## 停止 / 重跑

Ctrl+C 随时停止；再次双击即重跑。航次长度用拍数调节（建议 30~120）。
