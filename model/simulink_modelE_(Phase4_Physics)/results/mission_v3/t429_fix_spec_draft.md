# T-429 修复规格草案（P5 诊断段产物；DRAFT，待主 Agent 批准后施工）

- schema: mission_v3_t429_fix_spec_draft_v1
- generated_at: 2026-09-29
- 生成方式: 纯只读诊断（无 MATLAB 执行、无板、无 git 写操作）；除本文件外零修改
- 证据等级: 本文件是**施工规格草案**，不是 Gate 证据；其中"机械验证"一节是对既有冻结语料
  artifacts 的只读复算（Python/venv_tf，块级近似），用于支撑缺陷机制判断，
  **DEVELOPMENT_DIAGNOSTIC_NOT_GATE_EVIDENCE**（与 d1a_dir_paired_diagnostic 同级）
- 修复授权基础: 用户 P5 指令认定 DIR / DEPTH 两项为"实现缺陷而非科学结论"，
  授权修复生成器/审计对比实现；红线：禁改语料条目、判据阈值、registry、冻结件凑 PASS

---

## 0. 结论摘要

| 项 | verdict 原表述 | 诊断结论 | 修复 |
|---|---|---|---|
| DIR_UPSTREAM_VS_DOWNSTREAM | INVALID/UNRESOLVED due matched-segment implementation defect | **证实**。审计器未实现冻结 registry 自身的 `nuisance_controls: "same regime x same reach"` 与 `paired_effect_profile.aggregation: "group-level matched pure slices first"`：组级 delta 把 HARBOR/SHOAL 慢速段切片混入上游均值，段构成混杂反转符号（gate 实测 k=14/60 正） | DIR cohort key 增补 `reach (same)`（约 1 行）；无需改语料/registry/阈值 |
| DEPTH_LOW_ID_VS_NORMAL_ID | INSUFFICIENT EVIDENCE under frozen grouped-CV rule（"组数 12/12 不足"） | **部分不一致，如实更正**：组覆盖门（≥10/类）实际 PASS（C3: 12+12 已验证）；真正根因是 probe 折分配实现**类盲**（按切片数贪心平衡、不看类别），DEPTH 24 个近等大小组被分出缺类的测试折 → INSUFFICIENT。12/12 组在类分层折分配下对 grouped 5-fold **数学上充分**（设计 DGN-12 本身写明"每折约 ≥2 独立组/类"且 ≥5 组/类为数学必要） | probe 折分配改为确定性类分层贪心（约 15 行）；**不能也不需要增补语料** |

预期重跑后：DIR 变为可判定（paired effect 大概率转正，probe 不确定）；DEPTH 变为 VALID FAIL
（SBD 0.5461 « 1.5 与 probe≈chance 均为真实科学负结果）。overall 仍 FAIL（其余 4+1 项
VALID FAIL 是冻结语料上的真实发现），T-429 按 P5 预案归档 NO-GO 边界，但六项身份全部
clean（0 项 INVALID/UNRESOLVED、0 项 INSUFFICIENT）。

---

## 1. DIR matched-segment 缺陷（D1）

### 1.1 精确定位

全部位于 `scripts/+mission_v3/+audits/identifiability.m`：

1. **直接缺陷** — `local_paired_effect_eval`，L2341-2351：

```matlab
2341  in_a = (ga == g) & (ca == c);
2342  in_b = (gb == g) & (cb == c);
...
2347  m_up = mean(Pmean(idxA(in_a)));     % 该组 cohort cell 内【全部】upstream 切片的均值
2348  m_dn = mean(Pmean(idxB(in_b)));     % 该组 cohort cell 内【全部】downstream 切片的均值
2349  delta_g(end+1, 1) = m_up - m_dn;
```

   组内 A/B 切片未做任何段级（reach/role）匹配，"matched" 只发生在 family（base_draw）
   粒度。冻结的 `paired_effect_profile.aggregation = "group-level matched pure slices
   first, then signed paired delta in mean P_load"`（registry 原文）中的 **matched** 一环
   未实现。切片池本身已是 SEGMENT_PURE_DIAGNOSTIC_SLICE（纯段窗口），缺的是匹配键。

