# Dataset v3 未来信息契约（future_information_contract.md）

> Spec: `.specs/phase4-d-dataset-v3`（T-431 / T-004；FR-462~468、FR-500；design DGN-02/03/04/13）
> **事实源声明（FR-465）**：信号 role/枚举的机器权威为
> `config/dataset_v3_signal_registry.json`；C 侧必须输出的字段与类型的机器权威为
> `config/dataset_v3_producer_schema.json`。本文件是解释层；冲突时以机器契约为准。

## 1. 未来信息三分类（K/F/U，D-D3）

对 anchor 时刻 t₀ 的未来信息按认识论（epistemic）地位三分类——分类的对象是
**信息来源**，不是信号的时间位置：

| 类 | 定义 | 成员（registry role） | 部署时对应物 |
|---|---|---|---|
| **K**（Known，计划/海图确定） | 由计划、海图、航线几何等**先验文档**确定，t₀ 时刻即精确已知 | `INPUT_K`：v_plan_mps（planned along-track SOG，**非真实未来航速**）、s_plan_m、h_chart_m、P_hotel_plan_kW（仅 planned component，实际 residual 属 U） | 航线计划/配载计划/海图 |
| **F**（Forecast，预报） | t₀ 时刻由预报过程给出的估计，带 vintage（发布时刻）与误差 | `INPUT_F`：u_fcst_mps、wl_fcst_m | 水文气象预报服务 |
| **U**（Unknown，predictor-forbidden） | 未来实际量/突发量；数据集可物理存储但角色隔离 | TARGET（未来实际 P_load）、ORACLE（u_actual_future_mps、wl_actual_future_m 等真值对应物）、FORBIDDEN/METADATA（scenario latent） | 无（部署时不可得） |

硬规则（FR-468/D-D4）：**严禁用 s_actual,future / 任何未来真值构造 future feature**；
位置相关约束使用 interval-aware sampling（s_plan 沿计划轨迹采样）。

## 2. Issue-time causality（FR-463）

一切未来向特征满足 **issue-time 因果**：

```text
x(t₀+k | t₀)  ≜  t₀ 时刻可获得的、关于 t₀+k 的信息
```

- K 类：计划文档先验存在 ⇒ x(t₀+k|t₀) = 计划值（因果合法：计划本身是 ℐ_{≤t₀} 的成员）；
- F 类：预报生成过程必须满足 **F_{t₀} = g(ℐ_{≤t₀})**（FR-467）——只允许消费 t₀ 及以前
  的信息；把未来真值后处理定义成"预报"是**造假**（ORACLE_DERIVED provenance class 永拒收）；
- 因果性**不可从数值判断**，靠 provenance 字段机器校验（§4）+ 负向测试（future-truth
  冒充 INPUT_F 必须被检出，AC-427）。

## 3. 双索引存储：issue-time snapshot vs realization-time truth（FR-466）

两类数据在 schema 级区分（DGN-04），禁止从最终 future series 反推历史 vintage：

| 表 | 索引 | 内容 | 角色物理隔离 |
|---|---|---|---|
| `stream`（realization-time） | 单时间轴 `time_s`（native 0.01 s） | 全部 truth/H1/H2/ORACLE/METADATA 列 | INPUT_F 的真值对应物存此处（ORACLE role） |
| `kf_snapshots`（issue-time，long-table） | `(issue_time_s, lead_s)` 双索引，lead ∈ {10,…,300} | INPUT_K/INPUT_F 列 | 与 stream 物理分表 |

K/F 网格 = target 网格：`F_k = (t₀+(k−1)·10, t₀+k·10]`，k=1..30（FR-471）。
窗口化时严格取 `issue_time == t₀`（容差 1e-9）的快照行；缺失 ⇒ 该窗口 invalid 并计数
（DGN-07，阈值 0 ⇒ pipeline FAIL）。

## 4. Forecast provenance 六字段（design R6）

每航次的 INPUT_F 生成过程必须携带机器可验证 provenance（producer schema
`forecast_provenance` 能力族）：

| 字段 | 含义 |
|---|---|
| `forecast_generator_id` | 生成器标识 |
| `forecast_generator_version` | 生成器版本 |
| `forecast_generator_hash` | 生成器内容 SHA256 |
| `issue_time_s` | 预报发布时刻 |
| `source_cutoff_time_s` | 生成器可消费信息的截止时刻 |
| `forecast_provenance_class` | `CAUSAL_GENERATOR` / `FIXTURE_CAUSAL_STUB` / `ORACLE_DERIVED`（永拒收） |

机器不变量：**`source_cutoff_time_s ≤ issue_time_s`**（validator 强制）。
class 白名单：formal 仅 `CAUSAL_GENERATOR`；pilot 另允许 `FIXTURE_CAUSAL_STUB`；
`ORACLE_DERIVED` 一律拒收（FR-467：禁止未来真值后处理冒充预报）。

## 5. Pilot causal fixtures（DGN-13，全部 `PILOT_ONLY`）

现有 S/M 场景无 Mission v3，pilot 期 K/F 由 producer 内 causal stub 生成，全部行打
`fixture_kind="PILOT_ONLY"`（validator 拒绝含该标记的 voyage 进入任何 formal 产物）：

| fixture | 构造 | 因果性依据 |
|---|---|---|
| `v_plan/s_plan`（planned along-track SOG/位置） | 场景速度计划表（先验计划）自 t₀ 前向积分；**禁无条件复用 realized v_ref**（design R5） | 场景定义本身是 ℐ_{≤t₀} 的先验计划 |
| `h_chart_m` | 场景水深设定 | 海图已知量，K 类本就精确 |
| 桥区/限速区 | 静态空集（pilot 场景无桥区，fixture 语义如实记录） | 静态约束 |
| `u_fcst_mps = U_actual(t₀)` | persistence forecast，仅消费 t₀ 及以前 | F_{t₀}=g(ℐ_{≤t₀}) |
| `wl_fcst_m` 同理 | persistence | 同上 |

**严禁**用未来真值后处理构造 F（代码审查点 + 负向测试：故意注入 future-truth 构造的 F
必须被检出）。pilot fixture 永不入 formal fleet（FR-500）。

## 6. Predictor-access 限制

- `INPUT_K`/`INPUT_F` → predictor_access=default（默认特征，未来窗输入）；
- `TARGET`/`FORBIDDEN` → forbidden（target-as-feature 泄漏封锁）；
- `ORACLE` → oracle_only（仅显式 Oracle 实验可用，FR-464）；
- view 白名单由 manifest 按 registry role 构造，越权列进入 view 必须被拒（负向测试）。
