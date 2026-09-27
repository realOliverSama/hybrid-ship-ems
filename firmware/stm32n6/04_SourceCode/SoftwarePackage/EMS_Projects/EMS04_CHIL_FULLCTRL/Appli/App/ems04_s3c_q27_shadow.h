/**
  ******************************************************************************
  * @file    ems04_s3c_q27_shadow.h
  * @brief   EMS04 S3-C Q27 影子拷贝诊断（qualification-only）
  ******************************************************************************
  * @attention
  *
  * 背景（S3-C 资格判定 Q27 生命周期取证，用户裁决 §1-§4 APPROVED）：
  * REATT N1 推理（生成码 Model/reatt8_q_npu/npu_n1/gen/reattn1n.c 的
  * LL_ATON_End_EpochBlock_2）内，软件量化算子 ll_sw_forward_quantizelinear
  * 产出 Q27 张量（int8 [1,300,11]，3300 B，物理地址 0x34350000+28800 =
  * 0x34357080），其后的 LL_ATON_Cache_MCU_Clean_Range 已将其 clean 到物理，
  * 但该 NPU arena 区域随后被后续 epoch 复用覆盖。本模块在该行之后插入一次
  * 影子 memcpy，使 Q27 在生命周期内被留存取证（Q27 是否 == HOST_B 决定
  * S3-C 走 FAIL 还是 N1_TARGET_EXECUTION 分支）。
  *
  * 接线：Host/tools/patch_reattn1n_q27shadow.py 对生成码做单一锚点插入；
  * 构建需 -DEMS04_Q27_SHADOW=1（Appli/CMakeLists.txt 末尾独立块）。非影子
  * 构建零符号零体积（.c 实现整体被宏包裹）；reattn1n.c 的插入调用不条件
  * 编译——已补丁的生成码配非影子构建会链接失败，需先 --restore。
  *
  * 本模块只读响应 Q27 生命周期（一次 memcpy + CRC），不改数学、控制流、
  * NPU 输入与图字节；非侵入性自证见 q27shadow_provenance.json 与
  * patch 脚本注释。
  *
  * 禁止用于生产/正式 soft-HIL 航次（qualification-only）。
  ******************************************************************************
  */

#ifndef EMS04_S3C_Q27_SHADOW_H
#define EMS04_S3C_Q27_SHADOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 诊断头固定常量（全部显式字段，见下方布局） */
#define EMS04_S3C_Q27_SHADOW_MAGIC       0x51323753u  /* "Q27S" */
#define EMS04_S3C_Q27_SHADOW_VERSION     1u
#define EMS04_S3C_Q27_SHADOW_SRC_ADDR    0x34357080u  /* 0x34350000UL + 28800 */
#define EMS04_S3C_Q27_SHADOW_LEN         3300u        /* int8 [1,300,11] */

/* 诊断头大小：8 个显式字段，天然 4 字节对齐、无 padding（TU 内有
 * _Static_assert 复核）。 */
#define EMS04_S3C_Q27_SHADOW_HDR_SIZE    28u

/* 专用静态缓冲总大小（诊断头 + payload，普通 .bss / SRAM1-2） */
#define EMS04_S3C_Q27_SHADOW_BUF_SIZE                                              \
    (EMS04_S3C_Q27_SHADOW_HDR_SIZE + EMS04_S3C_Q27_SHADOW_LEN) /* = 3328 */

/* 诊断头布局（小端，紧随其后为 3300 B payload）：
 *   magic   u32 = 0x51323753 ("Q27S")
 *   version u16 = 1
 *   hdr_len u16 = 28
 *   seq     u32 = 内部自增计数器（首次 capture = 1）
 *   src_addr u32 = 0x34357080
 *   len     u32 = 3300
 *   crc32   u32 = CRC-32/IEEE（反射式，poly 0xEDB88320，对 3300 码字节）
 *   valid   u32 = 拷贝+CRC 全部完成后才写 1，开始前先清 0（fail-closed）
 */
typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t hdr_len;
    uint32_t seq;
    uint32_t src_addr;
    uint32_t len;
    uint32_t crc32;
    uint32_t valid;
} ems04_s3c_q27_shadow_hdr_t;

/**
  * @brief  影子拷贝 Q27 张量到专用静态缓冲并写诊断头。
  *         入口即清 valid=0（fail-closed），memcpy+CRC 全部完成后才置 valid=1。
  *         由补丁后的 reattn1n.c 在 Cache_MCU_Clean_Range(Q27) 之后调用。
  * @param  src  Q27 虚拟地址（ATON_LIB_PHYSICAL_TO_VIRTUAL_ADDR(0x34350000+28800)）
  */
void ems04_s3c_q27_shadow_capture(const void *src);

/**
  * @brief  返回影子缓冲首地址（诊断头在前；读取方须先检查 hdr.valid）。
  */
const void *ems04_s3c_q27_shadow_get(void);

/**
  * @brief  清 valid（seq 计数器不清零）。取证读取前的失效开关。
  */
void ems04_s3c_q27_shadow_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_S3C_Q27_SHADOW_H */
