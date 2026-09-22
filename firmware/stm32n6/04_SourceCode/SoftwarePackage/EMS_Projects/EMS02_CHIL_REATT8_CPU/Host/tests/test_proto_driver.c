/* test_proto_driver.c — ems_proto 主机环回测试驱动（T-506，spec ems01-chil-ecms-v1）。
 *
 * 由 test_proto_loopback.py 以 gcc -Wall -Wextra -std=c11 编译并经 stdin 行协议
 * 驱动（test_driver.c 同风格，T-503 环境回归沿用）。命令表：
 *   INIT                  重新 ems_proto_init（清计数/会话/事件环）
 *   TIME <ms>             设假时基（now_ms 回调返回值）
 *   CRC <hex>             ems_proto_crc16 → "CRC xxxx"
 *   OLDCRC <hex>          T-505 ems_app.c 原始 ems_crc16_ccitt 逐字迁移副本
 *   CHAIN <crc> <hex>     分段续算（新实现）；OLDCHAIN 同（旧实现）
 *   BUILD <type> <hex|->  ems_proto_send_frame → TX <hex>（含 CRC 的完整帧）
 *   FEED <hex>            ems_proto_feed_buf；期间吐 FRAME/TX/TXTEXT 行，尾行 STAT
 *   DECTICK <hex>         ems_proto_dec_cmd_tick 解码自检 → "DECTICK idx= t= P= ..."
 *   DECRSP <hex>          RSP_TICK 解码自检 → "DECRSP idx= P= e= K= cls= iu= cu= f="
 *   ENCRSP <idx> <P> <e> <K> <cls> <iu> <cu> <flags_hex>
 *                         ems_proto_enc_rsp_tick 字段序自检 → "ENCRSP <hex>"
 *   EVENTS                NEV <count> + EVENT n= type= aux= tick= ms= 逐行
 *   DUMP                  ems_proto_dump_events → TXTEXT 文本行
 *   RESETS                ems_proto_session_reset
 * 事件/计数面为机器可读证据（AGENTS.md：以实际输出为准，不主观解释）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems_proto.h"

/* ---- T-505 ems_app.c 原始实现（迁移契约的位级基准，逐字未改） ---- */
static uint16_t old_crc16_ccitt(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu; /* CCITT-FALSE 初值 */
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* T-505 ems_app.c 原始分段续算（迁移契约参考版） */
static uint16_t old_crc16_ccitt_chain(uint16_t crc_in, const uint8_t *data,
                                      uint32_t len)
{
    uint16_t crc = crc_in;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (int b = 0; b < 8; b++) {
            if ((crc & 0x8000u) != 0u) {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* 假时基（TIME 命令可编程；design §UART 会话时基经回调注入） */
static uint32_t g_ms = 0u;

static void tx_cap(void *user, const uint8_t *buf, uint32_t len)
{
    (void)user;
    int printable = (len > 0u);
    for (uint32_t i = 0; i < len; i++) {
        const uint8_t c = buf[i];
        const int ok = ((c >= 0x20u) && (c <= 0x7Eu)) || (c == 0x0Du) || (c == 0x0Au);
        if (!ok) { printable = 0; break; }
    }
    if (printable) {
        printf("TXTEXT ");
        for (uint32_t i = 0; i < len; i++) {
            if (buf[i] == (uint8_t)'\r') { printf("\\r"); }
            else if (buf[i] == (uint8_t)'\n') { printf("\\n"); }
            else { putchar((int)buf[i]); }
        }
    } else {
        printf("TX ");
        for (uint32_t i = 0; i < len; i++) {
            printf("%02X", buf[i]);
        }
    }
    printf("\n");
    (void)fflush(stdout);
}

static uint32_t now_cb(void *user)
{
    (void)user;
    return g_ms;
}

static void frame_cap(void *user, uint8_t type, const uint8_t *payload,
                      uint16_t len, uint32_t seq)
{
    (void)user;
    (void)seq; /* 载荷级 seq（2026-09-01）：主机环回用例按载荷字节断言，不单独回显 */
    printf("FRAME %02X %u ", type, (unsigned)len);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X", payload[i]);
    }
    printf("\n");
    (void)fflush(stdout);
}

static ems_proto_t g_proto;

static void print_stat(void)
{
    printf("STAT ok=%u crcerr=%u stale=%u sess=%u ev=%u total=%u\n",
           (unsigned)g_proto.stat_frames_ok, (unsigned)g_proto.stat_crc_err,
           (unsigned)ems_proto_session_stale(&g_proto),
           (unsigned)g_proto.session_active,
           (unsigned)ems_proto_event_count(&g_proto), (unsigned)g_proto.ev_total);
}

static int hexval(int c)
{
    if ((c >= '0') && (c <= '9')) { return c - '0'; }
    if ((c >= 'a') && (c <= 'f')) { return c - 'a' + 10; }
    if ((c >= 'A') && (c <= 'F')) { return c - 'A' + 10; }
    return -1;
}

/* hex → bytes（返回字节数；奇数串忽略，调用方保证） */
static size_t hexread(const char *s, uint8_t *out, size_t cap)
{
    size_t n = 0;
    while ((s[0] != '\0') && (s[1] != '\0')) {
        const int hi = hexval((unsigned char)s[0]);
        const int lo = hexval((unsigned char)s[1]);
        if ((hi < 0) || (lo < 0) || (n >= cap)) {
            break;
        }
        out[n++] = (uint8_t)((hi << 4) | lo);
        s += 2;
    }
    return n;
}

int main(void)
{
    static char line[16384];
    static uint8_t buf[EMS_PROTO_FRAME_MAX + 8u];

    ems_proto_init(&g_proto, tx_cap, NULL, now_cb, NULL, frame_cap, NULL);

    while (fgets(line, sizeof line, stdin) != NULL) {
        char *nl = strchr(line, '\n');
        if (nl != NULL) { *nl = '\0'; }
        nl = strchr(line, '\r');
        if (nl != NULL) { *nl = '\0'; }
        if ((line[0] == '\0') || (line[0] == '#')) {
            continue;
        }
        const char *cmd = strtok(line, " ");
        const char *a1 = strtok(NULL, " ");
        const char *a2 = strtok(NULL, " ");
        const char *a3 = strtok(NULL, " ");
        const char *a4 = strtok(NULL, " ");
        const char *a5 = strtok(NULL, " ");
        const char *a6 = strtok(NULL, " ");
        const char *a7 = strtok(NULL, " ");
        const char *a8 = strtok(NULL, " ");

        if (strcmp(cmd, "INIT") == 0) {
            ems_proto_init(&g_proto, tx_cap, NULL, now_cb, NULL, frame_cap, NULL);
            print_stat();
        } else if (strcmp(cmd, "TIME") == 0) {
            g_ms = (uint32_t)strtoul(a1, NULL, 10);
            printf("TIME %lu\n", (unsigned long)g_ms);
        } else if (strcmp(cmd, "CRC") == 0) {
            const size_t n = hexread(a1, buf, sizeof buf);
            printf("CRC %04X\n", (unsigned)ems_proto_crc16(buf, (uint32_t)n));
        } else if (strcmp(cmd, "OLDCRC") == 0) {
            const size_t n = hexread(a1, buf, sizeof buf);
            printf("CRC %04X\n", (unsigned)old_crc16_ccitt(buf, (uint32_t)n));
        } else if (strcmp(cmd, "CHAIN") == 0) {
            const uint16_t c = (uint16_t)strtoul(a1, NULL, 16);
            const size_t n = hexread(a2, buf, sizeof buf);
            printf("CRC %04X\n",
                   (unsigned)ems_proto_crc16_chain(c, buf, (uint32_t)n));
        } else if (strcmp(cmd, "OLDCHAIN") == 0) {
            const uint16_t c = (uint16_t)strtoul(a1, NULL, 16);
            const size_t n = hexread(a2, buf, sizeof buf);
            printf("CRC %04X\n",
                   (unsigned)old_crc16_ccitt_chain(c, buf, (uint32_t)n));
        } else if (strcmp(cmd, "BUILD") == 0) {
            /* BUILD <type_hex> <payload_hex|-> → TX <完整帧 hex> + BUILD ok? */
            const uint8_t type = (uint8_t)strtoul(a1, NULL, 16);
            const size_t n = (a2 != NULL) ? hexread(a2, buf, sizeof buf) : 0u;
            const uint8_t ok =
                ems_proto_send_frame(&g_proto, type, buf, (uint16_t)n);
            printf("BUILD %u\n", (unsigned)ok);
        } else if (strcmp(cmd, "FEED") == 0) {
            const size_t n = hexread(a1, buf, sizeof buf);
            ems_proto_feed_buf(&g_proto, buf, (uint32_t)n);
            print_stat();
        } else if (strcmp(cmd, "DECTICK") == 0) {
            /* payload 解码自检：→ DECTICK idx=.. t=.. P=.. SOC=.. vc=.. vs=.. a=.. x=.. */
            ems_proto_cmd_tick_t t;
            const size_t n = hexread(a1, buf, sizeof buf);
            if (ems_proto_dec_cmd_tick(buf, (uint16_t)n, &t) != 0) {
                printf("DECTICK ERR\n");
            } else {
                printf("DECTICK idx=%lu t=%.9g P=%.9g SOC=%.9g vcmd=%.9g "
                       "vs=%.9g a=%.9g x=%.9g\n",
                       (unsigned long)t.tick_idx, (double)t.sig[0],
                       (double)t.sig[1], (double)t.sig[2], (double)t.sig[3],
                       (double)t.sig[4], (double)t.sig[5], (double)t.sig[6]);
            }
        } else if (strcmp(cmd, "DECRSP") == 0) {
            ems_proto_rsp_tick_t r;
            const size_t n = hexread(a1, buf, sizeof buf);
            if (ems_proto_dec_rsp_tick(buf, (uint16_t)n, &r) != 0) {
                printf("DECRSP ERR\n");
            } else {
                printf("DECRSP idx=%lu P=%.9g e=%.9g K=%.9g cls=%u iu=%lu "
                       "cu=%lu f=%02X\n",
                       (unsigned long)r.tick_idx, (double)r.P_gen_ref,
                       (double)r.e_hat, (double)r.K, (unsigned)r.cls_eff,
                       (unsigned long)r.infer_us, (unsigned long)r.ctrl_us,
                       (unsigned)r.flags);
            }
        } else if (strcmp(cmd, "ENCRSP") == 0) {
            /* ENCRSP <idx> <P> <e> <K> <cls> <iu> <cu> <flags_hex> → 编码 hex 自检 */
            ems_proto_rsp_tick_t r;
            uint8_t pay[EMS_PROTO_RSP_TICK_LEN];
            r.tick_idx = (uint32_t)strtoul(a1, NULL, 10);
            r.P_gen_ref = (float)atof(a2);
            r.e_hat = (float)atof(a3);
            r.K = (float)atof(a4);
            r.cls_eff = (uint8_t)strtoul(a5, NULL, 16);
            r.infer_us = (uint32_t)strtoul(a6, NULL, 10);
            r.ctrl_us = (uint32_t)strtoul(a7, NULL, 10);
            r.flags = (uint8_t)strtoul(a8, NULL, 16);
            ems_proto_enc_rsp_tick(pay, &r);
            printf("ENCRSP ");
            for (int i = 0; i < (int)EMS_PROTO_RSP_TICK_LEN; i++) {
                printf("%02X", pay[i]);
            }
            printf("\n");
        } else if (strcmp(cmd, "EVENTS") == 0) {
            const uint32_t n = ems_proto_event_count(&g_proto);
            printf("NEV %lu\n", (unsigned long)n);
            for (uint32_t i = 0; i < n; i++) {
                const ems_proto_event_t *e = ems_proto_event_at(&g_proto, i);
                printf("EVENT n=%lu type=%u aux=%u tick=%lu ms=%lu\n",
                       (unsigned long)i, (unsigned)e->type, (unsigned)e->aux,
                       (unsigned long)e->tick, (unsigned long)e->ms);
            }
        } else if (strcmp(cmd, "DUMP") == 0) {
            ems_proto_dump_events(&g_proto); /* 文本行经 TXTEXT 回显 */
        } else if (strcmp(cmd, "RESETS") == 0) {
            ems_proto_session_reset(&g_proto);
            print_stat();
        } else if (strcmp(cmd, "LASTTICK") == 0) {
            printf("LASTTICK %08lX\n",
                  (unsigned long)ems_proto_last_tick_idx(&g_proto));
        } else if (strcmp(cmd, "NOP") == 0) {
            printf("NOP\n");
        } else {
            printf("ERR unknown\n");
        }
    }
    return 0;
}