# law=7 内联 MLP 前向 Stateflow 解析失败——根因报告（已结案）

- **日期**: 2026-08-26
- **分支**: debug/MATLAB-debug（基于 feat/adaptive-ecms-methanol @ 4baac9c1）
- **环境**: MATLAB R2026a Update 3（26.1.0.3276743）/ HybridShip_ModelD_DCBus / md5 ada8a732ba2e8ad47e904d5c76880efc
- **状态**: **ROOT CAUSE CONFIRMED + FIX VERIFIED（编译级 + 数值级 S01 闭环逐位一致）**

> **后续补充（同日）**：编译修复打通后发现并修复了第二颗隐藏 bug——wip 原始代码的环形缓冲
> 重排为时间降序（xP(1)=最新），而数据集 feature_layout 要求时间升序；兕底版重写时已写对。
> 该 bug 因内联版此前无法编译/运行而从未暴露。修复升序重排 + 补齐 t<lastT 新仿真重置后，
> S01 闭环对比：P_gen_ref 92,785 采样点、fuel、SOC_end、Vdc_min 全部**逐位一致**
> （results/ecms_phase2/debug_law7/inline_verification.json）。详见 §8。

---

## 1. 结论

`Stateflow:cdr:ErrorsParsingEmlFcn`（update 阶段、无行号、无 cause 链）的根因是
**R2026a Stateflow EMChart 编译器的结构性缺陷**，触发条件与 MLP 内容完全无关：

> **触发三要素（缺一不可的组合，其中③为核心）：**
> 1. 函数持有 persistent 状态（环形缓冲）；
> 2. for 循环内做索引重排（`i = k - j; if i < 1, i = i + 300; end`）；
> 3. **变量 `x` 在第一个 `if` 条件块内首次定义（循环结果与标量输入拼接），随后在第二个 `if` 块中以任何形式使用**（包括标量索引 `x(1)`）。

只要把 `x` 的构造移到条件块外（无条件执行），同样的完整 NN 前向即可编译通过。

## 2. 最小复现（零图级参数，已在真实模型上实证 FAIL）

```matlab
function P = ecms_nn_min(t, P_load, v_ship)
persistent lastT buf k vprev
if isempty(lastT)
    lastT = -1e9; k = 1;
    buf = zeros(300, 1); buf(:) = P_load;
    vprev = v_ship;
end
P = 0;
if t - lastT < 1.0 - 1e-9, return; end
lastT = t;
if isfinite(P_load), buf(k) = P_load; end
k = mod(k, 300) + 1;
accel = v_ship - vprev; vprev = v_ship;
ok = isfinite(P_load) && isfinite(v_ship) && isfinite(accel);
if ok
    xP = zeros(300, 1);
    for j = 1:300
        i = k - j;
        if i < 1, i = i + 300; end
        xP(j) = buf(i);
    end
    x = [xP; v_ship; accel];      % ← if 块内首次定义
    ok = all(isfinite(x));
end
if ok
    P = sum(x);                    % ← 跨块使用（任何形式都触发）
else
    P = 0;
end
end
```

该函数不含任何 ECMS_NN_* 图级参数、不含网格穷举、不含 BSFC 表——仍然 100% 复现
同一错误。可作为向 MathWorks 报告的最小案例（建议附上本报告 JSON 证据链）。

## 3. 修复（已验证，数值语义等价）

```matlab
% 原（崩）：x 构造在 if ok 块内
% 修（过）：x 构造无条件执行，ok 用短路与合并
xP = zeros(300, 1);
for j = 1:300
    i = k - j;
    if i < 1, i = i + 300; end
    xP(j) = buf(i);
end
x = [xP; v_ship; accel];
ok = ok && all(isfinite(x));      % 数值结果与原短路逻辑一致
if ok
    <完整 NN 前向>
    ...
end
```

**等价性论证**：原版在 `ok=false` 时跳过 x 计算但其值从不被使用；修复版总是计算
x 但仍仅在 `ok=true` 分支使用。控制流可见行为完全一致，仅每次 1 Hz 门控激活时
多一次 302 元素向量构造（微秒级，相对 1 s 控制周期可忽略）。

## 4. 证据链总览（4 轮 × 22 组单因子实验，全部在真实模型上进行）

### 第 1 轮（exp_matrix_results.json）

| 实验 | 结果 | 排除项 |
|---|---|---|
| EXP-0a wip 内联原样 | FAIL | 基线复现 ✓ |
| EXP-0b 前向换 y_norm=0 | PASS | 对照 ✓（同时排除图数据数量嫌疑）|
| EXP-1 全量显式 paramSizes | FAIL | ~~参数尺寸继承推断~~ |
| EXP-2 预转置权重消除 .'/(:) | FAIL | ~~转置/广播解析缺陷~~ |
| EXP-3a/b/c 前向逐语句二分 | 全 FAIL | **最小失败语句 = xnorm 行** |

### 第 2 轮（exp_matrix2_results.json）

