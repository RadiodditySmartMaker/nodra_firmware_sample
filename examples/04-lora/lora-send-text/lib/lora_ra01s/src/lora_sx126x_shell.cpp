/*
 * Layer 4a — SX126x shell command implementation.
 *
 * Ported from old lora_ra01s.cpp (RadioLib-based) — adapted to use
 * the custom SX126x driver layers (hal / config) instead of RadioLib.
 */

#include "lora_sx126x_shell.h"

#if defined(PIO_ENABLE_LORA_RA01S)

#include "lora_sx126x_bench.h"
#include "lora_sx126x_config.h"
#include "lora_sx126x_hal.h"
#include "lora_sx126x_spi.h"
#include "variant.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#if defined(PIO_ENABLE_SHELL_MODULE)
#include <shell_module.h>
#endif /* PIO_ENABLE_SHELL_MODULE */

/* ================================================================== */
/* State                                                                */
/* ================================================================== */
static bool s_started = false;
static bool s_async_rx = false;
static volatile bool s_rx_pending; /* set by DIO1 ISR */

typedef struct {
    const char *name;
    int (*fn)(int argc, char *argv[], char *out, size_t out_len);
    const char *help;
} str_shell_t;

/* Forward declarations */
#if defined(PIO_ENABLE_SHELL_MODULE)
static int cmd_lora_dispatch(int argc, char *argv[], char *out, size_t out_len);
#endif /* PIO_ENABLE_SHELL_MODULE */

/* Forward: thread wake from ISR — defined in lora_sx126x.cpp */
void sx126x_wake_thread_from_isr(void);

/* ================================================================== */
/* DIO1 ISR                                                            */
/* ================================================================== */
static void dio1_isr(void)
{
    s_rx_pending = true;
    sx126x_wake_thread_from_isr();
}

/* ================================================================== */
/* Internal helpers                                                    */
/* ================================================================== */

void sx126x_shell_hw_apply_pio_init(void)
{
    /* Reg 0x08B5 (RX sensitivity): RadioLib default is 0x05 */
    sx126x_hal_write_reg(0x08B5, (const uint8_t[]){0x05}, 1);
    /* OCP = 140 mA */
    sx126x_config_set_current_limit(140.0f);
}