2. **上游根因** — `local_contrast_specs`，L1363-1366，DIR 行 cohort 镜像缺 `reach`：

```matlab
1363  specs(end+1) = local_spec_row('DIR_UPSTREAM_VS_DOWNSTREAM', ...
1364      'upstream', 'downstream', 'direction', [], { ...
1365      local_pred('template', 'same'), local_pred('regime', 'same')}, ...
1366      true);
```

   而冻结 registry（`config/mission_v3/identifiability_contrast_registry.json`）DIR 行
   自身声明 `nuisance_controls: "same regime x same reach"`；design DGN-12 对比表同样写
   "DIR 方向轴混杂控制 = 同 regime × 同 reach"。`local_crosscheck_registry_rows` 只校验
   class 值/gate role/路由字段/pairing 旗标，不校验 cohort 字段镜像，故该遗漏未被 C 类
   结构检查捕获。`local_field_of`（L1770-1790）已支持 `'reach'`，cohort 机制可直接消费。

### 1.2 缺陷机制（为什么产生 INVALID）

方向映射（`scripts/+mission_v3/dag_sample.m` `local_direction`，L492-507）：
`return → downstream`；`berth/standby → none`；**其余全部角色 → upstream**。

因此实现口径下：
- 上游均值 `m_up` = departure(HARBOR, 2.5 m/s) + harbor(HARBOR, 2.5 m/s) +
  cruise/bridge/patrol 或 rush/mission（OPEN_WATER/BRIDGE）+ work(SHOAL, 1.2 m/s,
  近 hotel 载荷) 等全部出程段切片；
- 下游均值 `m_dn` = return(OPEN_WATER, 5.5 m/s) 单一段切片。

`m_up − m_dn` 混入了**段角色/航段构成差**（慢速近 hotel 段拉低上游均值 vs 快速长返程段），
方向效应（同 SOG 逆流 vs 顺流的功率差）被构成差淹没并反转。gate 实测
（identifiability_gate_result.json DIR 行）：`delta_point = −94.32 kW`，
`sign_test_k = 14/60`（p=1.0），`delta_min/max = [−261.0, +208.5]` → paired_effect FAIL；
t429_final_verdict.json 亦自述 "sign reversed by segment-mix"。

### 1.3 机械验证（只读复算，非 Gate 证据）

对 T429_CONFIRMATION_V2 语料 60 个 family 的 artifacts（`datasets_v3/mission_v3/t429conf_v2/*.mat`，
manifest sha256 与冻结一致）按速度块近似复算两种口径的组级 paired delta：

| 口径 | 组数 | delta 中位数 | 正/负 |
|---|---|---|---|
| 实现口径（return 前全部块 vs return） | 60 | **−147.3 kW** | 5/55 |
| reach 匹配口径（仅 OPEN_WATER 快速块 vs return） | 60 | **+62.3 kW** | **55/5** |
| role 匹配参考（WORK: cruise vs return） | 26 | +56.8 kW | 24/2 |

与 D1-A 开发诊断（v1 语料：ΔP 中位 +83.8 kW，方向 BA 17/18=0.944，p=7.2e-5，
`d1a_dir_paired_diagnostic.json`）方向一致。块级近似与 120/30/60 切片级数字有差
（实现口径 5/60 vs gate 精确 14/60），但符号结构结论不受影响：**符号反转由段构成混杂造成，
非方向物理不存在**。

### 1.4 修复后应满足的语义

对照冻结文档对"正确匹配"的定义：
1. DIR 切片池（SBD secondary、probe、paired effect 共用）只含同
   `(template, regime, family, reach)` 的切片（registry `nuisance_controls` + DGN-12 表）；
2. paired effect 的组级均值只在匹配 reach（即 OPEN_WATER——见 3.1 覆盖分析）内计算
  （`aggregation: "group-level matched pure slices first"`）；
3. 判据本身零改动：sign（up>dn）、组 bootstrap 95% CI 排零、sign test α=0.05、
   probe BA LCB95 ≥ 0.80、bootstrap 1000 次 seed 42901/42902 全部原样。

---

## 2. DEPTH 对比（D2）

### 2.1 12/12 组由什么决定