| 实验 | 结果 | 排除项 |
|---|---|---|
| 阶段诊断（全部 6 组） | setter 全 OK，update 全 FAIL | 失败锁定 update 编译阶段 |
| EXP-7 纯局部 ones(302,1) 向量运算 | FAIL 同款错误 | **~~ECMS_NN 参数对象~~（决定性）** |
| 4b/4c/4d 简单赋值 | MultipleErrors | 错误形态分化提示 |

### 第 3 轮（exp_matrix3_results.json）

| 实验 | 结果 | 排除项 |
|---|---|---|
| EXP-D 输入口显式 Size='1' | FAIL | ~~输入口继承尺寸~~（历史排除项干净复测）|
| EXP-A y_norm = x(1) 标量索引 | FAIL | ~~向量运算~~（任何使用形式都触发）|
| EXP-C x 显式预分配 zeros(302,1) | FAIL | ~~x 尺寸不可证~~ |
| **EXP-B x 构造移出 if 块** | **PASS** | **修复方向确立（含完整前向）**|

### 第 4 轮（exp_matrix4_results.json）

| 实验 | 结果 | 归因 |
|---|---|---|
| F1 x 块内 + 无块内 ok 重赋值 | FAIL | ok 重赋值非必要条件 |
| F2 x 块外 + 保留 ok 重赋值 | PASS | ok 重赋值亦非充分条件 |
| MIN 零参数最小复现 | FAIL | **确证纯编译器 bug** |
| B2 修复候选复跑 | PASS | 修复稳定 |

## 5. 对历史存档结论的修正

`项目开发进展.md` 2026-08-25 条目中「转置形式——x.'*W 与 W.'*x 两种写法都失败」
以及 `+ecms2/nn_forward_code.m` 注释中「W1.' 矩阵转置形式通过」均为此轮推翻/修正：
两种转置形式的差异从未是问题本质——**任何引用跨条件块变量 x 的语句都会失败**，
与是否含转置无关。当时「探针环境不可用」「/tmp 阴影污染」两个教训正是根因长期
未定位的原因：该 bug 只能在真实模型的 update 编译中复现，而历史 bisect 因路径
污染反复失效。

## 6. 验证边界与后续建议

**已验证**：修复后的内联版（EXP-B/B2 结构）在真实模型上 update 编译通过（两轮稳定）。

**未验证（切换产品实现前必做）**：
1. 内联版数值正确性：T1 位一致性测试需改造 harness 后重跑（当前 test_law7 只覆盖
   extrinsic 版）；
2. S01/S20 回归逐位对照（内联 vs extrinsic）；
3. 若切回内联为产品实现：T-205 矩阵 + gate 重跑（Phase 2 收尾状态变更，属设计决策）。

**产品决策留待用户**：Phase 2 目前以 extrinsic 兑底收尾（gate=PARTIAL 已入库）。
内联修复已可用，是否切换、何时切换（如并入 Phase 3 NPU/MCU 分工设计）由用户裁决。
本分支不改动产品构建脚本。

**建议**：将 §2 最小复现提交 MathWorks 报告 R2026a EMChart 编译器缺陷。

## 7. 产物清单

- `scripts/debug_law7/exp_matrix{,2,3,4}.m` — 四轮实验脚本（可复现）
- `results/ecms_phase2/debug_law7/exp_matrix{,2,3,4}_results.json` — 机器可读结果

## 8. 数值一致性验证（2026-08-26 同日补充）

### 8.1 第二颗 bug：环形缓冲重排方向反了

编译打通后的首次闭环对比即暴露控制轨迹系统性分岔（dP mean 69 kW、fuel 差 2.4%）：

```matlab
% wip 原始内联版（错）：降序 —— xP(1)=最新 … xP(300)=最旧
for j = 1:300
    i = k - j; if i < 1, i = i + 300; end
    xP(j) = buf(i);
end
% ecms2.nn_predict_ehat 兕底版（对）：升序 —— xP(1)=P(t-299) … xP(300)=P(t)
xP = [buf(k:300); buf(1:k-1)];   % 与 datasets/fleet_norm.json feature_layout 一致
```

该 bug 在 wip 时期就存在，但因编译从未成功而无从暴露；兕底版重写时顺序已写对，
故 T1 位一致性测试（函数版 vs Python）未能覆盖此缺陷。另同步补齐 `t < lastT`
新仿真重置语义（连续多次仿真的 persistent 残留防护，与兕底版对齐）。

### 8.2 验证结果（exp_verify_inline.m，S01 闭环，EMS_CTRL_LAW=7）

修复后内联版 vs 产品兕底版，同一场景各跑一遍：

| 指标 | 结果 |
|---|---|
| P_gen_ref 逐点差 | **92,785 采样点 max\|Δ\|=0（逐位一致）** |
| fuel_total | 91.2317939047896 kg 双方逐位一致 |
| SOC_end / Vdc_min | 逐位一致 |
| 判据 | fuel 相对差<0.1%、dSOC<1e-3、dP≤5 kW — 全部满足（实际为 0）|