int sx126x_shell_apply_config(bool tx)
{
    int state;

    state = sx126x_config_set_frequency(g_sx126x_cfg.frequency);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_preamble_length(g_sx126x_cfg.preamble_len);
    if (state != SX126X_ERR_NONE)
        return state;

    sx126x_shell_hw_apply_pio_init();

    if (tx) {
        state = sx126x_config_set_spreading_factor(g_sx126x_cfg.sf);
        if (state != SX126X_ERR_NONE)
            return state;

        state = sx126x_config_set_bandwidth(g_sx126x_cfg.bw_khz);
        if (state != SX126X_ERR_NONE)
            return state;

        state = sx126x_config_set_coding_rate(g_sx126x_cfg.cr);
        if (state != SX126X_ERR_NONE)
            return state;

        state = sx126x_config_set_output_power(g_sx126x_cfg.tx_power);
        if (state != SX126X_ERR_NONE)
            return state;
    }

    return state;
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

int sx126x_shell_start(void)
{
    int state;

    if (s_started)
        return 0;

    /* HAL init: PWR=HIGH → GPIO → SPI → HW reset → attach DIO1 IRQ */
    state = sx126x_hal_init(SX1262_CHIP_TYPE, dio1_isr);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Config init: packet type, modulation defaults, DIO2, regulator... */
    state = sx126x_config_begin_lora(g_sx126x_cfg.frequency, g_sx126x_cfg.bw_khz, g_sx126x_cfg.sf, g_sx126x_cfg.cr,
                                     g_sx126x_cfg.tx_power, g_sx126x_cfg.preamble_len);
    if (state != SX126X_ERR_NONE) {
        sx126x_hal_term();
        return state;
    }

    /* Post-init */
    sx126x_config_set_dio2_as_rf_switch(true);
    sx126x_shell_hw_apply_pio_init();
    sx126x_config_set_rx_boosted_gain(true);

    /* Standby */
    sx126x_config_standby();

    s_started = true;
    s_rx_pending = false;
    return 0;
}

int sx126x_shell_stop(void)
{
    /* Always execute full cleanup regardless of s_started state.
     * If async RX is running, stop it first. */
    if (s_async_rx) {
        sx126x_config_standby();
        s_async_rx = false;
    }

    /* HAL term: detach IRQ → SPI deinit → all pins OUTPUT LOW → PWR LOW */
    sx126x_hal_term();

    s_started = false;
    s_rx_pending = false;
    return 0;
}

int sx126x_shell_send(const uint8_t *data, size_t len)
{
    if (!s_started)
        return -1;

    int state = sx126x_shell_apply_config(true);
    if (state != SX126X_ERR_NONE)
        return state;

    return sx126x_config_transmit(data, len);
}

int sx126x_shell_recv_data(uint8_t *buf, size_t buf_len, uint32_t timeout_ms, int16_t *rssi, int8_t *snr)
{
    if (!s_started)
        return -1;

    sx126x_shell_apply_config(false);

    return sx126x_config_receive(buf, buf_len, timeout_ms, rssi, snr);
}

int sx126x_shell_start_async_rx(void)
{
    if (!s_started)
        return -1;
    if (s_async_rx)
        return 0;

    sx126x_shell_apply_config(false);

    int state = sx126x_config_start_receive();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Check for already-received packet */
    uint16_t irq = sx126x_config_get_irq_flags();
    if (irq != 0) {
        s_rx_pending = true;
    }

    s_async_rx = true;
    return 0;
}

int sx126x_shell_stop_async_rx(void)
{
    if (!s_async_rx)
        return 0;

    sx126x_config_standby();
    s_async_rx = false;
    s_rx_pending = false;
    return 0;
}

bool sx126x_shell_is_rx_pending(void)
{
    return s_rx_pending;
}

int16_t sx126x_shell_consume_rx(uint8_t *buf, size_t buf_len, int16_t *rssi, int8_t *snr)
{
    if (!s_rx_pending)
        return 0;
    s_rx_pending = false;

    int16_t ret = sx126x_config_read_data(buf, buf_len, rssi, snr);
    if (ret > 0) {
        /* Re-arm for next packet */
        sx126x_config_start_receive();
    } else if (ret == SX126X_ERR_CRC_MISMATCH) {
        /* Still got data, re-arm */
        sx126x_config_start_receive();
        return ret;
    } else {
        /* Error, re-arm anyway */
        sx126x_config_start_receive();
    }

    return ret;
}

uint32_t sx126x_shell_calc_airtime_us(uint16_t payload_len)
{
    return sx126x_config_calc_airtime_us(payload_len, &g_sx126x_cfg);
}

bool sx126x_shell_is_started(void)
{
    return s_started;
}

bool sx126x_shell_is_async_rx(void)
{
    return s_async_rx;
}

/* ================================================================== */
/* Shell commands                                                      */
/* ================================================================== */

#if defined(PIO_ENABLE_SHELL_MODULE)

/* Forward declarations for bench */
extern int sx126x_bench_reject_if_busy(char *out, size_t out_len);

/* ── Subcommand implementations ── */

static int cmd_start(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int ret = sx126x_shell_start();
    if (ret == 0) {
        snprintf(out, out_len, "LoRa started (%.0f MHz, SF%d, BW%.0f, CR4/%d, %d dBm, preamble=%u)\r\n", g_sx126x_cfg.frequency,
                 g_sx126x_cfg.sf, g_sx126x_cfg.bw_khz, g_sx126x_cfg.cr, g_sx126x_cfg.tx_power, g_sx126x_cfg.preamble_len);
    } else {
        snprintf(out, out_len, "LoRa start failed: %d\r\n", ret);
    }
    return ret;
}

static int cmd_stop(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int ret = sx126x_shell_stop();
    if (ret == 0) {
        snprintf(out, out_len, "LoRa stopped\r\n");
    } else {
        snprintf(out, out_len, "LoRa stop failed: %d\r\n", ret);
    }
    return ret;
}

static int cmd_send(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora send <text>\r\n");
        return -1;
    }
    if (!s_started) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;
    if (s_async_rx) {
        snprintf(out, out_len, "async RX running, run 'lora recv_async_stop' first\r\n");
        return -2;
    }

    size_t len = strlen(argv[2]);
    ret = sx126x_shell_send((const uint8_t *)argv[2], len);
    if (ret == SX126X_ERR_NONE) {
        snprintf(out, out_len, "Sent %u bytes: %s\r\n", (unsigned)len, argv[2]);
    } else {
        snprintf(out, out_len, "Send fail: %d\r\n", ret);
    }
    return (ret == SX126X_ERR_NONE) ? 0 : ret;
}

