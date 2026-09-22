# 板会话中断记录（2026-09-03 02:00，无人值守窗口）

## 状态快照（保全现场）

| 项 | 状态 |
|---|---|
| NOR 权重 | ✅ TCN hex @0x70200000 已烧录并独立回读验证（byte-map 165,137B 逐位一致，CRC32=0x47191C22）；workclf @0x70400000、FSBL 零变化（nor_evidence_t811/） |
| SRAM appli | 已不在运行（CubeProgrammer 烧录会话自带 software reset；预期内，下次 SRAM 加载 deterministic_reset 恢复） |
| ST-LINK | ❌ **USB 枚举消失**（system_profiler 无 0483 设备；CubeProgrammer `-l` 亦不可见；CH340 串口 /dev/cu.usbserial-1130 仍在=板供电正常） |
| 诊断 | 烧录后 ST-LINK 掉出 USB 枚举（CubeProgrammer 会话后偶发，需物理重新插拔 USB 或板电源循环恢复） |

## 晨间恢复清单（用户操作 + Agent 续作）

1. **用户**：重新插拔板上 ST-LINK USB（或板电源循环，Development boot 保持）；
2. Agent 续作（全部工件已就绪，无需重烧 NOR）：
   - T-812：SRAM 加载 **P3 NPU 镜像**（`EMS01_CHIL_ECMS_TCN64/Appli/build/Debug/EMS01_CHIL_ECMS_Appli.elf` sha256 前缀 `1dbd86f9`，令牌 `DEBUG_MUTATION_AUTHORIZED_1DBD86F9`）→ INFER_TEST ×200（FR-504 LSB 门 vs tcnq int8 参考）+ workclf us_infer 实测（T-817：IC2 600MHz 预期 ~28ms vs M1 42.5ms）+ INJECT 全轨迹（层②）
   - T-813：闭环 (a) NPU × S20/voyage002 → SRAM 加载 **P3 REF 流式镜像**（sha16 `396c4c8b`，令牌 `DEBUG_MUTATION_AUTHORIZED_396C4C8B`）→ 层① 全轨迹 INJECT（closure ~1.3s/拍）→ 闭环 (b) × 2
   - T-814/815：收尾

## 已锁定的离线证据（不依赖板）

| 层 | 证据 | 结论 |
|---|---|---|
| L0 batch | `EMS01_CHIL_ECMS/Host/results/tcn_swap/l0_tcn_diff.json` | overall=true，全数据集 PASS |
| L0 streaming | `EMS01_CHIL_ECMS_TCN64/Host/results/tcn_swap/l0_tcn_stream_diff.json` | overall=true，**bit-exact 9,965/9,965（max=0）**+ vs-keras ≤4.5e-5 kWh |
| 构建 | 两工程 fw_gate 7/7（P3 含 FW_GATE_ALLOW_IOC=1 授权） | PASS |
| NOR | nor_evidence_t811/ | byte-map 逐位 + FSBL/workclf 零变化 |
| 板 REF ×200（batch 版） | board_ref_infer/ref_keras_gate.json | PASS（裕量 10⁴×） |
| 流式实现 | P3 `Appli/App/ems_infer_tcn.c`（closure，413 行）+ `EMS_TCN_BATCH_REF` 测试基准 | bit-exact 构造性成立 + 实测 |