/**
  ******************************************************************************
  * @file    ems04_s3c_q27_shadow.c
  * @brief   EMS04 S3-C Q27 影子拷贝诊断（qualification-only）
  ******************************************************************************
  * @attention
  *
  * 非影子构建零符号零体积：本 TU 整体被
  * #if defined(EMS04_Q27_SHADOW) && EMS04_Q27_SHADOW 包裹（-DEMS04_Q27_SHADOW=0
  * 或未定义时编译产物为空，头文件声明无害）。构建接线见 Appli/CMakeLists.txt
  * 末尾独立块；生成码插桩见 Host/tools/patch_reattn1n_q27shadow.py。
  *
  * 缓冲 = 单个静态 struct（诊断头 + 3300 B payload），普通 .bss（SRAM1-2 内，
  * 禁加 section 属性——PREDPARITY 链接脚本仅定向 ems04_fp_board.o 的 BSS 去
  * HyperRAM，本 TU 不受影响）。
  *
  * CRC-32/IEEE：反射式，多项式 0xEDB88320，初值/终值异或 0xFFFFFFFF，
  * 逐位自实现，不引外部库。仅诊断路径执行（每 REATT query 一次，3300 B
  * × 8 次迭代/字节，CM55 上可忽略）。
  ******************************************************************************
  */

#include "ems04_s3c_q27_shadow.h"

#if defined(EMS04_Q27_SHADOW) && EMS04_Q27_SHADOW

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(ems04_s3c_q27_shadow_hdr_t) == EMS04_S3C_Q27_SHADOW_HDR_SIZE,
               "q27 shadow header layout drift (expected 28 B, no padding)");
_Static_assert(sizeof(ems04_s3c_q27_shadow_hdr_t) + EMS04_S3C_Q27_SHADOW_LEN
                   == EMS04_S3C_Q27_SHADOW_BUF_SIZE,
               "q27 shadow buffer size macro drift (expected 3328 B)");

/* 缓冲 = 单个静态 struct（诊断头 + Q27 payload，普通 .bss，SRAM1-2）：
 * 单 struct 保证 payload 紧跟头后（offset 28），链接器布局自由度不影响契约 */
typedef struct
{
    ems04_s3c_q27_shadow_hdr_t hdr;
    uint8_t payload[EMS04_S3C_Q27_SHADOW_LEN];
} ems04_s3c_q27_shadow_buf_t;

static ems04_s3c_q27_shadow_buf_t g_ems04_s3c_q27_shadow;

/* 内部自增序号（首次 capture = 1） */
static uint32_t g_ems04_s3c_q27_seq = 0u;

/* CRC-32/IEEE 反射式（poly 0xEDB88320），逐位自实现 */
static uint32_t ems04_s3c_q27_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    int b;

    for (i = 0u; i < len; i++)
    {
        crc ^= (uint32_t)data[i];
        for (b = 0; b < 8; b++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

void ems04_s3c_q27_shadow_capture(const void *src)
{
    /* fail-closed：任何中途异常/复位遗留的旧 valid 先失效 */
    g_ems04_s3c_q27_shadow.hdr.valid = 0u;

    g_ems04_s3c_q27_seq += 1u;

    /* 影子拷贝：对响应 Q27 生命周期仅读（Clean_Range 之后，arena 复用之前） */
    (void)memcpy(g_ems04_s3c_q27_shadow.payload, src, EMS04_S3C_Q27_SHADOW_LEN);

    /* 诊断头（全部显式字段） */
    g_ems04_s3c_q27_shadow.hdr.magic    = EMS04_S3C_Q27_SHADOW_MAGIC;
    g_ems04_s3c_q27_shadow.hdr.version  = EMS04_S3C_Q27_SHADOW_VERSION;
    g_ems04_s3c_q27_shadow.hdr.hdr_len  = (uint16_t)EMS04_S3C_Q27_SHADOW_HDR_SIZE;
    g_ems04_s3c_q27_shadow.hdr.seq      = g_ems04_s3c_q27_seq;
    g_ems04_s3c_q27_shadow.hdr.src_addr = EMS04_S3C_Q27_SHADOW_SRC_ADDR;
    g_ems04_s3c_q27_shadow.hdr.len      = EMS04_S3C_Q27_SHADOW_LEN;
    g_ems04_s3c_q27_shadow.hdr.crc32    = ems04_s3c_q27_crc32(g_ems04_s3c_q27_shadow.payload,
                                                       EMS04_S3C_Q27_SHADOW_LEN);

    /* 拷贝+CRC 全部完成后才置 1 */
    g_ems04_s3c_q27_shadow.hdr.valid = 1u;
}

const void *ems04_s3c_q27_shadow_get(void)
{
    /* 返回缓冲首地址：诊断头在前，payload 紧随其后（见宏布局注释） */
    return (const void *)&g_ems04_s3c_q27_shadow;
}

void ems04_s3c_q27_shadow_reset(void)
{
    g_ems04_s3c_q27_shadow.hdr.valid = 0u;
}

#endif /* defined(EMS04_Q27_SHADOW) && EMS04_Q27_SHADOW */
