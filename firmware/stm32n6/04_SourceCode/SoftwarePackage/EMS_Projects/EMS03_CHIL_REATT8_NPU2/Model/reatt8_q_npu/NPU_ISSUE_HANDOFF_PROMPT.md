# NPU_ISSUE_HANDOFF_PROMPT — REATT-on-NeuralART 问题移交提示词（2026-09-23）

> 用途：在新的 AI 会话中整体投喂，请对方以 ST Neural-ART / STM32N6 NPU 部署专家
> 身份协助定位。本文档即提示词正文；方括号内容投喂前按需补充。

---

## 提示词正文（复制以下全部内容）

你是 STM32N6 Neural-ART NPU 与 ST Edge AI Core 工具链的资深部署专家。请基于以下
完整事实，帮我定位"模型在 NPU 上可启动但计算结果错误 + 切分子图完全不推进"的根因，
并给出按可能性排序的假设清单、每个假设的验证实验、以及修复方案。如果你知道 ST
4.0.1 的已知缺陷/勘误/相关案例，请明确指出并给出出处。

### 平台与工具链
- 板：AlienTek ATK-CNN647B（STM32N647，Neural-ART 14 NPU + Cortex-M55 Helium）
- 工具：ST Edge AI Core **4.0.1**（generate 产物自检：atonn 1.1.3-275 /
  NetworkRuntime1201_CM55_GCC.a）；目标 stm32n6；NPU 形态
  `--st-neural-art`，选项：`--enable-epoch-controller --all-buffers-info
  --no-hw-sw-parallelism --cache-maintenance --enable-virtual-mem-pools
  --native-float --optimization 3 --Os --Omax-ca-pipe 4 --Ocache-opt
  --expand-softmax`
- 运行时：官方 4.0.1 N6 示例同款 ll_aton 源集 + `ll_aton_stai_internal.c`（stai API，
  ASYNC 模式 + run_continue/WFE）；裸机（LL_ATON_OSAL_BARE_METAL）
- 内存池：激活 npuRAM3-5（AXISRAM 0x34200000 起，各 448KB contiguous），权重
  xSPI2 NOR octoFlash（CACHEABLE_ON，memory-mapped），EC blobs 编入代码段
- 权重烧录：外部 loader 逐字节校验一致；appli 读 NOR 权重区校验和与主机一致

### 模型（REAT-T，量化分位数回归预测器）
- 1,012,835 参数；结构：past 分支 = TCN（Conv1D Cp=96，dilation 1..32，
  LayerNorm+GELU(tanh 近似)，残差）→ learned-query attention pooling → [1,96]；
  future 分支 = Dense(128)+位置编码 → 3×(PreLN → MHA(4头) → Add → PreLN →
  FFN(4×GELU) → Add)；fusion（h_past tile 拼接 → LN → Dense）→ decoder → raw
  (m,a,b) [1,30,3]
- 部署图：keras → 输出裁剪到 raw_proj（TFLite SOFTPLUS 溢出对策，分位变换移出
  模型）→ int8 量化（TFLite 整数量化，标定集=部署口径归一化 train 窗 1024）
  → tf2onnx → ONNX int8 (QDQ) → stedgeai generate
- host 端 ORT 跑同一 ONNX：与 keras fp32 对比 q50 MAE 23.5 kW / corr 0.981（图本身
  语义正确）

### 症状（板上实测，证据链完整）
1. **整图 NPU 形态**：analyze/generate 全 PASS、可烧录、run() 返回 STAI_DONE、
   耗时 ~175-300ms；但**输出与输入完全无关**——30 个 lead 的输出为同一常数三元组
   （int8 码 m=−128=a≈b，即输出≈量化零点/偏置量级），跨调用缓慢漂移，同输入重复
   调用有 ±1 LSB 非确定。golden 输入下逐字节比对 **0/90**（期望码值如 (−19,−125,−125)）。
2. **切分子图**（把整图在中间张量处切成 4 个子图，各自独立 --name 编译）：即使最小
   的子图（全 f32、仅 Unsqueeze/Reshape/Q/DQ/Slice/Tile、**零权重**）也在
   `run_continue` 内部**永久停滞**——不报错、不 DONE、不前进。**切子图无法执行**。