- 配额来源是**冻结预注册**，不是生成器可调参数：`t429_amendment_v2.json`
  `fix2_depth_token_safe_layer`（class_A/B 各 12 unique split groups，HT 带宽
  [1.5,2.0]/[2.2,2.6]）+ `confirmation_plan_exact.draw_allocation`
  （`STRAT_WORK_MISSION__DRY draws=24`，sampling_conditions 条件重抽样）。
- 执行环节：`scripts/+mission_v3/producer_confirm.m`（L73-78 配额注释、L155-166
  LOW_ID_BAND pin + Escalate 阈值、L248-259 normal band 读 registry、L323-346 层映射、
  L388-399 band 读取）；24 draws → 24 航次 → 每航次独立 split_group → 12 low + 12 normal。
- manifest 实况（t429conf_v2_manifest.json layer_stats）：depth 24 voyages，
  unique_split_groups_low=12 / normal=12 —— 与配额一致。

### 2.2 INSUFFICIENT 的真实根因（与 verdict 表述的差异，如实报告）

verdict 表述"组数 12/12 不足"不准确：
- 组覆盖门**已通过**：registry `minimum_group_coverage=10`/类，C3 结构检查明确记录
  "LOW groups=12 (HT [1.51,2])；NORMAL groups=12 (HT [2.23,2.55] inside band)"；
- 真正根因在 `local_probe_eval` 折分配，L2047-2059：

```matlab
2047  % ---- grouped 5-fold assignment (deterministic greedy balance) ------
2049  n_g = numel(ug);
2050  cnt_all = accumarray(g, 1);
2051  cnt = cnt_all(ug);
2052  ord = sortrows([cnt, ug], [-1, 2]);        % 仅按切片数降序 + 组 id —— 不看类别
2055  for k = 1:n_g
2056      [~, fi] = min(fold_cnt);               % 仅平衡切片总量 —— 不看类别
```

  类盲贪心在 DEPTH 的 24 个**近等大**小组（每组约 58-59 切片，`699/12`、`704/12`）上
  排序退化为按组号，min-fill 结果取决于组号的类别交错与切片数的微小差异，可以分出
  **缺类测试折**。实测 fold 2 无效（fold_ba[2]=null，reason "a class is absent in a test
  fold"）→ L2103-2105 `continue` → L2111-2118 整体 INSUFFICIENT（fail-closed）。
- design 文档（.specs/phase4-c-mission-v3/design.md DGN-12）对折结构的既定意图是
  "5-fold grouped CV 要求每类 ≥5 组为数学必要；v1 候选冻结为每类 ≥10 unique split
  groups（**每折约 ≥2 独立组/类**）"。现实现不能保证该既定折性质；类分层分配是同一
  冻结规则（"grouped 5-fold CV by split_group_id"）的合格实现，不是改判据。
- 其余 5 个必需对比折均有效纯属该实现的抽签运气（组内双类共存的对比，任一折同时含
  两类切片）；只有 DEPTH 这类"组与类一一对应"的对比会暴露此缺陷。

### 2.3 增补确认样本的合法性结论：**不能**

三重冻结锁定，任何一条都独立否决生成器增补：

1. `t429_freeze_v2.json` 把确认语料 manifest 钉死：`confirmation_corpus_manifest.sha256
   = bcf4e1a7…`、`voyages = 144`；`local_verify_freeze` 逐项机检，manifest 任何条目增删
   → `GATE:MISSION_V3:T429:FREEZE_VIOLATION`，重跑直接无效。
2. `t429_amendment_v2.json` `v2_fail_consequences_frozen.insufficient_coverage =
   "STOP, no third-round top-up"`；且 fix1 规则明文"预注册精确配额（禁边跑边补）"。
3. `t429_final_verdict.json` Day-3 FINAL Ruling："no third T-429 round"。

"新增条目 vs 修改既有条目"的界限也因此无歧义：两者都被禁止——既有条目受
`rerun_equal/payload_hash/artifact_sha256` 逐条 hash 链保护（改即 REPLAY 校验失败），
新增条目动 manifest sha256（改即 FREEZE_VIOLATION）。PREQUAL-AMEND-01 的重生成授权
发生在 T429_FREEZE_v2 **之前**，不构成先例。

