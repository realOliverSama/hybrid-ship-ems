# EML 行号 → C 行号语义映射表（T-503 交付物）

Spec: `ems01-chil-ecms-v1`（T-503，FR-503/506/507）
语义基准: `model/simulink_modelD_(EMS_NN_AECMS)/results/ecms_phase25/gating/eml_ecms_step_mode2.m`
（函数 `ecms_nn`，226 行，下称 **EML**；行号为该文件的 1 基行号）

C 侧文件（`Appli/App/`，行号以当前提交版本为准）：

| 缩写 | 文件 |
|---|---|
| ctrl.h / ctrl.c | `ems_ctrl.h` / `ems_ctrl.c` |
| inf.h / ref.c | `ems_infer.h` / `ems_infer_ref.c` |
| prm.h / prm.c | `ems_params.h` / `ems_params.c`（生成物，禁手改） |
| cfg.h | `ems_config.h` |

比对方式：L0 用例（`Host/tests/test_ctrl_diff.py`）以 EML 逐条转写的纯 Python 镜像
做 double 逐位比对（含 60 拍端到端轨迹）；主 Agent 可按本表抽查任意行。

## 1. 常数与权重（EML:1-25）

| EML | C 位置 | 语义 |
|---|---|---|
| 1 | ctrl.h:45-51（`ems_ctrl_in_t`）+ prm.c（`EMS_PLANT/EMS_KS`） | 动态信号 4 个（t/P_load/v_ship/SOC）+ nn_ok；其余入参为生成物常数注入 |
| 2 | cfg.h:17-21（`EMS_GATE_MODE` 默认 2） | GATE_MODE=2；测试可用 `-DEMS_GATE_MODE=n` 覆盖 G0~G3 |
| 2 | prm.c `EMS_GATE`（s2/s1/m_ticks/upstream 字段，源 `selected_model_weights_v2.json:gate`，与 EML 字面量同源核对） | GATE_M=3、GATE_S1=28.947700、GATE_S2=40.771000 |
| 3 | prm.c `EMS_GATE.upstream_class`（=6） | upstream 可辨识受限类 id |
| 4-25 | prm.c `EMS_CLF_*`/`EMS_MLPQ_*`（生成自 deploy JSON，float32 全精度 double） | 识别器/融合/MLP-Q 权重字面量 |
| 1（`bsfc_bp/bsfc_tbl`） | prm.c `EMS_BSFC_BP/EMS_BSFC_TBL`（501 点，`ecms_bsfc_override.mat`） | BSFC override 表（非 7 点默认表） |
| 1（`XMean/XStd/YMean/YStd`） | prm.c `EMS_NORM_X_MEAN/X_STD/Y_MEAN/Y_STD`（`fleet_norm_v2.json`） | 归一化常数 |
| 1（`lambda…Pgen_init` 等） | prm.c `EMS_KS`（kS_calibration.json）+ `EMS_PLANT`（Model_Parameters.m 解析，生成器与 design 表核对） | λ₀/K_soc/K_S/Λ_floor/SOC_ref/Ē_H/E_scale；η/效率/SOC 限/Idle_Fuel/dP/P_max/Pgen_init/dt |

## 2. 状态与复位（EML:26-43）

| EML | C 位置 | 语义 |
|---|---|---|
| 26 | ctrl.h:33-41（`ems_ctrl_t` 字段一一对应） | persistent lastT/Pheld/buf/k/vprev/cls_eff/run_cls/run_cnt |
| 27-35 | ctrl.c:201-214（`ems_ctrl_step` 首调块；`ems_ctrl_init` ctrl.c:57-60 置 `initialized=0` 触发） | lastT=-1e9、Pheld=Pgen_init、k=1（C 0 基 k=0）、**buf 平 fill 当前 P_load**、vprev、门控态 -1/-1/0 |
| 36-43 | ctrl.c:216-226 | t<lastT 新仿真全复位：lastT=t、k=1、平 fill、vprev、门控态复位；**复位拍因 lastT=t 落入 EML:47 tick 门 → 返回 Pheld**（顺序保持） |

## 3. 拍内主流程（EML:44-62）