3. **对照实验（证明 NPU 通路本身正常）**：
   - 平凡自检图（x→Q→DQ→MatMul(恒等I8)→Q→DQ→MatMul(恒等)→Q，权重 145B 放同款
     NOR 池）：输出与输入**逐字节相等**（NPU 执行、NOR 权重读取、量化交接全对）；
   - workclf（小型 Dense 分类器，全 NPU + NOR 权重）：**40/40 = 100% 类别一致**；
   - appli 侧 CPU 读 NOR 权重区校验和与烧录内容逐字节一致。
4. **生命周期**：官方 hello_world 流（`get_inputs` 取运行时缓冲直填 →
   `stai_ext_network_new_inference` → `run(ASYNC)` → WFE/run_continue →
   `get_nn_run_status` 至 DONE）。已知首拍 new_inference 返回
   STILL_RUNNING(0x50000)（init 后 exec_status=SUCCESS，reset 只认 DONE/WFE），
   官方示例不检查该返回码——我们已容忍。跳过首拍与容忍首拍两种写法症状相同。
5. **WFE 现象**：子图 run(ASYNC) 返回 RUNNING_WFE 后，第一次
   `stai_ext_wfe()` 能返回、`run_continue` 推进一拍，之后状态恒 RUNNING_WFE，
   **第二次 wfe 永久睡死**；改忙轮询（不用 wfe）后 run_continue 内部同样不前进。
   整图（reattn2n）却能"完成"（DONE，175ms）——但结果是错的。
6. **cache 契约**：输入 clean / 输出 invalidate 已按官方 cache-maintenance 文档实现；
   NPU cache 已使能（CACHEAXI 时钟/复位 + RCC->MEMENSR AXISRAM3-6/CACHEAXIRAM 全
   使能，npu_cache_enable 已调——与同板简单图可执行相互印证）。
7. **量化参数**：运行时经 stai get_info 读取（scale/zeropoint 与工具 Q.json 一致），
   已逐项核对。
8. **历史对照**：同板同运行时，2026-09-03 一个 **TCN 卷积图** NPU 化后数值部分偏差
   （34%/47.7% 逐位一致）；小型 Dense 图正常。即：失败似乎特定于"较大/多 epoch/
   attention/Tile 组合"的图，小图正常。

### 已排除
算子支持问题（能编译能启动）；权重烧录/读取（三重校验）；输入毒化（golden 输入
下 0/90）；cache 用户缓冲契约；量化参数错配；I/O 绑定 API 用法（官方流/自缓冲流
都试过）；地址对齐（1MB 对齐池）；HyperRAM 依赖（占用 0）；--expand-softmax
（开/关同样复现）；--Ono-clone-dma（同样复现）。

### 请你输出
1. 按可能性排序的根因假设（≥5 条），每条给出：机制解释（为何恰好"输出=偏置量级
   常数"且切子图停滞在 run_continue）、区分性验证实验（可在板上/主机做的最小实
   验）、预期修复；
2. ST Edge AI 4.0.1 已知相关缺陷/勘误/限制清单（尤其是 EC 多 epoch 图、
   --native-float、clone_dma、expand-softmax、--Ocache-opt、虚拟内存池）；
3. 是否建议改用 `stedgeai validate --mode target`（官方 on-target 校验固件）做
   逐层对拍，以及该固件在 N6 上的构建要点；
4. 若确认工具链缺陷：向 ST 支持递交的最小复现包应包含哪些内容；
5. 其他大模型/AI 可能忽略但关键的排查点（例如 EC blob 加载、npuRAM 多 bank 交错、
   XSPI2 对 NPU master 的 CacheAXI 属性、atonn 中间表示检查手段）。

### 可提供的材料（如需请列清单）
整图与切子图的 ONNX（含 OE 展开版 + Q.json）、generate 报告、EC blobs 头、板上
串口原始日志、golden 输入/期望输出 npz、复现用 Host 工具脚本（导出/量化/编译/
烧录/对拍全套）。

---

## 附：仓库内对应物（供新会话快速引用）
- 阶梯/切图导出：`Host/tools/npu/npu_export_ladder.py`（cut/golden/emit-c）
- 部署链：`Host/tools/quantize_reatt.py`、`Host/tools/npu/npu_export_concat.py`
- 板级对拍：`Host/tools/reatt_board_client.py`（--form whole|split）、
  `ladder_morning.py`（一键判决）、`wc_test_client.py`（workclf 对照）、
  `reatt_split_probe.py`
- 定性记录：`Model/reatt8_q_npu/NOTE.md`（假设台账 + 三级终局判决）
- 对照工程（可执行形态）：`../EMS02_CHIL_REATT8_CPU/`（整图 CPU，0.8s/窗，已验收）
