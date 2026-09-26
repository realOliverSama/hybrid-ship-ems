# S2_PROVENANCE — EMS04-S2（FEATURE_PANEL_PARITY）封存溯源

> 封存日期：2026-09-26 · 裁决：用户 S2 GATE VERDICT（FEATURE_PANEL_PARITY = PASS，接受并关闭）
> 目的：S2 实现与溯源在进入 S3 前固化；**S3 变更不得破坏 S2 的独立可复现性**。
> 完整哈希索引（1572 条）：`experiments/softhil_ems03_2026-09-25/ems04_s2/PROVENANCE_SHA256.txt`
> （生成器 `…/ems04_s2/make_provenance.sh`，可重建；experiments/ 不入库，清单即不可变索引）

## 身份（Identity）

| 项 | 身份 |
|---|---|
| 契约 | `FEATURE_PANEL_CONTRACT.md`，FPC-1.0，sha256 `d263fc92e90073f1…0eb69e` |
| 线格式 | `b/PROTOCOL_FP.md`，sha256 `18efb703720ce7d2…e1bba3` |
| 航次 | `FLTA_S2_D01009_M1`（GOOD_V1，377 锚点）、`FLTA_S1_D00000_M2`（DEGRADED_V1，378 锚点） |
| 固件形态 | `EMS04_FP_PARITY`（CMake cache，Release + `STM32N647XX_LRUN_RAMxspi1.ld`） |
| 固件 ELF | `Appli/build/FpParity/EMS01_CHIL_ECMS_Appli.elf` sha256 `827978188338576373d4d8057e672fce40b9273201bcc4218ee91f286b838c45`（单 PT_LOAD⊆白名单；RAM 65.8%） |
| 板级捕获 | `…/ems04_s2/c/out/board_capture.bin`（10,657,586 B）sha256 `b89e57960fc42ead7e857b675b80c932504201948f6c908c39a69b458fbae2a0` |
| host 参考 trace | `…/ems04_s2/b/out_b2/trace_B2_O2.bin` sha256 **同上（逐字节全同）** |
| 真值 manifest | `…/ems04_s2/truth/truth_manifest.json` sha256 `733cb3b4f18add1c…bcb2c9f` |
| kf 表（route A） | D01009 `9de37a57b3c182d4…54a4a5e6`（274,672 B）· M2 `273891d7689558d8…fd8631d`（275,400 B） |
| 真值索引 | D01009 `T0_INDEX.json` sha256 `f2bf140269ac362c…3c5e33` · M2 `30ca391215bcaf23…1d47098` |

## 编译口径（Compiler contract）

- host 对拍台：`cc -std=c99 -O2 -ffp-contract=off -Wall -Wextra`（另有 `-O0` 与 ASan/UBSan 构建；三构建 trace 逐位一致，见 `c/logs/`）。
- 固件：CMake Release + `-DEMS04_FP_PARITY=1`（钉死 LRUN_RamRel 链接），日志 `…/ems04_s2/c/logs/fp_build.log`；RAM 用量 1,379,528 B / 2,096,128 B。

## S2 实现清单（本工程内，随 git 提交）

| 角色 | 文件 |
|---|---|
| FP 服务五模块 | `Appli/App/ems04_sha256.{c,h}` / `ems04_fp_proto.{c,h}` / `ems04_h1win.{c,h}` / `ems04_kftable.{c,h}` / `ems04_fp_svc.{c,h}`（与 B 阶段冻结源逐字节一致） |
| UART 胶水 | `Appli/App/ems04_fp_board.{c,h}` |
| 集成点 | `Appli/App/ems_app.c`（USER 区 0x20..0x26 路由，`#ifdef EMS04_FP_PARITY`）、`Appli/CMakeLists.txt`（`EMS04_FP_PARITY` cache 开关） |
| host 工具 | `Host/tools/fp_replay_client.py`（UART 回放客户端，字节透传 + 期望级流控） |

## 仓库外档案（gitignored，按哈希索引）

- 契约/协议/工具/真值/报告全部在 `experiments/softhil_ems03_2026-09-25/ems04_s2/`（A/B/C 三阶段 + `truth/` 真值包 755 锚点 + `PROVENANCE_SHA256.txt`）。
- 大捕获（board capture / host trace / 真值 bin）不入 git；身份由上述 SHA256 清单固定。

## 复现（S3 之后仍须成立）

```text
A（host 真值）:  .venv_tf/bin/python a/tools/gen_canonical_truth.py --selfcheck
                 .venv_tf/bin/python a/tools/pack_kf_table.py --selfcheck
B（host 对拍）:  cd b && ./build.sh            # 1332/1332 × O0/O2/ASan
                 （稠密 755：见 b/build_b2.sh / B_PARITY_REPORT.md）
C（板级）:       debug-stm32n647-sram 规程加载 FpParity ELF（确定性 reset）
                 Host/tools/fp_replay_client.py replay --scripts …（两脚本束）--capture …
                 c/tools/verify_capture.py --capture … --truth ../truth --scripts … --out C_BOARD_PARITY
清单重建:        bash make_provenance.sh
```

## 判定（S2 = FEATURE_PANEL_PARITY，PASS）

755/755 锚点 hist+kf f32 原始字节逐位 == canonical 1 Hz 训练面真值；全语料回归 576 航次/197,448 窗/651,578,400 值 0 失配；板 capture ≡ host trace 全哈希相等；0 NACK / 0 CRC 错；负路径全 fail-closed；NOR/Flash/option bytes 零触碰。

**注**：以上仅覆盖特征面板。预测器输出（S3 = PREDICTOR_OUTPUT_PARITY）与整机控制输出（S4 = FULL_CONTROLLER_OUTPUT_PARITY）另行验收。除结果性实现变更外，不重跑 S2。
