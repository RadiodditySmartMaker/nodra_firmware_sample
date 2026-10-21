/*
 * Layer 4b — SX126x benchmark TX/RX.
 *
 * Ported from old lora_ra01s_bench.cpp (RadioLib-based).
 * Protocol: 255-byte packets with magic 'L' 'R', 4-byte seq, CRC8.
 */

#include "lora_sx126x_bench.h"

#if defined(PIO_ENABLE_LORA_RA01S)

#include "lora_sx126x_config.h"
#include "lora_sx126x_shell.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#if defined(PIO_ENABLE_SHELL_MODULE)
#include <shell_module.h>
#else
static void dbg_log(const char *tag, const char *fmt, ...) {}
#endif /* PIO_ENABLE_SHELL_MODULE */

/* ================================================================== */
/* Constants                                                           */
/* ================================================================== */
#define BENCH_PKT_SIZE 255
#define BENCH_MAGIC_0 0x4C /* 'L' */
#define BENCH_MAGIC_1 0x52 /* 'R' */
#define BENCH_CRC_OFFSET 254
#define BENCH_RX_IDLE_MS 3000

/* ================================================================== */
/* Mode enum                                                           */
/* ================================================================== */
typedef enum {
    BENCH_NONE = 0,
    BENCH_TX,
    BENCH_RX,
} bench_mode_t;

static bench_mode_t s_bench_mode;

/* ── TX state ── */
static uint32_t s_tx_target;
static uint32_t s_tx_count;
static uint32_t s_tx_errors;
static uint64_t s_tx_start_ms;
static uint64_t s_tx_last_report_ms;

/* ── RX state ── */
static uint32_t s_rx_valid;
static uint32_t s_rx_corrupt;
static uint32_t s_rx_duplicate;
static uint32_t s_rx_out_of_order;
static uint32_t s_rx_gap;
static uint32_t s_rx_first_seq;
static uint32_t s_rx_last_seq;
static uint32_t s_rx_expected_next;
static bool s_rx_have_first;
static uint64_t s_rx_first_ms;
static uint64_t s_rx_last_ms;
static uint64_t s_rx_idle_last_ms;

/* ================================================================== */
/* CRC8 polynomial 0x07                                                */
/* ================================================================== */
static uint8_t bench_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1) ^ 0x07U);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ================================================================== */
/* Packet encode / validate                                            */
/* ================================================================== */
static void bench_pkt_encode(uint8_t *buf, uint32_t seq)
{
    memset(buf, 0, BENCH_PKT_SIZE);
    buf[0] = BENCH_MAGIC_0;
    buf[1] = BENCH_MAGIC_1;
    buf[2] = (uint8_t)(seq & 0xFF);
    buf[3] = (uint8_t)((seq >> 8) & 0xFF);
    buf[4] = (uint8_t)((seq >> 16) & 0xFF);
    buf[5] = (uint8_t)((seq >> 24) & 0xFF);
    buf[BENCH_CRC_OFFSET] = bench_crc8(buf, BENCH_CRC_OFFSET);
}

static bool bench_pkt_validate(const uint8_t *data, uint16_t size, uint32_t *seq_out)
{
    if (size != BENCH_PKT_SIZE || data[0] != BENCH_MAGIC_0 || data[1] != BENCH_MAGIC_1) {
        return false;
    }
    if (bench_crc8(data, BENCH_CRC_OFFSET) != data[BENCH_CRC_OFFSET]) {
        return false;
    }
    *seq_out = (uint32_t)data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
    return true;
}

/* ================================================================== */
/* RX statistics                                                       */
/* ================================================================== */
void bench_rx_reset_stats(void)
{
    s_rx_valid = 0;
    s_rx_corrupt = 0;
    s_rx_duplicate = 0;
    s_rx_out_of_order = 0;
    s_rx_gap = 0;
    s_rx_have_first = false;
    s_rx_first_ms = 0;
    s_rx_last_ms = 0;
}

