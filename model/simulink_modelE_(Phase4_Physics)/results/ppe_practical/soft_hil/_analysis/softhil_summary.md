# phase4-g soft-HIL 配对摘要（非正式）

基线：冻结 genB C2 host keras fp32；差 = board − host。

| voyage | fuel_board | fuel_host | Δfuel kg | Δfuel % | ΔSOC_end | ΔJ_FEC |
|---|---|---|---|---|---|---|
| FLTA_S2_D01009_M1 | 155.78 | 156.25 | -0.47 | -0.303% | -0.00090 | +0.05 |

q50 逐锚点（板 vs keras fp32 重放，n=377）： RMSE mean 26.73 kW / p99 67.31；板推理 375921321 µs mean（p99 4288408044）；往返 2704 ms mean。
