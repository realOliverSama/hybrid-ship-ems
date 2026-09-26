/* ems04_sha256.c — EMS04-S2 B1：FIPS 180-4 SHA-256 实现（流式）。
 *
 * 参考：NIST FIPS PUB 180-4（2015-08）§5.1/§6.2。
 * KAT（b1_unit.c 内固化公布答案）："" / "abc" / 56 B 边界 / 1,000,000×'a'。
 */
#include "ems04_sha256.h"

#include <string.h>

/* ---- FIPS 180-4 §4.2.2 轮常数 ---- */
static const uint32_t K256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

/* ---- FIPS 180-4 §4.1.2 初始链接变量 ---- */
static const uint32_t H0_INIT[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
};

static uint32_t rotr32(uint32_t x, uint32_t n)
{
    return (uint32_t)((x >> n) | (x << (32u - n)));
}

/* 大端装配（FIPS 180-4 §3.2：消息块按大端字节序解释） */
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void be64_store(uint64_t v, uint8_t *p)
{
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >> 8);
    p[7] = (uint8_t)(v);
}

static void sha256_block(uint32_t h[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, hh;
    uint32_t i;

    for (i = 0; i < 16u; ++i) {
        w[i] = be32(block + 4u * i);
    }
    for (i = 16u; i < 64u; ++i) {
        uint32_t s0 = rotr32(w[i - 15u], 7) ^ rotr32(w[i - 15u], 18) ^ (w[i - 15u] >> 3);
        uint32_t s1 = rotr32(w[i - 2u], 17) ^ rotr32(w[i - 2u], 19) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; hh = h[7];

    for (i = 0; i < 64u; ++i) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + s1 + ch + K256[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;

        hh = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void ems04_sha256_init(ems04_sha256_ctx_t *ctx)
{
    uint32_t i;
    for (i = 0; i < 8u; ++i) {
        ctx->h[i] = H0_INIT[i];
    }
    ctx->total_bytes = 0u;
    ctx->buf_len = 0u;
    memset(ctx->buf, 0, sizeof(ctx->buf));
}

void ems04_sha256_update(ems04_sha256_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    if (len == 0u) {
        return;
    }
    ctx->total_bytes += (uint64_t)len;

    if (ctx->buf_len > 0u) {
        uint32_t need = 64u - ctx->buf_len;
        uint32_t take = (len < (size_t)need) ? (uint32_t)len : need;
        memcpy(ctx->buf + ctx->buf_len, p, take);
        ctx->buf_len += take;
        p += take;
        len -= (size_t)take;
        if (ctx->buf_len == 64u) {
            sha256_block(ctx->h, ctx->buf);
            ctx->buf_len = 0u;
        }
    }
    while (len >= 64u) {
        sha256_block(ctx->h, p);
        p += 64u;
        len -= 64u;
    }
    if (len > 0u) {
        memcpy(ctx->buf, p, len);
        ctx->buf_len = (uint32_t)len;
    }
}

void ems04_sha256_final(ems04_sha256_ctx_t *ctx, uint8_t out[32])
{
    uint64_t bitlen = ctx->total_bytes * 8u;
    uint8_t pad[72];
    uint32_t pad_len;
    uint32_t i;

    /* 填充：0x80 + k 个 0x00，使 长度 ≡ 56 (mod 64)，再附 64 bit 大端长度 */
    pad_len = (ctx->buf_len < 56u) ? (56u - ctx->buf_len) : (120u - ctx->buf_len);
    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80u;
    ems04_sha256_update(ctx, pad, pad_len);
    be64_store(bitlen, pad);
    ems04_sha256_update(ctx, pad, 8u);

    for (i = 0; i < 8u; ++i) {
        out[4u * i]      = (uint8_t)(ctx->h[i] >> 24);
        out[4u * i + 1u] = (uint8_t)(ctx->h[i] >> 16);
        out[4u * i + 2u] = (uint8_t)(ctx->h[i] >> 8);
        out[4u * i + 3u] = (uint8_t)(ctx->h[i]);
    }
    /* 清零中间态（避免残留链接变量被误用） */
    memset(ctx, 0, sizeof(*ctx));
}

void ems04_sha256(const void *data, size_t len, uint8_t out[32])
{
    ems04_sha256_ctx_t ctx;
    ems04_sha256_init(&ctx);
    ems04_sha256_update(&ctx, data, len);
    ems04_sha256_final(&ctx, out);
}
