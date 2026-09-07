# config/mission_v3/ — Phase 4-C Mission v3 生成侧注册表（T-001/T-002）

> **入库策略**: 入库。

## 用途

Phase 4-C Mission v3（真实内河航次与工况生成器 v3）的**生成侧机器可读注册表九件**，
当前为**候选态（CANDIDATE）**：数值冻结经 Phase 1 证据 + T-007 快照生效。
权威链：`.specs/phase4-c-mission-v3/{requirements,design,tasks}.md` →
`experiments/mission_v3/T-001_delegation_spec.md`（生成侧六件，字段级落地裁决）+
`experiments/mission_v3/T-002_delegation_spec.md`（strata / held-out token /
contrast 三件，字段级落地裁决）。

每件 JSON 均含 `schema` / `version` / `status` / `value_basis` / `self_hash`；
self_hash 由 Python（权威维护方）按 canonicalization 字符串计算，并被 MATLAB validator
跨语言复算验证（canonical JSON + SHA256 逐字节一致）。

## 文件清单

| 文件 | schema | 角色 | 对应 design |
|---|---|---|---|
| `mission_template_registry.json` | `mission_v3_template_registry_v1` | 三 mission archetype（ROUTINE_PATROL / EMERGENCY_RESPONSE / WORK_MISSION）+ 段序列 + hotel base + operator prior（DGN-02 HEAVY_CURRENT 正交性裁决落地） | DGN-02（FR-512/553） |
| `conditional_dag_config.json` | `mission_v3_conditional_dag_config_v1` | conditional-DAG 十四节点表（L0~L4 + BASE DRAW）、父子边与分布冻结、河流坐标语义 R1、独立不变量；hash 入 stack identity | DGN-03（FR-513~518） |
| `environment_regime_params.json` | `mission_v3_environment_regime_params_v1` | 三 regime（NORMAL/FLOOD/DRY）流均值/OU δU/水位偏移参数 + OU 注入纪律 + traffic Poisson 表 + 海图水深 | DGN-04（FR-508/515） |
| `id_support_bounds.json` | `mission_v3_id_support_bounds_v1` | 三轴 ID 支撑界/guard band/OOD activation candidate（adverse/following 沿迹相对流 + H/T）+ 联合可行性规则 + 边界 sanity 义务 | DGN-05（FR-519） |
| `forecast_quality_profiles.json` | `mission_v3_forecast_quality_profiles_v1` | GOOD_V1 / DEGRADED_V1 因果预报 profile、prehistory_s=1800（R5 Path A）、observability 白名单/禁用清单、provenance（generator_id=mission_v3_env_forecast） | DGN-06（FR-526~531） |
| `seed_tree_policy.json` | `mission_v3_seed_tree_policy_v1` | 科学 seed 树：SHA256 lineage 身份、Threefry4x64 uint32 引擎映射、八分支清单、干预分支规则、碰撞 fail-closed、可复现不变量 | DGN-07（FR-532） |
| `scenario_stratum_registry.json` | `mission_v3_scenario_stratum_registry_v1` | 九格 scenario stratum（3 template × 3 regime 全 ACTIVE，DGN-02 HEAVY_CURRENT 正交性裁决 → 无 STRUCTURAL_NOT_APPLICABLE 格）；canonical 键 mission_template_id × environment_regime（FR-552/553） | DGN-13（FR-552~553） |
| `heldout_token_registry.json` | `mission_v3_heldout_token_registry_v1` | 三组合 held-out token（C1 bridge×高 ID adverse 流 / C2 harbor×低 ID H/T / C3 rush(emergency sprint)×低 ID H/T）：结构化谓词 + 生成侧 metadata 字段白名单（realized 信号拒收）+ 候选 band（HIGH [1.4,1.8] / LOW [1.5,2.0]）+ EXCLUDE/REQUIRE 双向消费 + ood_config_surface | DGN-11（FR-541~546） |
| `identifiability_contrast_registry.json` | `mission_v3_identifiability_contrast_registry_v1` | T-429 必需对比注册表（R9 清理后 6 REQUIRED + 2 SECONDARY_DIAGNOSTIC）：contrast/轴/class/混杂控制 cohort 谓词/signal_view/SBD+probe/minimum_group_coverage=10（候选）；混轴对比移出必需 Gate 的 R9 注记 | DGN-12（FR-547~551） |

## Validator 与测试

- Validator：`../../scripts/+mission_v3/validate_registries.m`（薄入口调用
  `mission_v3.validate_registries(ROOT)`；检查 JSON 可解析/必备字段/schema 名/枚举/
  结构/交叉引用/支撑界算术/self_hash 跨语言复算；T-002 增补：strata 九格/枚举/ID 模式/
  reason、token C1~C3 谓词白名单（白名单外字段 FAIL）/候选 band 算术/边际覆盖下限/
  ood_config_surface、contrast R9 清单/全字段/SBD+probe/coverage=10/cohort 白名单、
  strata↔template/env registry 与 token band↔id_support_bounds 跨文件闭合；
  FAIL 先写 `../../results/mission_v3/registry_validation.json`
  （overall:false + 逐项错误）再 `error('GATE:MISSION_V3:REGISTRY:*')`）。
- 负向测试：`../../scripts/+mission_v3/tests/test_validate_registries.m`
  （正向 PASS + 八类负向：非法枚举 / 缺字段 / 篡改数值保留旧 hash / 断裂引用 /
  self_hash 篡改 / 谓词白名单外字段 / strata 格数违例 / contrast 覆盖违例；
  临时副本运行，不写 results/；新增三例同时断言目标 issue class）。

## 注意

- 本目录九件为**候选态**：修改任何数值/枚举/父子边/谓词/白名单前必须回到
  `.specs/phase4-c-mission-v3/` 与 T-001/T-002 delegation spec 的 Fixed Decisions；
  修改后必须重算 self_hash（Python 权威）并重跑 validator（两语言一致）。
- scenario_stratum_registry / heldout_token_registry / identifiability_contrast_registry
  三件由 T-002 增补（DGN-11/12/13）：谓词白名单是机器校验核心——token 与 contrast
  cohort 谓词仅可引用生成侧因果 metadata 八字段；band/coverage 数值均为候选，
  minimum_group_coverage 经 T-004 证据确认后由 T-007 冻结。