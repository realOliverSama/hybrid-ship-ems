/**
  ******************************************************************************
  * @file    ems04_s3c_bddiag.c
  * @brief   EMS04 S3-C BD 腿批量诊断回传（qualification-only，开关默认 0）
  ******************************************************************************
  * @attention
  *
  * 本 TU 整体被 #if defined(EMS04_BDDIAG_UART) && EMS04_BDDIAG_UART 包裹：
  * 默认构建零符号零体积。接线见 Appli/CMakeLists.txt 独立块；主机侧客户端
  * Host/tools/s3c_bddiag_client.py。
  *
  * 只读边界缓冲（ems_reatt_split.c 的 rs_n1_hpast/rs_n2_out/rs_n2_kf，
  * 经同开关包裹的访问器）与 Q27 影子头（ems04_s3c_q27_shadow_get()，若
  * EMS04_Q27_SHADOW=1）。打包为定长 666 B（布局见头文件注释）。
  ******************************************************************************
  */

#include "ems04_s3c_bddiag.h"

#if defined(EMS04_BDDIAG_UART) && EMS04_BDDIAG_UART

#include <string.h>

#if defined(EMS04_Q27_SHADOW) && EMS04_Q27_SHADOW
#include "ems04_s3c_q27_shadow.h"
#endif

static void bddiag_wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

void ems04_s3c_bddiag_fill(uint8_t *rsp, uint32_t seq)
{
    uint32_t crc = 0u;
    uint8_t valid = 0u;

#if defined(EMS04_Q27_SHADOW) && EMS04_Q27_SHADOW
    {
        const ems04_s3c_q27_shadow_hdr_t *h =
            (const ems04_s3c_q27_shadow_hdr_t *)ems04_s3c_q27_shadow_get();
        crc = h->crc32;
        valid = (uint8_t)((h->valid != 0u) ? 1u : 0u);
    }
#endif

    bddiag_wr_u32(&rsp[0], seq);
    bddiag_wr_u32(&rsp[4], crc);
    rsp[8] = valid;
    rsp[9] = 0u;
    rsp[10] = 0u;
    rsp[11] = 0u;
    memcpy(&rsp[12], (const void *)ems04_s3c_bddiag_hpast(),
           EMS04_S3C_BDDIAG_HPAST_N * sizeof(float));
    memcpy(&rsp[396], (const void *)ems04_s3c_bddiag_n2out(),
           EMS04_S3C_BDDIAG_N2OUT_N);
    memcpy(&rsp[486], (const void *)ems04_s3c_bddiag_n2kf(),
           EMS04_S3C_BDDIAG_N2KF_N);
}

#endif /* EMS04_BDDIAG_UART */