### 2.4 合法替代路径（分层/统计功效侧）

即 Fix 2（类分层折分配）：修复后 12/12 组在 grouped 5-fold 下每折 2-3 组/类，
满足 DGN-12 的数学必要条件（≥5 组/类）与既定折性质（每折 ≥2 组/类），**无需任何新语料**。
修复后 DEPTH 必然可判定：
- SBD 侧不含折，`R_lcb95 = 0.5461 < 1.5` 不变 → STANDARD_SHAPE 双判据下 SBD 即判 FAIL；
- probe 侧预期 ≈ chance（现存 4 个有效折 BA 0.432-0.567）。
即 DEPTH 的身份从"INSUFFICIENT（实现伪影）"转为 **VALID FAIL（科学负结果）**——这正是
amendment 预冻结后果 `depth_still_fail_legal_cohort: "accept possible depth-identifiability
limit at current Model-E/H1 fidelity"` 的适用情形。**不需要也不允许**为通过而加样本。

---

## 3. 修复方案（pseudo-diff 级）

唯一允许改动的文件：`scripts/+mission_v3/+audits/identifiability.m`
（+audits 被 FD2 裁决排除在 qualified generation stack 复合 hash 之外：
stack_identity.m L13-14、L160-167 `excluded_dirs = {'tests','+audits'}`，故修复不触碰
冻结生成栈身份；语料、registry、冻结件、阈值、种子全部零改动）。

### 3.1 Fix 1 — DIR reach 匹配（~1 行）

`local_contrast_specs`，DIR 行：

```diff
     specs(end+1) = local_spec_row('DIR_UPSTREAM_VS_DOWNSTREAM', ...
         'upstream', 'downstream', 'direction', [], { ...
-        local_pred('template', 'same'), local_pred('regime', 'same')}, ...
+        local_pred('template', 'same'), local_pred('regime', 'same'), ...
+        local_pred('reach', 'same')}, ...
         true);
```

传导效果（全部经既有机制自动发生，无需改其他函数）：
- cohort key 变为 `template|regime|reach`（+`|FAM<idx>` pairing 前缀）→
  `local_contrast_pool` 的 mixed cells = family × reach；
- SBD cross/within 对、probe 池、`local_paired_effect_eval` 的组内均值输入同步变为
  reach 匹配（`in_a/in_b` 由 cell 过滤自动限定）；
- `pool.applied` 将自动记录 `reach (same)`，报告 cohort_applied 字段如实反映；
- `local_paired_effect_eval` 的 must_link 单 cell 检查（L2316-2330）与 C1 通用检查
  （L2828-2909）不需改动（每组恰好落入唯一 mixed cell，见下）；
- `local_crosscheck_registry_rows` 不受影响（不校验 cohort 字段镜像）。

**覆盖影响分析**（为什么 60/60 组保留、min cov 10 仍满足）：方向映射下有下游侧的 reach
只有 OPEN_WATER（return）；BRIDGE/HARBOR/SHOAL 无下游对应（berth/standby 方向为 none），
其 cell 单类、自动退出 mixed cells。块级复算显示 60/60 个 family 在 OPEN_WATER 两侧均有
切片（出程快速块 ≥1 + return），故 `n_groups_A/B = 60 ≥ 10` 保持；
DIR 的 SBD/probe 池收缩为 OPEN_WATER 切片子集（报告的 n_slices/n_pairs 相应变小，
属预期如实变化）。

**Option 1b（不写入默认方案，单列供主 Agent 裁决）**：reach 匹配下上游 OPEN_WATER 仍混
role（ROUTINE: cruise+patrol；EMERGENCY: rush 6.5/7.0 m/s + mission），EMERGENCY 的
delta 会混入 rush 高速功率（非纯方向效应）。若要求 role 槽位级配对（cruise↔return），
EMERGENCY 无对应槽（出程无 5.5 m/s 段），需子集化或排除该模板 → 改变估计对象、
超出冻结文本（DGN-12 写的是 reach 级）→ 必须主 Agent/用户裁决后方可实施。
**草案默认按冻结文本取 reach 级**；EMERGENCY 的混杂方向是抬高上游（rush 功率远高于
return），对"上游>下游"的必需符号是保守偏置，不构成凑 PASS。

