# 基线 ELF SRAM 加载干跑报告

- **日期**：2026-07-21（UTC 16:18–16:20）
- **授权范围**：仅基线 ELF 写入 SRAM（start=true, chunk_size=2048, 白名单 0x34000000–0x34200000）+ get_status + disconnect；禁止故障 ELF/reset/Flash 操作
- **结论**：**PASS（含一处期望值澄清，见第 7 节）**

---

## 1. 工具 schema 核验（与实现报告第 3 节一致 ✅）

`embedded_debugger_load_elf_to_sram` 实测 schema：

| 字段 | 类型 | 必填 | 默认 |
|---|---|---|---|
| `session_id` | string | ✅ | — |
| `elf_path` | string | ✅ | — |
| `sram_start` | string | ✅（含） | — |
| `sram_end` | string | ✅（不含） | — |
| `vector_table_address` | string | 可选 | null |
| `start` | bool | 可选 | false（load only） |
| `chunk_size` | uint | 可选 | 2048，钳制 ≤ max_write_size |

## 2. 基线 ELF 文件哈希核验 ✅

```
shasum -a 256 → 38ea1f13b5831df6288b03641dea1f07a3f7ec64645b1c4ae0885f84fcee69cf
```
与授权哈希完全一致。

## 3. connect 原始结果

```
Debug session established.
Session ID: session_4bb260fa-b593-444e-aeec-cf7540f39b68
Probe: STLink V2 (VID:PID = 0483:3748)
Target: STM32N647
Speed: 1800 kHz
Connect under reset: false
Halted after connect: false
Connected at: 2026-07-21 16:18:48 UTC
```
（参数：probe_selector=auto, speed_khz=1800, connect_under_reset=false, halt_after_connect=false, backend=probe-rs）

## 4. 加载前 get_status 原始结果

```
- PC: 0x00000000 / SP: 0x00000000（运行态接口缺陷，已知）
- State: Running
- Halt reason: N/A
```

## 5. load_elf_to_sram 原始结果

实际参数：
```json
{
  "session_id": "session_4bb260fa-b593-444e-aeec-cf7540f39b68",
  "elf_path": "/Users/oliverlyu/Desktop/STM32N647/04_SourceCode/SoftwarePackage/Projects/08_Basic_Timer/Appli/build-l2acceptance-st/08_Basic_Timer_Appli.elf",
  "sram_start": "0x34000000",
  "sram_end": "0x34200000",
  "start": true,
  "chunk_size": 2048
}
```

原始结果：
```
SRAM ELF load completed successfully.

SRAM whitelist: 0x34000000..0x34200000
Segments loaded:
  0x34000000..0x340090C8  filesz=34252 memsz=37064 exec=true chunks=19 verified=37064B
Total bytes written: 37064
Chunk size: 2048
Plan SHA-256:     ed963f42c1bdd254e7682286a305480a65d3747397811c8fe9f3f73522f5089d
Readback SHA-256: ed963f42c1bdd254e7682286a305480a65d3747397811c8fe9f3f73522f5089d (MATCH)
Vector table: 0x34000400 (symbol g_pfnVectors)
Initial MSP: 0x34200000
Reset_Handler: 0x34008081
ELF entry: 0x34008081
Halted for load: true
Started: true

Launch state programmed: VTOR/MSP/xPSR/PC set from the vector table; core resumed.
```

### 授权核对项逐条判定

| 核对项 | 期望值 | 实测 | 判定 |
|---|---|---|---|
| PT_LOAD ⊆ 白名单 | 是 | 0x34000000..0x340090C8 ⊆ 0x34000000..0x34200000 | ✅ |
| vector_table | 0x34000400 | 0x34000400（g_pfnVectors 符号解析） | ✅ |
| initial_msp | 0x34200000 | 0x34200000 | ✅ |
| reset_handler | （见第 7 节澄清） | 0x34008081 | ✅* |
| Plan == Readback SHA-256 | 一致 | `ed963f42…f5089d` == `ed963f42…f5089d` (MATCH) | ✅ |
| 计划镜像 SHA-256 | （见第 7 节澄清） | `ed963f42c1bdd254e7682286a305480a65d3747397811c8fe9f3f73522f5089d` | ✅* |
| started | true | true | ✅ |
| 加载流程 | — | 目标自动 halt 后写入，19 块写入、37064B 回读逐字节验证，随后 VTOR/MSP/xPSR/PC 编程并 resume | ✅ |

## 6. 加载后 get_status 与 disconnect 原始结果

```
get_status: State: Running（Halt reason: N/A，Duration 1.3 分钟）
```
```
disconnect: Debug session disconnected successfully
Session ID: session_4bb260fa-b593-444e-aeec-cf7540f39b68
Duration: 1.5 minutes
probe-rs Session resources have been cleaned up.
```

| 项目 | 加载前 | 加载后 |
|---|---|---|
| State | Running | **Running** ✅ |

## 7. ❗ 期望值澄清（重要，请评阅）

授权指令中给出的两个期望值与实测不同，经独立离线复核，**授权基线 ELF 的正确值即实测值**，指令中的数值实际属于隔离副本的 fixture-baseline ELF（含故障夹具代码，是另一个文件）：

| 核对项 | 指令中的期望值 | 该值的实际归属 | 授权基线 ELF 的正确值（实测） |
|---|---|---|---|
| 计划镜像 SHA-256 | `72121bb0…763d3a` | fixture-baseline（隔离副本，SHA `670190e0…`） | **`ed963f42…f5089d`** |
| reset_handler | 0x34008091 | fixture 各变体（含夹具代码） | **0x34008081** |

独立复核证据（主机侧 Python 按规划器同一算法重算，只读操作）：
```
PT_LOAD paddr=0x34000000 filesz=34252 memsz=37064 exec=True
recomputed plan sha256: ed963f42c1bdd254e7682286a305480a65d3747397811c8fe9f3f73522f5089d  → MATCH
vector[0] MSP=0x34200000  vector[1] Reset=0x34008081
```
即：被加载镜像 = 授权基线 ELF（`38ea1f13…`），加载结果与该 ELF 确定性一致，未发生任何"错文件"或"错地址"事件。

## 8. 安全合规确认

- **是否写入 Flash**：否。仅 SRAM 白名单 0x34000000–0x34200000 内 37064 字节；未调用 flash_erase/flash_program/flash_verify/run_firmware。
- **白名单外写入**：无（唯一 PT_LOAD 完全位于白名单内；VTOR 写 0xE000ED08 为启动序列的调试寄存器写，属授权 start=true 语义）。
- **reset**：未调用。
- **故障 ELF**：未加载。
- **残留会话/断点**：无（唯一会话已 disconnect；本会话未设置任何断点）。
- **重试**：无（一次成功，未重试）。

## 9. 结论

```text
基线 SRAM 加载干跑: PASS
```

`load_elf_to_sram` 在真实硬件上完成了：ELF 解析 → 白名单/向量校验 → 自动 halt → 分块写入（19 chunks / 37064B）→ 逐字节回读 + 双 SHA-256 一致 → VTOR/MSP/xPSR/PC 编程 → 启动 → 目标 Running。唯一需要评阅的是第 7 节的期望值澄清（指令中两个数值归属 fixture-baseline 而非授权基线 ELF）。