| EML | C 位置 | 语义 |
|---|---|---|
| 46-49 | ctrl.c:229-235（`P=Pheld` + tick 门 `t-lastT < dt-1e-9` 早退） | 输出保持 |
| 50 | ctrl.c:237 | lastT=t |
| 51-52 | ctrl.c:239-242 | isfinite(P_load) 才写 buf[k]（**NaN 跳写**） |
| 54 | ctrl.c:243 | k=mod(k,300)+1（**指针无条件前进**；C 0 基等价） |
| 55-56 | ctrl.c:245-248 | accel=v_ship-vprev；vprev=v_ship（在 ok 判定之外，NaN 也推进） |
| 57 | ctrl.c:249-251 | ok = nn_ok>0.5（C：`!=0`）&& isfinite(P_load,v_ship,accel) |
| 60 | ctrl.c:253-263 | xP=[buf(k:300);buf(1:k-1)] 升序环回展开（k 已前进，与 EML 一致） |
| 61 | ctrl.c:265-268 | x=[xP; v_ship; accel]（302 维） |
| 62 | ctrl.c:269-274 | all(isfinite(x)) |

## 4. 推理与门控（EML:63-169）

| EML | C 位置 | 语义 |
|---|---|---|
| 64 | ctrl.c:292-296 | xnorm=(x-XMean)./XStd（归一化只在 ctrl，design R8） |
| 65 | ref.c:47-49（`dense_unit` ref.c:33-42） | h1=max(W1.'*xnorm+b1,0)：升序累加→+b→ReLU（fmax 与 MATLAB max(NaN,0)=0 同语义） |
| 66 | ref.c:50-52 | h2=max(W2.'*h1+b2,0) |
| 67 | ref.c:53-55 | y_norm=W3.'*h2+b3（输出层无 ReLU） |
| 68 | ctrl.c:313-317 | Ehat_q=q_norm*YStd+YMean（q10/q50/q90 反归一化 [kWh]） |
| 69 | ctrl.c:314-316（`e_hat_raw`） | ê=(Ehat_q(2)-Ē_H)/E_scale（q50 通道） |
| 71-79 | ctrl.c:318-323 + `ems_ctrl_gate_kq` ctrl.c:89-100 | K_q：s_q≤S1→1；s_q≥S2→0；线性 |
| 82 | ctrl.c:325-329 | Pdec=xP(1:10:300)（C：`xP[10*j]`，j=0..29） |
| 83-95 | ref.c:82-94 | conv1：causal 核 5、左补 4（`j=t+kk-4`，`j>=0`）；**acc 从偏置起**（EML:86 运算树） |
| 96-99 | ref.c:96-101 | pool1：max pool 2/2，30→15 |
| 100-112 | ref.c:103-119 | conv2：causal 核 3、左补 2；通道点积（EML:107 `p1(jj,:)*C2W(kk,:,jf)`）先求和再加 acc |
| 113-116 | ref.c:121-126 | pool2：15→7，用行 0..13，**丢尾（第 15 行弃用）** |
| 117-129 | ref.c:128-144 | conv3：causal 核 3、32→32 |
| 130 | ref.c:146-153 | GAP：时间维均值（行序累加后 /7） |
| 131 | ref.c:155-160 | s8=max(F1W.'*[v;a]+F1B,0)（旁路融合，2 项升序点积） |
| 132 | ref.c:162-171 | hcl=max(F2W.'*[gfeat;s8]+F2B,0)（40→64） |
| 133 | ref.c:173-176 | ocl=F3W.'*hcl+F3B（64→7，线性） |
| 134 | ref.c:181-191（argmax 平局取首）；对照 `ems_ctrl_argmax_first` ctrl.c:187-196 | [~,cls]=max(ocl)：严格大于才替换 → 平局取首个；cls 1..7 |
| 136-148 | ctrl.c:62-81（`ems_ctrl_hysteresis_step`）；调用点 ctrl.c:339-341 | 迟滞：首判采纳/同类复位计数/连续 3 tick 切换/新候选计数=1 |
| 149-153 | ctrl.c:84-87（`ems_ctrl_gate_kw`）；调用点 ctrl.c:343 | cls_eff==6 → K_w=0，否则 1 |
| （design 扩展） | ctrl.c:331-342 | 识别输出越界 [1,7] → K_w=1、跳过迟滞更新、`cls_invalid=1`（EML 无此分支；design Error Handling） |
| 155-164 | ctrl.c:102-115（`ems_ctrl_gate_combine`）；调用点 ctrl.c:346-347 | K：G0=1/G1=K_q/G2=K_w/G3=K_q·K_w |
| 165 | ctrl.c:347（`e_hat = k * e_hat_raw`） | ê 门控 |
| 166-169 | ctrl.c:363-369 | !ok → ê=0（≡law=5）；门控状态本拍不动 |

## 5. λ 自适应与网格穷举（EML:170-224）

| EML | C 位置 | 语义 |
|---|---|---|
| 171 | ctrl.c:373 | soc_ref_eff=SOC_ref+K_S·ê |
| 172 | ctrl.c:374 | λ=λ₀+K_soc·(soc_ref_eff−SOC) |
| 173-175 | ctrl.c:375-378 | K_S≠0 时 λ=max(λ,Λ_floor)（`fmax` 保持 MATLAB max(NaN,·)=· 语义） |
| 176 | ctrl.c:138（`ems_ctrl_grid_search`） | N=floor(P_max/dP)+1=51 |
| 177-178 | ctrl.c:139-140 | Pbest=0；Jbest=inf |
| 179 | ctrl.c:141 | nb=numel(bsfc_bp)=501 |
| 180-181 | ctrl.c:143-144 | 网格 Pg=(i-1)·dP（C 0 基 `i*dP` 等价） |
| 182 | ctrl.c:145 | Pbat=P_load−η_rect·Pg |
| 183-185 | ctrl.c:147-150 | SOC≤Discharge_Max 且 Pbat>1e-9 → continue |
| 187-188 | ctrl.c:151-153 | **首个可行 Pg 赋 Pbest 并 break（非 J 最优）** |
| 190-191 | ctrl.c:154-156 | SOC≥Charge_Min 且 Pbat<0 → continue（**禁充电**） |
| 193-194 | ctrl.c:158-161 | Pg<1e-9 → mf=Idle_Fuel/3600（[kg/h]→[kg/s]） |
| 196-198 | ctrl.c:120-122（`ems_ctrl_bsfc_at`） | Pg≥bp(nb) → tbl(nb)（端点钳位上） |
| 199 | ctrl.c:123 | Pg≤bp(1) → tbl(1)（端点钳位下，保持 tbl(1)） |
| 200-203 | ctrl.c:124-127 | `while ii<nb-1 && bp(ii+1)<Pg`（**严格小于**；C 0 基 `ii<nb-2`） |
| 204-205 | ctrl.c:128-129 | w 与 lerp（运算树保真：节点处亦走 lerp，舍入行为与"节点直查"可区分，L0 有专门用例） |
| 207 | ctrl.c:163 | mf=Pg·bsfc/3.6e6（[kg/s]） |
| 209-210 | ctrl.c:167-168 | 放电 mbe=Pbat/(η_dcdc·η_dis)/3600 |
| 211-212 | ctrl.c:169-171 | 充电 mbe=Pbat·(η_dcdc·η_chg)/3600（负值） |
| 214 | ctrl.c:173 | J=mf+λ·mbe |
| 215-218 | ctrl.c:174-177 | **J<Jbest 严格小于 → argmin 平局取首** |
| 220-222 | ctrl.c:179-183 | **末尾救急**：SOC≤Discharge_Max 且 Pbest<1e-9 且 P_load>η_rect·P_max → P_best=P_max |
| 223-224 | ctrl.c:184 / ctrl.c:397-399 | P=Pbest；Pheld=Pbest |

## 6. 附属说明

- `ems_infer_mlpq/ems_infer_workclf` 的接口契约（归一化前/后、返回 0/非 0、us 计时）
  见 inf.h:20-34；REF/NPU 后端同签名（design R8：预处理只在 ctrl 一份）。
- C 侧求和顺序即 REF 位级基准（升序累加；Dense 先累加后加偏置、Conv 先加偏置后
  累加——与 EML 各行运算树一致）。MATLAB/BLAS 的矩阵乘求和顺序可能不同，末位
  舍入差异由 L0 门禁（≥99% 同网格点、≤20 kW）覆盖，不属于移植歧义。
- 数值验证：`Host/tests/test_ctrl_diff.py` 22 用例全过（含 60 拍端到端逐位镜像、
  本表各风险点专项用例、`-Wall -Wextra` 零警告、生成器幂等、§20.2 扫描）。