static int cmd_recv(int argc, char *argv[], char *out, size_t out_len)
{
    uint32_t timeout_ms = 5000;
    if (!s_started) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;
    if (s_async_rx) {
        snprintf(out, out_len, "async RX running, run 'lora recv_async_stop' first\r\n");
        return -2;
    }
    if (argc >= 3)
        timeout_ms = (uint32_t)atoi(argv[2]);

    int16_t rssi;
    int8_t snr;
    uint8_t buf[64];

    dbg_print("Waiting for packet (timeout: %u ms)...", (unsigned)timeout_ms);

    ret = sx126x_shell_recv_data(buf, sizeof(buf) - 1, timeout_ms, &rssi, &snr);
    if (ret == SX126X_ERR_RX_TIMEOUT) {
        dbg_print("recv timeout (nothing heard in %u ms)", (unsigned)timeout_ms);
    } else if (ret < 0) {
        dbg_log("lora", "recv error: %d", ret);
    } else {
        size_t actual_len = sx126x_config_get_packet_length();
        if (actual_len >= sizeof(buf))
            actual_len = sizeof(buf) - 1;
        buf[actual_len] = '\0';
        dbg_log("lora", "RX %u bytes: %s  (RSSI: %d dBm, SNR: %d dB)", (unsigned)actual_len, (char *)buf, rssi, snr);
    }
    out[0] = '\0';
    return 0;
}

static int cmd_recv_async(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    if (!s_started) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;

    ret = sx126x_shell_start_async_rx();
    if (ret == 0) {
        snprintf(out, out_len, "Async RX started\r\n");
    } else {
        snprintf(out, out_len, "Async RX start fail: %d\r\n", ret);
    }
    return ret;
}

static int cmd_recv_async_stop(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    if (!s_async_rx) {
        snprintf(out, out_len, "Async RX not running\r\n");
        return 0;
    }
    int ret = sx126x_shell_stop_async_rx();
    if (ret == 0) {
        snprintf(out, out_len, "Async RX stopped\r\n");
    } else {
        snprintf(out, out_len, "Async RX stop fail: %d\r\n", ret);
    }
    return ret;
}

static int cmd_status(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    snprintf(out, out_len,
             "status      : %s\r\n"
             "frequency   : %.0f MHz\r\n"
             "bandwidth   : %.0f kHz\r\n"
             "SF          : SF%d\r\n"
             "coding rate : CR4/%d\r\n"
             "tx power    : %d dBm\r\n"
             "preamble    : %u\r\n",
             s_started ? "started" : "stopped", g_sx126x_cfg.frequency, g_sx126x_cfg.bw_khz, g_sx126x_cfg.sf, g_sx126x_cfg.cr,
             g_sx126x_cfg.tx_power, g_sx126x_cfg.preamble_len);
    return 0;
}

