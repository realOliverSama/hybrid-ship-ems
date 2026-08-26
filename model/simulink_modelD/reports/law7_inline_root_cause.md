# law=7 内联 MLP 前向 Stateflow 解析失败——根因报告（已结案）

- **日期**: 2026-08-26
- **分支**: debug/MATLAB-debug（基于 feat/adaptive-ecms-methanol @ 4baac9c1）
- **环境**: MATLAB R2026a Update 3（26.1.0.3276743）/ HybridShip_ModelD_DCBus / md5 ada8a732ba2e8ad47e904d5c76880efc
- **状态**: **ROOT CAUSE CONFIRMED + FIX VERIFIED（编译级）**

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
