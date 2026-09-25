# Host/tools/npu — REAT-T NPU 形态导出/量化/拆分脚本（phase4-g NPU 线）

本目录工具服务于 "Keras REAT-T → ONNX → int8 QDQ → stedgeai 编译" 的 NPU 形态
部署链。全部脚本按 `_find_repo_root` 相对路径推导（AGENTS.md §20.2），无绝对路径。

| 脚本 | 用途 | 关键点 |
|---|---|---|
| `npu_export_tanh.py` | 部署变体导出（初版） | NPU 编译器不支持 Erfc（精确 GELU）→ GELU 换 tanh 近似（corr 0.999） |
| `npu_export_static.py` | 静态 attention 重写（v2） | Keras MHA 的 tf.shape→Reshape 运行时长链致 "Unknown dimensions: W"；改写为显式常量 Reshape/Permute/MatMul/Softmax |
| `onnx_static_fold.py` | ONNX 常量化手术 | 把 Shape/Gather/Concat 运行时形状链折叠为常量 |
| `onnx_fix_midpad.py` | 中间维 Pad 原位替换（v2 路线） | Pad 作用在非最内维 → Transpose-Pad-Transpose 手术（工具可编但板端退化，已被 v3 取代） |
| `npu_export_concat.py` | **v3 导出（现行整图形态）** | 静态 attention + tanh GELU + **Concat 零填充**（Slice→Mul(0)→Concat，无 Pad） |
| `npu_quantize_int8.py` | 整图 int8 QDQ 量化 | 标定 = 归一化 train 窗 1024（部署口径，与 CPU 形态同抽样/同变换） |
| `onnx_truncate.py` | 按张量截断 ONNX | NPU 编译器故障二分定位用（逐层缩短图找首个致病算子） |
| `npu_export_split.py` | **拆分导出（2026-09-23）** | N1 = hist→h_past（TCN 段，走 CPU 形态）；N2 = kf+h_past→raw (m,a,b)（attention/Dense 段，走 NPU）；权重自冻结 winner 移植 |
| `npu_quantize_split.py` | **拆分 int8 量化（2026-09-23）** | `--part n1\|n2`；N2 的 h_past 标定输入取 **N1 int8 输出**（部署口径，非 fp32） |
| `npu_export_ladder.py` | **N2 逐级 bisection 阶梯（2026-09-23 夜备）** | `cut/golden/emit-c` 三步：切 dl1..dl4 子图 + dlf1 消融变体、ORT golden 提取、固件判决数据生成；一次烧录全部跑完 |
| `npu_bypass_ln_qdq.py` | 拆掉 N2 里 9 个 LayerNorm 的激活 Q/DQ | 方差/sqrt 保持浮点；产物 `npu_safe/reattn2n_lnfloat.onnx`，编译名 `reattn2l` |
| `flash_ladder.sh` | 阶梯固件一键烧录（7 项全带 -v 校验） | Dev boot 下执行；清单含 appli/reattn2n@0x70A00000/dl1/dl2/dl4/dlf1/reattself |

## 与 stedgeai 的配合

```text
导出：python3 npu_export_split.py            → reatt_n1_{fp32,int8}.onnx / reatt_n2_{fp32,int8}.onnx
量化：python3 npu_quantize_split.py --part n1|n2
编译（N1 CPU 形态）： stedgeai generate --target stm32n6 --model reatt_n1_int8.onnx --name reattn1c
编译（N2 NPU 形态）： stedgeai generate --target stm32n6 --model reatt_n2_int8.onnx \
                      --st-neural-art default@user_neuralart.json --name reattn2n \
                      --load-mpool <工程>/Model/reatt8_q_npu/my_mpools/stm32n6.mpool ...
```

生成的 C 代码落在 `Model/reatt8_q_npu/part_n1|part_n2/`（入库，供 CMake 直接编译）；
stedgeai 的中间工作目录（`st_ai_output/`、`st_ai_ws/`，含 OE ONNX 与池文件副本）
**不入库**（本地可再生，体积 37 MB / 104 MB）。

板级验证工具（对拍/探针/workclf 判别/晨间阶梯判决）在上一层 `Host/tools/`：
`reatt_board_client.py`（`--form whole|split`）、`reatt_split_probe.py`、
`wc_test_client.py`、`gen_reference_split.py`、`ladder_morning.py`。