static int cmd_config(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 4) {
        snprintf(out, out_len,
                 "usage: lora config <key> <val>\r\n"
                 "  keys: freq (MHz), sf (5-12), bw (125/250/500), cr (5-8), power (-17..22)\r\n");
        return -1;
    }
    const char *key = argv[2];
    float val = (float)atof(argv[3]);

    if (strcmp(key, "freq") == 0) {
        if (val < 150.0f || val > 960.0f) {
            snprintf(out, out_len, "freq must be 150-960 MHz\r\n");
            return -1;
        }
        g_sx126x_cfg.frequency = val;
    } else if (strcmp(key, "sf") == 0) {
        uint8_t sf = (uint8_t)val;
        if (sf < 5 || sf > 12) {
            snprintf(out, out_len, "SF must be 5-12\r\n");
            return -1;
        }
        g_sx126x_cfg.sf = sf;
    } else if (strcmp(key, "bw") == 0) {
        if (val != 125.0f && val != 250.0f && val != 500.0f) {
            snprintf(out, out_len, "BW must be 125, 250, or 500 kHz\r\n");
            return -1;
        }
        g_sx126x_cfg.bw_khz = val;
    } else if (strcmp(key, "cr") == 0) {
        uint8_t cr = (uint8_t)val;
        if (cr < 5 || cr > 8) {
            snprintf(out, out_len, "CR must be 5-8 (4/5 .. 4/8)\r\n");
            return -1;
        }
        g_sx126x_cfg.cr = cr;
    } else if (strcmp(key, "power") == 0) {
        int8_t p = (int8_t)val;
        if (p < -17 || p > 22) {
            snprintf(out, out_len, "power must be -17..22 dBm\r\n");
            return -1;
        }
        g_sx126x_cfg.tx_power = p;
    } else {
        snprintf(out, out_len, "unknown key: %s\r\n", key);
        return -1;
    }

    if (s_started) {
        int ret = sx126x_shell_apply_config(true);
        if (ret != SX126X_ERR_NONE) {
            snprintf(out, out_len, "config applied but set failed: %d\r\n", ret);
            return ret;
        }
    }
    snprintf(out, out_len, "%s = %.0f\r\n", key, val);
    return 0;
}

static int cmd_setpower(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora setpower <-17..22>\r\n");
        return -1;
    }
    int8_t power = (int8_t)atoi(argv[2]);
    if (power < -17 || power > 22) {
        snprintf(out, out_len, "power must be -17..22 dBm\r\n");
        return -1;
    }
    g_sx126x_cfg.tx_power = power;
    if (s_started) {
        int ret = sx126x_shell_apply_config(true);
        if (ret != SX126X_ERR_NONE) {
            snprintf(out, out_len, "setpower fail: %d\r\n", ret);
            return ret;
        }
    }
    snprintf(out, out_len, "TX power set to %d dBm%s\r\n", power, s_started ? "" : " (applied on next start)");
    return 0;
}

static int cmd_cw(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora cw <sec> [power]\r\n");
        return -1;
    }
    if (!s_started) {
        snprintf(out, out_len, "LoRa not started, run 'lora start' first\r\n");
        return -1;
    }
    int ret = sx126x_bench_reject_if_busy(out, out_len);
    if (ret < 0)
        return ret;

    uint16_t duration_s = (uint16_t)atoi(argv[2]);
    int8_t power = (argc >= 4) ? (int8_t)atoi(argv[3]) : g_sx126x_cfg.tx_power;
    if (duration_s == 0 || duration_s > 60) {
        snprintf(out, out_len, "duration must be 1..60 s\r\n");
        return -1;
    }

    ret = sx126x_shell_apply_config(true);
    if (ret != SX126X_ERR_NONE) {
        snprintf(out, out_len, "config fail: %d\r\n", ret);
        return ret;
    }

    dbg_print("CW: %.0f Hz, %d dBm, %u s...", g_sx126x_cfg.frequency, power, duration_s);

    /* Carrier wave via repeated transmits */
    uint8_t cw_buf[32] = {0x55};
    uint32_t start = millis();
    while ((millis() - start) < duration_s * 1000UL) {
        sx126x_config_transmit(cw_buf, sizeof(cw_buf));
    }

    sx126x_config_standby();
    sx126x_shell_apply_config(false);

    dbg_print("CW done");
    out[0] = '\0';
    return 0;
}

static int cmd_airtime(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora airtime <bytes>\r\n");
        return -1;
    }
    uint32_t bytes = (uint32_t)atoi(argv[2]);
    if (bytes == 0 || bytes > 255) {
        snprintf(out, out_len, "bytes must be 1..255\r\n");
        return -1;
    }
    uint32_t air_us = sx126x_shell_calc_airtime_us((uint16_t)bytes);
    uint32_t air_ms = (air_us + 999) / 1000;
    snprintf(out, out_len, "Airtime for %u bytes: %u ms (%u us)\r\n", (unsigned)bytes, (unsigned)air_ms, (unsigned)air_us);
    return 0;
}

/* ================================================================== */
/* Test subcommands — HAL layer diagnostics                           */
/* ================================================================== */