### 3.2 Fix 2 — probe 类分层折分配（~15 行，确定性）

`local_probe_eval`，替换 L2047-2059 的折分配块：

```diff
-    % ---- grouped 5-fold assignment (deterministic greedy balance) ------
+    % ---- grouped 5-fold assignment (deterministic class-stratified
+    % greedy balance; realizes the DGN-12 documented fold property
+    % "each fold >= ~2 independent groups per class"; groups are assigned
+    % whole (grouped CV by split_group_id unchanged); class counts use
+    % per-group slice counts per class, so groups contributing both
+    % classes (shared groups) are balanced on total size as before ------
     ug = unique(g);
     n_g = numel(ug);
     cnt_all = accumarray(g, 1);
     cnt = cnt_all(ug);
+    cntA_all = accumarray(g, double(y == 0));
+    cntB_all = accumarray(g, double(y == 1));
+    is_shared = cntA_all(ug) > 0 & cntB_all(ug) > 0;
     ord = sortrows([cnt, ug], [-1, 2]);
     fmap = containers.Map('KeyType', 'double', 'ValueType', 'double');
     fold_cnt = zeros(1, par.fold_count);
+    fold_pure = zeros(2, par.fold_count);   % [pure-A groups; pure-B groups] per fold
     for k = 1:n_g
-        [~, fi] = min(fold_cnt);
-        fmap(double(ord(k, 2))) = fi;
-        fold_cnt(fi) = fold_cnt(fi) + ord(k, 1);
+        gi = double(ord(k, 2));
+        key = fold_cnt;
+        if is_shared(k)
+            % shared group serves both classes: balance total slices only
+            [~, fi] = min(key);
+        else
+            ci = 1; 
+            if cntB_all(gi) > 0, ci = 2; end      % pure-B vs pure-A
+            key = fold_pure(ci, :) * 1e9 + fold_cnt + (1:par.fold_count) * 1e-6;
+            [~, fi] = min(key);
+        end
+        fmap(gi) = fi;
+        fold_cnt(fi) = fold_cnt(fi) + ord(k, 1);
+        if ~is_shared(k)
+            ci = 1; 
+            if cntB_all(gi) > 0, ci = 2; end
+            fold_pure(ci, fi) = fold_pure(ci, fi) + 1;
+        end
     end
```

性质与影响：
- 确定性（无 RNG），fold_count=5、probe seed 42902（仅 bootstrap 用）、模型/编码/
  scaler 全部不变；
- 类纯组按"该类在折内的组数 → 总切片数 → 折序"最小化分配；DEPTH 12+12 →
  每折 2-3 个纯 A 组 + 2-3 个纯 B 组 → 任一测试折两类俱全 → fold_valid 全真；
- 共存组（组内同时贡献两类切片：全部 SEG/DIR/TRAFFIC 对比）退化为原总量平衡 ——
  这些对比的折划分仅随排序微动，probe 点估计/LCB95 会有小幅数值漂移（须在报告中
  如实呈现新旧对照）；五个 STANDARD_SHAPE 对比的判定由 SBD 主导（R 0.50-0.82 全部
  « 1.5），probe 漂移不改变其 FAIL 结论；
- 建议同步在 `scripts/+mission_v3/tests/` 新增一个折分配性质断言（每折两类俱全/组不跨折）
  ——测试文件新增属施工阶段授权范围，本草案仅建议不改。

### 3.3 结果保全与治理标记（施工时执行，非本诊断段范围）

1. 重跑前保全现行 v2-conformance 证据（EXECFIX-01 产物，被 t429_final_verdict 引用）：
   git 提交既有 `identifiability_gate_result.json/md` 后再重跑，或将副本存为
   `identifiability_gate_result_execfix01_v2conf.json`（命名纪律 identifiability_* 合规）；
2. 本次修复登记为 EXECFIX-02（沿用 EXECFIX-01 先例：commit 8a1ed398 修复 evaluator
   conformance、run01 INVALID 保全），在修复 commit message 与开发进展日志中记录
   "P5 指令授权的审计实现修复轮"，与"无第三轮证据生成"区分。

---

