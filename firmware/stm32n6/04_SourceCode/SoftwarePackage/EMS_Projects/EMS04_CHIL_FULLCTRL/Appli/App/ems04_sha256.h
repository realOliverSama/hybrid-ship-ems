/* ems04_sha256.h — EMS04-S2 B1：FIPS 180-4 SHA-256（流式 init/update/final）。
 *
 * 用途：kf 航次表 row payload 完整性校验（FEATURE_PANEL_CONTRACT.md §3、
 * PROTOCOL_FP.md §1/§2 声明值 vs 重组结果的位级比对）。
 *
 * 纪律：纯 C99；无动态内存；无机器字节序假设（显式大端装配、按字节输出）；
 * 不依赖 libcrypto/libssl。本文件语义不得为“让测试通过”而修改。
 */
#ifndef EMS04_SHA256_H
#define EMS04_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t h[8];          /* 链接变量 */
    uint64_t total_bytes;   /* 已吃入字节数（FIPS 长度域） */
    uint8_t  buf[64];       /* 未满块暂存 */
    uint32_t buf_len;       /* [0,64) */
} ems04_sha256_ctx_t;

void ems04_sha256_init(ems04_sha256_ctx_t *ctx);
void ems04_sha256_update(ems04_sha256_ctx_t *ctx, const void *data, size_t len);
void ems04_sha256_final(ems04_sha256_ctx_t *ctx, uint8_t out[32]);

/* 便捷封装：一次吃完（内部即 init/update/final，与流式结果必须逐位相同）。 */
void ems04_sha256(const void *data, size_t len, uint8_t out[32]);

#ifdef __cplusplus
}
#endif

#endif /* EMS04_SHA256_H */