static int cmd_test_power_on(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    if (s_started) {
        snprintf(out, out_len, "already started (use 'lora test power off' first)\r\n");
        return -1;
    }

    /* Full HAL init without DIO1 IRQ (test mode) */
    int16_t ret = sx126x_hal_init(SX1262_CHIP_TYPE, NULL);
    if (ret != SX126X_ERR_NONE) {
        snprintf(out, out_len, "Power ON failed: %d\r\n", ret);
        return ret;
    }

    snprintf(out, out_len, "Power ON, GPIO/SPI init OK\r\n");
    return 0;
}

static int cmd_test_power_off(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    sx126x_hal_term();

    snprintf(out, out_len, "Power OFF, all pins OUTPUT LOW\r\n");
    return 0;
}

static int cmd_test_nop(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    /* Send NOP command, read status */
    uint8_t status;
    sx126x_hal_wait_on_busy();
    sx126x_spi_assert_cs();
    uint8_t tx = SX126X_CMD_NOP, rx;
    sx126x_spi_transfer(&tx, &rx, 1);
    sx126x_spi_release_cs();

    status = rx;
    int16_t ret = sx126x_hal_parse_status(status);

    uint8_t chip_mode = status >> 5;
    const char *mode_str = "unknown";
    switch (chip_mode) {
    case 1:
        mode_str = "STDBY_RC";
        break;
    case 2:
    case 3:
        mode_str = "STDBY_XOSC";
        break;
    case 4:
        mode_str = "FS";
        break;
    case 5:
        mode_str = "RX";
        break;
    case 6:
        mode_str = "TX";
        break;
    }

    snprintf(out, out_len, "NOP response: status=0x%02x  parsed=%d  chip_mode=%s\r\n", status, ret, mode_str);
    return (ret == SX126X_ERR_NONE) ? 0 : ret;
}

static int cmd_test_version(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    uint8_t ver[16];
    memset(ver, 0, sizeof(ver));
    int16_t ret = sx126x_hal_read_reg(SX126X_REG_VERSION_STRING, ver, 16);
    if (ret != SX126X_ERR_NONE) {
        snprintf(out, out_len, "read version fail: %d\r\n", ret);
        return ret;
    }

    /* Ensure null-terminated */
    ver[15] = '\0';

    snprintf(out, out_len, "Version: %s\r\n", (const char *)ver);
    return 0;
}

static int cmd_test_reg(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        snprintf(out, out_len, "usage: lora test reg <hex_addr> [len]\r\n");
        return -1;
    }

    uint32_t addr = strtoul(argv[2], NULL, 16);
    uint32_t len = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 1;
    if (addr > 0xFFFF || len > 64) {
        snprintf(out, out_len, "addr 0-0xFFFF, len 1-64\r\n");
        return -1;
    }

    uint8_t buf[64];
    int16_t ret = sx126x_hal_read_reg((uint16_t)addr, buf, (uint8_t)len);
    if (ret != SX126X_ERR_NONE) {
        snprintf(out, out_len, "read reg 0x%04X fail: %d\r\n", (unsigned)addr, ret);
        return ret;
    }

    /* Format output */
    size_t pos = 0;
    for (uint32_t i = 0; i < len && pos < out_len - 8; i++) {
        pos += (size_t)snprintf(out + pos, out_len - pos, "%02x ", buf[i]);
    }
    if (pos < out_len)
        out[pos] = '\0';

    return 0;
}

static int cmd_test_irq(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    uint16_t irq = sx126x_hal_get_irq_flags();
    snprintf(out, out_len, "IRQ flags: 0x%04x\r\n", irq);
    return 0;
}

static int cmd_test_errors(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    int16_t err = sx126x_hal_get_device_errors();
    if (err < 0) {
        snprintf(out, out_len, "get device errors fail: %d\r\n", err);
        return err;
    }

    snprintf(out, out_len, "Device errors: 0x%04x\r\n", (unsigned)err);
    return 0;
}

static int cmd_test_busy(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int level = digitalRead(PIN_LORA_BUSY);
    snprintf(out, out_len, "BUSY pin: %s\r\n", level ? "HIGH (busy)" : "LOW (ready)");
    return 0;
}