## 4. 不变性论证（为什么不改语料语义/判据/阈值）

| 冻结对象 | 本方案是否触碰 | 依据 |
|---|---|---|
| 语料条目/manifest（144 voyages, sha256 bcf4e1a7…） | 否 | 重跑 `verify_freeze` 逐条 sha 校验必须原样通过；Fix 1/2 均在审计器内 |
| 对比 registry（hash 2274da4a…） | 否 | DIR reach 匹配实现的是 registry 行**自身**的 nuisance_controls 文本；registry 文件零改动，hash 不变 |
| 判据阈值（R_min=1.5, A_min=0.8, α=0.05） | 否 | `local_eval_params` 全部读自 freeze，未触碰 |
| 统计配置（bootstrap 1000、seed 42901/42902、C=1.0、iter cap 1000、几何 120/30/60） | 否 | 同上；折分配算法是冻结规则"grouped 5-fold by split_group_id"内未指明的实现细节，修复使其满足设计文档既定折性质 |
| 生成栈身份（composite 19180c21…） | 否 | +audits 被 FD2 裁决排除在复合 hash 外（stack_identity.m L167） |
| 既有语料条目的重放语义 | 否 | `env_realize/plan_compose` 生成链零改动 |
| SBD∧probe 规则、paired_effect 判据式（sign∧CI∧sign test） | 否 | 判据组合逻辑零改动，仅修正喂入判据的池/折构成 |

结论：本方案不构成 FR-551 意义上的"因失败修改阈值/registry"，不构成 amendment 级
变更；属于 EXECFIX-01 同类的 evaluator conformance repair（把实现修到冻结语义）。

---

## 5. 重跑命令链（D3）

```bash
cd model/simulink_modelE_(Phase4_Physics)
# (0) 施工 + 先跑回归（十套件 + 生成栈零改动快照）：
#     matlab -batch "addpath(fullfile(pwd,'scripts','+mission_v3','tests')); run_all('run')"
#     另跑 C4：scripts/+mission_v3/tests/test_metric_routing.m
# (1) 只做冻结机检（可选前置）：
#     matlab -batch "T429_MODE='verify'; eval(fileread('scripts/81_run_mission_v3_t429.m'))"
# (2) 全量审计重跑：
matlab -batch "eval(fileread('scripts/81_run_mission_v3_t429.m'))"
```

- 输入（由 81 入口 → `mission_v3.audits.identifiability('evaluate')` 消费）：
  `results/mission_v3/t429_freeze_v2.json`（self_hash 复算）→ `t429_amendment_v2.json`
  （self_hash）→ `config/mission_v3/identifiability_contrast_registry.json` +
  `heldout_token_registry.json`（hash 对冻结）→ `results/mission_v3/t429conf_v2_manifest.json`
  （sha256 对冻结；144 artifacts 逐条 sha256；Strict 栈复合重收集对
  `qualification_stack_identity.json`）。
- 输出（任何受控 error 之前先落盘）：`results/mission_v3/identifiability_gate_result.json`
  与 `identifiability_report.md`。
- 判据读取位置：gate JSON 顶层 `overall`（logical）/`verdict`/
  `required_contrasts_pass`/`required_contrasts_total`；逐项 `contrasts[i].status`
  （PASS / METRIC_FAIL / INSUFFICIENT_IDENTIFIABILITY_EVIDENCE）；overall=false 时
  落盘后抛 `error('GATE:MISSION_V3:T429:FAIL')`（设计内受控 FAIL，非零退出）。
- 验收点（机器可读）：`freeze_verification` 全 ok=1；`structural_conformance.all_ok=1`；
  DIR 行 `cohort_applied` 含 `reach (same)` 且 `paired_effect.sign_ok=true`、
  `sign_test_ok=true`、`ci_excludes_zero=true`、`paired_effect_pass=true`；
  DEPTH 行 `probe_status="OK"`（fold_ba 无 NaN）。

---

## 6. 预期判定路径