static void bench_rx_on_valid(uint32_t seq, uint64_t now_ms)
{
    if (!s_rx_have_first) {
        s_rx_first_seq = seq;
        s_rx_last_seq = seq;
        s_rx_expected_next = seq + 1U;
        s_rx_have_first = true;
        s_rx_first_ms = now_ms;
        s_rx_last_ms = now_ms;
        s_rx_valid++;
        return;
    }

    if (seq < s_rx_expected_next) {
        if (seq == s_rx_last_seq) {
            s_rx_duplicate++;
        } else {
            s_rx_out_of_order++;
        }
        return;
    }

    if (seq == s_rx_expected_next) {
        s_rx_valid++;
        s_rx_expected_next = seq + 1U;
        s_rx_last_seq = seq;
        s_rx_last_ms = now_ms;
        return;
    }

    /* Gap detected */
    s_rx_gap += seq - s_rx_expected_next;
    s_rx_valid++;
    s_rx_expected_next = seq + 1U;
    s_rx_last_seq = seq;
    s_rx_last_ms = now_ms;
}

static void bench_rx_print_summary(void)
{
    uint64_t elapsed_ms = 0;
    uint32_t seq_gap_loss = 0;
    uint32_t pps_x100 = 0;
    uint32_t kbps_x10 = 0;

    if (s_rx_have_first && s_rx_valid > 0U) {
        seq_gap_loss = (s_rx_last_seq - s_rx_first_seq + 1U) - s_rx_valid;
    }
    if (s_rx_valid >= 2U) {
        elapsed_ms = s_rx_last_ms - s_rx_first_ms;
    }
    if (elapsed_ms > 0) {
        pps_x100 = (uint32_t)((s_rx_valid * 100000ULL) / elapsed_ms);
        kbps_x10 = (uint32_t)((s_rx_valid * (uint64_t)BENCH_PKT_SIZE * 8 * 10) / elapsed_ms);
    }

    dbg_log("lora", "bench RX done");
    dbg_log("lora", "  rx_valid      : %u", s_rx_valid);
    dbg_log("lora", "  corrupt       : %u", s_rx_corrupt);
    dbg_log("lora", "  duplicate     : %u", s_rx_duplicate);
    dbg_log("lora", "  out_of_order  : %u", s_rx_out_of_order);
    dbg_log("lora", "  gap_events    : %u", s_rx_gap);
    dbg_log("lora", "  seq_gap_loss  : %u", seq_gap_loss);
    if (s_rx_have_first) {
        dbg_log("lora", "  first_seq     : %u", s_rx_first_seq);
        dbg_log("lora", "  last_seq      : %u", s_rx_last_seq);
    }
    dbg_log("lora", "  elapsed_ms    : %lu", (unsigned long)elapsed_ms);
    if (elapsed_ms > 0) {
        dbg_log("lora", "  pps           : %u.%02u", pps_x100 / 100U, pps_x100 % 100U);
        dbg_log("lora", "  kbps_raw      : %u.%u", kbps_x10 / 10U, kbps_x10 % 10U);
    }
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void sx126x_bench_init(void)
{
    s_bench_mode = BENCH_NONE;
}

bool sx126x_bench_is_busy(void)
{
    return s_bench_mode != BENCH_NONE;
}

bool sx126x_bench_is_rx(void)
{
    return s_bench_mode == BENCH_RX;
}

int sx126x_bench_reject_if_busy(char *out, size_t out_len)
{
    if (sx126x_bench_is_busy()) {
        snprintf(out, out_len, "bench running, wait for completion\r\n");
        return -2;
    }
    return 0;
}

/* ── TX (one packet per call, driven by PioThread) ── */

int32_t sx126x_bench_tx_run_once(void)
{
    if (s_bench_mode != BENCH_TX)
        return -1;

    uint8_t buf[BENCH_PKT_SIZE];
    bench_pkt_encode(buf, s_tx_count);
    s_tx_count++;

    if (s_tx_count == 1) {
        s_tx_start_ms = millis();
        s_tx_last_report_ms = s_tx_start_ms;
    }

    int ret = sx126x_shell_send(buf, BENCH_PKT_SIZE);
    if (ret < 0) {
        s_tx_errors++;
    }

    if (s_tx_count >= s_tx_target) {
        uint64_t elapsed = millis() - s_tx_start_ms;
        uint32_t pps_x100 = 0, kbps_x10 = 0;
        if (elapsed > 0) {
            pps_x100 = (uint32_t)((s_tx_count * 100000ULL) / elapsed);
            kbps_x10 = (uint32_t)((s_tx_count * (uint64_t)BENCH_PKT_SIZE * 8 * 10) / elapsed);
        }
        dbg_log("lora", "bench TX done: target=%u attempts=%u errors=%u elapsed=%lu ms", s_tx_target, s_tx_count, s_tx_errors,
                (unsigned long)elapsed);
        if (elapsed > 0) {
            dbg_log("lora", "  pps: %u.%02u  kbps_raw: %u.%u", pps_x100 / 100U, pps_x100 % 100U, kbps_x10 / 10U, kbps_x10 % 10U);
        }
        s_bench_mode = BENCH_NONE;
        return -1;
    }

    uint64_t now = millis();
    if (now - s_tx_last_report_ms >= 1000) {
        s_tx_last_report_ms = now;
        dbg_log("lora", "bench TX progress: %u/%u errors=%u", s_tx_count, s_tx_target, s_tx_errors);
    }

    return 0; /* run again ASAP */
}

/* ── RX ── */

void sx126x_bench_on_rx_packet(const uint8_t *data, int16_t len, int16_t rssi, int8_t snr)
{
    (void)rssi;
    (void)snr;
    if (s_bench_mode != BENCH_RX)
        return;

    uint32_t seq;
    if (!bench_pkt_validate(data, (uint16_t)len, &seq)) {
        s_rx_corrupt++;
        return;
    }

    bench_rx_on_valid(seq, millis());
}

void sx126x_bench_rx_idle_touch(void)
{
    s_rx_idle_last_ms = millis();
}

int32_t sx126x_bench_rx_check_idle(void)
{
    if (s_bench_mode != BENCH_RX)
        return 0;

    uint64_t now = millis();
    if ((now - s_rx_idle_last_ms) >= BENCH_RX_IDLE_MS) {
        bench_rx_print_summary();
        s_bench_mode = BENCH_NONE;
        return -1;
    }
    return 10;
}

void sx126x_bench_rx_print_snap(void)
{
    if (s_bench_mode != BENCH_RX)
        return;
    dbg_log("lora", "bench RX snap: valid=%lu corrupt=%lu gap=%lu last_seq=%lu", (unsigned long)s_rx_valid,
            (unsigned long)s_rx_corrupt, (unsigned long)s_rx_gap, (unsigned long)(s_rx_have_first ? s_rx_last_seq : 0));
}

/* ================================================================== */
/* Shell command handlers                                              */
/* ================================================================== */

#if defined(PIO_ENABLE_SHELL_MODULE)

int cmd_lora_bench_tx(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora bench_tx <count>\r\n");
        return -1;
    }
    if (!sx126x_shell_is_started()) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;
    if (sx126x_shell_is_async_rx()) {
        snprintf(out, out_len, "async RX running, run 'lora recv_async_stop' first\r\n");
        return -2;
    }

    unsigned long count = strtoul(argv[2], NULL, 10);
    if (count == 0 || count > 100000) {
        snprintf(out, out_len, "count must be 1..100000\r\n");
        return -1;
    }

    /* Configure TX */
    ret = sx126x_shell_apply_config(true);
    if (ret < 0) {
        snprintf(out, out_len, "config TX fail: %d\r\n", ret);
        return ret;
    }

    s_tx_target = (uint32_t)count;
    s_tx_count = 0;
    s_tx_errors = 0;
    s_bench_mode = BENCH_TX;

    snprintf(out, out_len, "Bench TX started: %u packets (255 bytes each)\r\n", (unsigned)count);

    /* PioThread will drive TX via bench_tx_run_once() */
    extern void sx126x_wake_thread(void);
    sx126x_wake_thread();

    return 0;
}

int cmd_lora_bench_rx(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    if (!sx126x_shell_is_started()) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;
    if (sx126x_shell_is_async_rx()) {
        snprintf(out, out_len, "async RX running, run 'lora recv_async_stop' first\r\n");
        return -2;
    }

    bench_rx_reset_stats();
    s_bench_mode = BENCH_RX;

    ret = sx126x_shell_start_async_rx();
    if (ret < 0) {
        s_bench_mode = BENCH_NONE;
        snprintf(out, out_len, "start async RX fail: %d\r\n", ret);
        return ret;
    }

    s_rx_idle_last_ms = millis();
    snprintf(out, out_len, "Bench RX started (auto-stop after %d ms idle)\r\n", BENCH_RX_IDLE_MS);

    extern void sx126x_wake_thread(void);
    sx126x_wake_thread();

    return 0;
}

#endif /* PIO_ENABLE_SHELL_MODULE */

#endif /* PIO_ENABLE_LORA_RA01S */