static int cmd_test_rx(int argc, char *argv[], char *out, size_t out_len)
{
    uint32_t timeout_ms = 5000;
    if (argc >= 3)
        timeout_ms = (uint32_t)atoi(argv[2]);
    if (timeout_ms < 100 || timeout_ms > 60000) {
        snprintf(out, out_len, "timeout 100-60000 ms\r\n");
        return -1;
    }

    /* Use the full init path (must have done 'lora test power on' first) */
    int16_t state;

    /* Check if already started */
    bool was_started = s_started;
    if (!was_started) {
        snprintf(out, out_len, "not started, run 'lora start' first\r\n");
        return -1;
    }

    /* ── Configure for receive ── */
    /* Set frequency and preamble */
    state = sx126x_shell_apply_config(false);
    if (state != SX126X_ERR_NONE) {
        snprintf(out, out_len, "apply_config fail: %d\r\n", state);
        return state;
    }

    /* Standby */
    state = sx126x_config_standby();
    if (state != SX126X_ERR_NONE) {
        snprintf(out, out_len, "standby fail: %d\r\n", state);
        return state;
    }

    /* Set packet params */
    state = sx126x_config_set_packet_params(g_sx126x_cfg.crc_enabled, false, 0xFF, false);
    if (state != SX126X_ERR_NONE) {
        snprintf(out, out_len, "set_packet_params fail: %d\r\n", state);
        return state;
    }

    /* Set DIO1 IRQ: RX_DONE + TIMEOUT, only RX_DONE mapped to DIO1 */
    state =
        sx126x_config_set_dio_irq_params(SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERR | SX126X_IRQ_TIMEOUT, SX126X_IRQ_RX_DONE, 0, 0);
    if (state != SX126X_ERR_NONE) {
        snprintf(out, out_len, "set_dio_irq fail: %d\r\n", state);
        return state;
    }

    /* Buffer base address */
    state = sx126x_config_set_buffer_base_address();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Clear IRQ */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Fix sensitivity */
    sx126x_config_fix_sensitivity();

    /* ── Enter RX mode ── */
    uint32_t timeout_raw = (uint32_t)((float)timeout_ms * 1000.0f / 15.625f);
    uint8_t rx_data[3] = {(uint8_t)((timeout_raw >> 16) & 0xFF), (uint8_t)((timeout_raw >> 8) & 0xFF),
                          (uint8_t)(timeout_raw & 0xFF)};

    state = sx126x_hal_cmd(SX126X_CMD_SET_RX, rx_data, 3);
    if (state != SX126X_ERR_NONE) {
        snprintf(out, out_len, "SetRx fail: %d\r\n", state);
        return state;
    }

    /* ── Poll DIO1 ── */
    dbg_print("RX: waiting %u ms, DIO1=%d BUSY=%d ...", (unsigned)timeout_ms, digitalRead(PIN_LORA_DIO1),
              digitalRead(PIN_LORA_BUSY));

    uint32_t start = millis();
    bool dio1_fired = false;
    while ((millis() - start) < timeout_ms + 100) {
        if (digitalRead(PIN_LORA_DIO1)) {
            dio1_fired = true;
            dbg_print("RX: DIO1 fired at %lu ms", (unsigned long)(millis() - start));
            break;
        }
        /* Print DIO1/BUSY state every second */
        if ((millis() - start) % 1000 < 10) {
            dbg_print("RX: t=%lu DIO1=%d BUSY=%d", (unsigned long)(millis() - start), digitalRead(PIN_LORA_DIO1),
                      digitalRead(PIN_LORA_BUSY));
        }
        delay(10);
    }

    /* ── Check result ── */
    state = sx126x_config_standby();

    uint16_t irq = sx126x_hal_get_irq_flags();
    uint8_t pkt_len = 0, offset = 0;
    sx126x_hal_get_rx_buffer_status(&pkt_len, &offset);

    if (dio1_fired && pkt_len > 0) {
        /* Read packet */
        uint8_t buf[64];
        int16_t rssi;
        int8_t snr;
        int16_t ret = sx126x_config_read_data(buf, sizeof(buf), &rssi, &snr);
        if (ret > 0) {
            out[0] = '\0';
            size_t pos = 0;
            for (int16_t i = 0; i < ret && pos < out_len - 4; i++) {
                pos += (size_t)snprintf(out + pos, out_len - pos, "%02x ", buf[i]);
            }
            dbg_print("RX: got %d bytes RSSI=%d SNR=%d", ret, rssi, snr);
        } else {
            dbg_print("RX: read_data ret=%d", ret);
        }
    }

    snprintf(out + strlen(out), out_len - strlen(out), "RX done: DIO1=%s pkt_len=%u IRQ=0x%04x\r\n",
             dio1_fired ? "fired" : "timeout", (unsigned)pkt_len, (unsigned)irq);

    /* Clear IRQ and re-arm */
    sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);

    return 0;
}