**传递性结论**：内联块 ≡ ecms2.nn_predict_ehat（本轮逐位实证）≡ Python 推理
（test_law7 T1，归一化空间 \|Δ\|<1e-5）→ 三方一致闭环。

机器可读证据：`results/ecms_phase2/debug_law7/inline_verification.json`。
脚本：`scripts/debug_law7/exp_verify_inline.m`（内存改图，.slx 零改动）。

### 8.3 产品化落地（2026-08-26 同日第三阶段）

数值验证通过后实施产品切换，过程中又暴露并解决了两颗隐藏问题：

1. **生成器遗漏别名块**：sprintf 重构时丢失 `W1 = ECMS_NN_W1; ...` 别名段，
   fwd 引用的标识符变成未定义 → 解析失败。已补回。
2. **R2026a Parameter 端口固化缺陷（新发现）**：新建块+单遍写入路径下，
   `Scope=Parameter` 的 Data 不会从函数签名中收缩，而是固化为悬空输入端口
   （收 0 而非 workspace 值）→ fuel=85.83 ≠ golden。内存注入路径因图继承自
   成熟接口而侥幸正确，导致该问题仅在产品重建路径暴露。

**最终方案：权重/归一化常数改为显式信号线传入**——CODE_ECMS_NN 签名扩为
24 输入（11 原始信号 + 13 个 ECMS_NN_*），law=7 变体内新增 13 个 nnconst_*
Constant 块（Value='ECMS_NN_W1' 等引用 workspace）接入第 12~24 号口。
彻底绕开 Parameter 持久化机制，天然兼容 HIL/代码生成（显式接口本为 MBD 最佳实践）。其余
6 个变体不动（框架本就容忍变体间接口差异）。

**最终全链验证（full_verify2.m，全绿）**：

| 阶段 | 结果 |
|---|---|
| F1 rebuild（7 变体全部构建+update 成功） | PASS |
| F2 结构核对（20 图；19 图与 HEAD 逐字一致；law=7 匹配 24 输入方案） | PASS |
| F3 10b smoke（T01C/T04C/T10C 全部 true；移至最后执行） | PASS |
| F4 law=7 S01 黄金值 | **fuel=91.2317939047896 逐位 == golden** |
| F5 test_law7 T1/T2/T3 | 本机 .venv_tf Python 缺失，T1 无法运行（环境限制）；其等价性已由 F4 承载；环境重建后可补跑 |

**新发现的遗留问题（Phase 3 必读）**：10b smoke 会污染同进程内后续 law=7 运行
（bisect 实证：rebuild→sim = 91.23 逐位正确；smoke→sim = 85.83 退化）。
full_verify2 已通过调整验证顺序规避；根因待 Phase 3 定位。

### 8.4 交付清单变更

- `build_model_c.m`：CODE_ECMS_NN 内联化（24 输入签名）；buildNNECMSVariant
  新增 13 个 nnconst_* Constant；setMFCode 增加 twoPass 可选参数（默认 false，
  仅历史调试用）；主流程注释同步。
- `.slx`：内联 law=7 重建产物（md5 dd7fe119…，替代 HEAD ada8a732…）。
- `test_law7.m`：venv python 路径跨平台探测（Windows Scripts / Unix bin）。

### 8.5 Mac 端补跑（2026-08-26，handoff_mac_issues.md 执行）

Windows 端因缺 .venv_tf 未能运行的 T1 已在 Mac 端补跑完成，**T1/T2/T3 全绿**：

| 项 | 结果 |
|---|---|
| T1 nn_predict_ehat vs Python | max\|Δ\| = 1.22125e-14 < 1e-5（与 Windows 证据 511f28ae 逐位一致） |
| T2 回退路径（mat 缺失 → ê=0） | 块 ≡ 离线 law=6-ehat=0，max\|Δ\| = 0 kW（容差 0） |
| T3 law=7 S01 全闭环 | fuel=91.2317939 kg（逐位 == golden），SOC_end=0.6145，Vdc_min=683.3 V |

**新发现（内联版对 T2 的机制适配）**：内联版（§8.3）的权重经 nnconst_* Constant
从 base workspace 的 `ECMS_NN_*` 显式信号传入，回退门控为 `ECMS_NN_WeightsOK`。
原 T2 仅 rename mat、依赖外置版函数惰性 load 的语义——内联版下 rename mat 不会撤回
已注入 base 的权重，导致 T2 期间 NN 仍活跃（实测 P_gen_ref 饱和 1000 kW、max\|Δ\|=220 kW，
回退路径根本没被测到）。修复：T2 在 rename mat 后同步调用 `ecms2.ensure_nn_params(ROOT)`
（此时 mat 缺失 → WeightsOK=0，回退真正生效）。

**SMOKE-STAGE POISON 未在 Mac 复现**：本次未按 smoke→law7 顺序执行（遵循 §8.3
规避顺序：功能验证在前），未触发退化；POISON 根因仍待 Phase 3 定位。