1. **DIR**：reach 匹配后 paired effect 预期转正（块级近似 55/60 正、中位 +62.3 kW；
   精确切片数字以重跑为准）→ sign_ok ∧ CI 排零 ∧ sign test 大概率全真。probe 在
   匹配池上的 BA_lcb95 能否 ≥ 0.80 **不确定**（D1-A 配对方向 BA 0.944 是组内配对口径，
   冻结 probe 是混合池口径；不能预设 PASS）。→ DIR 身份变为可判定：
   PASS_DIR（两判据全过）或 VALID METRIC_FAIL；不再有 INVALID/UNRESOLVED。
2. **DEPTH**：折修复后 probe 可算（预期 ≈ chance），SBD 0.5461 不变 →
   **VALID METRIC_FAIL**（科学负结果，触发预冻结后果"接受当前保真度下的深度可辨识性
   边界"）。
3. **overall**：其余 4 项 VALID FAIL（SBD R 0.50-0.82 « 1.5；TRAFFIC 纯宿主修复后仍
   FAIL = 真实交通物理发现）在冻结语料上不可改变 → T-429 overall 仍 FAIL
   （预期 verdict 从 FAIL_INSUFFICIENT_EVIDENCE 变为 FAIL_METRIC，即 0 项 INSUFFICIENT）。
4. 按 P5 预案：仍 FAIL → 归档 NO-GO 边界（六项身份全部 clean 的终态），
   T-432 登记维持解锁失败状态，走 C 收口后的 downstream scientific-roadmap amendment。

---

## 7. 风险与 Escalate 点

1. **[Escalate-裁决] 匹配粒度**：reach 级（冻结文本，本草案默认）vs role 槽位级
   （更严格，但 EMERGENCY 无对应槽、需改估计对象）。若主 Agent 选 role 级，
   须先出 amendment 说明并重估覆盖率，本草案的 Fix 1 需相应扩写。
2. **[披露] probe 数值全局微漂**：Fix 2 改变所有对比的折划分，五项 VALID FAIL 的
   probe 点估计/LCB95 会小幅变化（SBD 主导判定，结论不翻转）；报告需并陈新旧值。
3. **[如实报告] verdict 表述更正**："组数 12/12 不足"与证据不符（覆盖门 PASS，
   根因是类盲折分配）；终判/日志中应按本草案 §2.2 更正口径，避免 NO-GO 归档带上
   错误的"样本不足"标签。
4. **[不可修复项] DEPTH SBD 语义**：within-A Q95=1.0 受零范数防御影响（近常数切片
   距离顶格 1.0），压低 R——属冻结 metric 语义（D 侧 audit_leakage.py 同源），
   P5 红线内不动；verdict 已将其登记为 DGN-12 candidate-calibration 未来工作。
5. **[治理] FINAL Ruling 后的重跑授权**：Day-3 裁决"no third T-429 round"；本轮的
   合法性来自 P5 指令对"实现缺陷修复"的显式授权，须由主 Agent 在 EXECFIX-02 登记
   与开发进展日志中固定（§3.3），明确"第三轮"指证据生成轮、不含审计实现修复轮。
6. **[停止条件] Escalate 而非施工**：若重跑后 DIR paired effect 仍 FAIL（reach 匹配
   后符号仍不正），DIR 转为 VALID FAIL 归档，禁止继续调匹配粒度/口径凑 PASS
   （v2_fail_consequences: dir_still_fail = scientific/spec decision）。

---

## 8. 验收清单（施工完成定义）

- [ ] `git status`：仅 `scripts/+mission_v3/+audits/identifiability.m` 一个修改文件
      （+授权的测试/保全副本文件）；
- [ ] `run_all('run')` 十套件 PASS；`test_metric_routing.m` PASS；
- [ ] `T429_MODE='verify'`：FREEZE_VIOLATION 不出现（冻结链全 ok）；
- [ ] 全量重跑落盘 gate JSON/report；`freeze_verification` 与
      `structural_conformance.all_ok=1`；
- [ ] DIR：`cohort_applied` 含 reach (same)；`n_groups_A/B ≥ 10`；
      paired_effect 结果按 §6.1 口径如实呈现（无论正负）；
- [ ] DEPTH：`probe_status="OK"`；整体身份为 VALID METRIC_FAIL（或科学上真实的
      PASS，不得调参追求）；
- [ ] 无语料/registry/冻结件/阈值 diff；无 Model E 基线 diff。