/* ================================================================== */
/* TX diagnostic — full config + send, check chip mode after SetTx   */
/* ================================================================== */
static int cmd_test_tx(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    if (!s_started) {
        snprintf(out, out_len, "not started, run 'lora start' first\r\n");
        return -1;
    }

    int16_t state;
    uint8_t mode_byte;

    /* Full apply_config to set frequency, modulation, power, etc. */
    dbg_print("TX: apply_config...");
    state = sx126x_shell_apply_config(true);
    dbg_print("TX: apply_config=%d", state);

    /* Standby then enter TX via config_transmit's setup */
    dbg_print("TX: standby...");
    state = sx126x_config_standby();
    dbg_print("TX: standby=%d", state);

    /* Set packet params */
    state = sx126x_config_set_packet_params(g_sx126x_cfg.crc_enabled, false, 3, false);
    dbg_print("TX: set_packet_params=%d", state);

    /* Set DIO IRQ */
    state = sx126x_config_set_dio_irq_params(SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT, SX126X_IRQ_TX_DONE, 0, 0);
    dbg_print("TX: set_dio_irq=%d", state);

    /* Buffer base + write data */
    sx126x_config_set_buffer_base_address();
    sx126x_hal_write_buffer(0, (const uint8_t *)"TX!", 3);

    /* Clear IRQ + fix sensitivity */
    sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    sx126x_config_fix_sensitivity();

    /* Check pre-Tx mode */
    sx126x_hal_cmd_read(SX126X_CMD_GET_STATUS, &mode_byte, 1);
    dbg_print("TX: pre-Tx mode=0x%02x", mode_byte);

    /* SetTx no timeout */
    dbg_print("TX: SetTx...");
    state = sx126x_hal_cmd(SX126X_CMD_SET_TX, (const uint8_t[]){0x00, 0x00, 0x00}, 3);
    dbg_print("TX: SetTx=%d BUSY=%d", state, digitalRead(PIN_LORA_BUSY));

    /* Wait and poll mode — chip might need XOSC/PLL settling time */
    for (int i = 0; i < 5; i++) {
        delay(10);
        sx126x_hal_cmd_read(SX126X_CMD_GET_STATUS, &mode_byte, 1);
        dbg_print("TX: t=%dms mode=0x%02x BUSY=%d DIO1=%d", i * 10, mode_byte, digitalRead(PIN_LORA_BUSY),
                  digitalRead(PIN_LORA_DIO1));
    }

    /* If still not TX, try SetFs then SetTx */
    if ((mode_byte & 0x70) != 0x60) {
        dbg_print("TX: not in TX mode, trying SetFs then SetTx...");
        sx126x_hal_cmd(SX126X_CMD_SET_FS, NULL, 0);
        delay(10);
        sx126x_hal_cmd_read(SX126X_CMD_GET_STATUS, &mode_byte, 1);
        dbg_print("TX: after SetFs mode=0x%02x", mode_byte);

        sx126x_hal_cmd(SX126X_CMD_SET_TX, (const uint8_t[]){0x00, 0x00, 0x00}, 3);
        delay(10);
        sx126x_hal_cmd_read(SX126X_CMD_GET_STATUS, &mode_byte, 1);
        dbg_print("TX: after retry mode=0x%02x", mode_byte);
    }

    /* Poll DIO1 for 3s */
    uint32_t start = millis();
    bool fired = false;
    while ((millis() - start) < 3000) {
        if (digitalRead(PIN_LORA_DIO1)) {
            fired = true;
            break;
        }
        delay(10);
    }

    /* Report */
    uint16_t irq = sx126x_hal_get_irq_flags();
    sx126x_hal_cmd_read(SX126X_CMD_GET_STATUS, &mode_byte, 1);
    sx126x_config_standby();

    dbg_print("TX: DIO1=%s IRQ=0x%04x final_mode=0x%02x", fired ? "fired" : "timeout", irq, mode_byte);
    snprintf(out, out_len, "TX done: DIO1=%s IRQ=0x%04x\r\n", fired ? "fired" : "timeout", irq);
    return fired ? 0 : -1;
}

static const str_shell_t lora_test_subcommands[] = {
    {"power_on", cmd_test_power_on, "Power ON + hal_init"},
    {"power_off", cmd_test_power_off, "hal_term + power OFF"},
    {"nop", cmd_test_nop, "Send NOP, check status"},
    {"version", cmd_test_version, "Read chip version string"},
    {"reg", cmd_test_reg, "Read register (hex addr, optional len)"},
    {"irq", cmd_test_irq, "Read IRQ flags"},
    {"errors", cmd_test_errors, "Read device errors"},
    {"busy", cmd_test_busy, "Read BUSY pin level"},
    {"rx", cmd_test_rx, "Enter RX mode, poll DIO1, report IRQ"},
    {"tx", cmd_test_tx, "Minimal TX, report chip mode and DIO1"},
    {NULL, NULL, NULL},
};

static int cmd_test_dispatch(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 3) {
        dbg_print("usage: lora test <sub>:");
        for (int i = 0; lora_test_subcommands[i].name; i++) {
            dbg_print("  %s - %s", lora_test_subcommands[i].name, lora_test_subcommands[i].help);
        }
        out[0] = '\0';
        return 0;
    }

    const char *sub = argv[2];

    for (int i = 0; lora_test_subcommands[i].name; i++) {
        if (strcmp(sub, lora_test_subcommands[i].name) == 0) {
            return lora_test_subcommands[i].fn(argc - 1, argv + 1, out, out_len);
        }
    }

    snprintf(out, out_len, "unknown test: %s\r\n", sub);
    return -1;
}

/* ── Dispatch table ── */

static const str_shell_t lora_subcommands[] = {
    {"start", cmd_start, "Power on and init SX1262"},
    {"stop", cmd_stop, "Stop and power off"},
    {"send", cmd_send, "Send data: lora send <text>"},
    {"recv", cmd_recv, "Blocking receive: lora recv [timeout_ms]"},
    {"recv_async", cmd_recv_async, "Start async receive (DIO1 IRQ + PioThread)"},
    {"recv_async_stop", cmd_recv_async_stop, "Stop async receive"},
    {"status", cmd_status, "Print current config"},
    {"config", cmd_config, "Set param: lora config <key> <val>"},
    {"setpower", cmd_setpower, "Set TX power: lora setpower <-17..22>"},
    {"cw", cmd_cw, "Carrier wave: lora cw <sec> [power]"},
    {"airtime", cmd_airtime, "Calculate airtime: lora airtime <bytes>"},
    {"test", cmd_test_dispatch, "HAL diagnostics: test power/nop/version/reg/irq/errors/busy"},
    {"bench_tx", cmd_lora_bench_tx, "Bench TX: lora bench_tx <count>"},
    {"bench_rx", cmd_lora_bench_rx, "Bench RX: lora bench_rx"},
    {NULL, NULL, NULL},
};

static int cmd_lora_dispatch(int argc, char *argv[], char *out, size_t out_len)
{
    if (argc < 2) {
        dbg_print("LoRa subcommands:");
        for (int i = 0; lora_subcommands[i].name; i++) {
            dbg_print("  %s - %s", lora_subcommands[i].name, lora_subcommands[i].help);
        }
        out[0] = '\0';
        return 0;
    }

    for (int i = 0; lora_subcommands[i].name; i++) {
        if (strcmp(argv[1], lora_subcommands[i].name) == 0) {
            return lora_subcommands[i].fn(argc, argv, out, out_len);
        }
    }

    snprintf(out, out_len, "unknown lora subcommand: %s\r\n", argv[1]);
    return -1;
}

void sx126x_shell_init(void)
{
    shell_register_cmd("lora", cmd_lora_dispatch,
                       "LoRa RA-01S control: "
                       "start|stop|send|recv|recv_async|recv_async_stop|"
                       "status|config|setpower|cw|airtime|bench_tx|bench_rx");
}

#else /* PIO_ENABLE_SHELL_MODULE */

void sx126x_shell_init(void) {}

#endif /* PIO_ENABLE_SHELL_MODULE */

#endif /* PIO_ENABLE_LORA_RA01S */
