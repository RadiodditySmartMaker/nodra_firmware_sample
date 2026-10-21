/*
 * Layer 2 — SX126x register / buffer / command SPI access.
 *
 * Implements the SX126x stream-type SPI protocol:
 *   [cmd] [addr_hi/status] [addr_lo/data...] [data...]
 *
 * Busy pin is checked before/after each transaction.
 */

#include "lora_sx126x_hal.h"
#include "lora_sx126x_spi.h"

#include "variant.h"
#include <Arduino.h>
#include <string.h>

#if defined(PIO_ENABLE_LORA_RA01S)

/* ── Timeout for busy pin wait (milliseconds) ── */
#define SX126X_BUSY_TIMEOUT_MS 100

/* ── Debug logging (enable via build_flags: -D SX126X_DEBUG) ── */
// #define SX126X_DEBUG
#if defined(SX126X_DEBUG) && defined(PIO_ENABLE_SHELL_MODULE)
#include "shell_module.h"
static char _hex_buf[256];
#define SX126X_DBG_PRINTF(...) dbg_print(__VA_ARGS__)
#define SX126X_DBG_HEX(s, l, f)                                                                                                  \
    do {                                                                                                                         \
        int _offset = 0;                                                                                                         \
        size_t _print_len = (l) > 80 ? 80 : (l);                                                                                 \
        _offset += snprintf(_hex_buf + _offset, sizeof(_hex_buf) - _offset, "%s (%u bytes):", (f), (unsigned)_print_len);        \
        for (size_t _i = 0; _i < _print_len; _i++) {                                                                             \
            _offset += snprintf(_hex_buf + _offset, sizeof(_hex_buf) - _offset, " %02x", (s)[_i]);                               \
        }                                                                                                                        \
        dbg_print("%s", _hex_buf);                                                                                               \
    } while (0)
#else
#define SX126X_DBG_PRINTF(...) ((void)0)
#define SX126X_DBG_HEX(s, l, f) ((void)0)
#endif

/* ================================================================== */
/* Internal: wait for BUSY pin to go low                              */
/* ================================================================== */

void sx126x_hal_wait_on_busy(void)
{
    uint32_t start = millis();
    while (digitalRead(PIN_LORA_BUSY)) {
        if ((millis() - start) >= SX126X_BUSY_TIMEOUT_MS) {
            SX126X_DBG_PRINTF("sx126x: BUSY timeout after %lu ms", (unsigned long)SX126X_BUSY_TIMEOUT_MS);
            break;
        }
    }
}

/* ================================================================== */
/* Internal: single SPI transaction (CS managed)                      */
/* ================================================================== */

/*
 * Perform an SPI transaction in SX126x stream mode.
 *
 * For READ transactions (read_data != NULL):
 *   MOSI: [cmd...] [NOP] [NOP...]     (status gap + N data bytes)
 *   MISO: [cmd...] [status] [data...]
 *   total = cmd_len + 1(status_gap) + data_len
 *
 * For WRITE transactions (read_data == NULL):
 *   MOSI: [cmd...] [write_data...]
 *   MISO: [cmd...] [status...]
 *   total = cmd_len + data_len  (no status gap, status embedded in response)
 */
static void spi_transact(const uint8_t *cmd_buf, uint8_t cmd_len, const uint8_t *write_data, uint8_t *read_data,
                         uint16_t data_len, uint8_t *status_out)
{
    bool read_mode = (read_data != NULL);
    uint16_t status_gap = read_mode ? 1 : 0;
    uint16_t total = cmd_len + status_gap + data_len;
    uint8_t tx_buf[total];
    uint8_t rx_buf[total];

    memset(tx_buf, SX126X_CMD_NOP, total);
    memcpy(tx_buf, cmd_buf, cmd_len);

    if (write_data && data_len > 0) {
        /* For writes: data starts right after cmd bytes.
         * For reads with status gap: data starts after cmd + status_gap. */
        memcpy(&tx_buf[cmd_len + status_gap], write_data, data_len);
    }

    sx126x_spi_assert_cs();
    sx126x_spi_transfer(tx_buf, rx_buf, total);
    sx126x_spi_release_cs();

    /* Status is at index cmd_len (right after command bytes in RX buffer) */
    if (status_out) {
        *status_out = rx_buf[cmd_len];
    }

    if (read_data && data_len > 0) {
        /* Read data starts after cmd + status_gap */
        memcpy(read_data, &rx_buf[cmd_len + status_gap], data_len);
    }

    SX126X_DBG_HEX(tx_buf, total, "SPI TX");
    SX126X_DBG_HEX(rx_buf, total, "SPI RX");
}

/* ================================================================== */
/* Internal: send simple command with optional data, no response data */
/* ================================================================== */

static int16_t cmd_simple(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[1] = {cmd};
    uint8_t status;
    spi_transact(cmd_buf, 1, data, NULL, data_len, &status);

    /* Wait for BUSY to go LOW after the command executes */
    sx126x_hal_wait_on_busy();

    /* RadioLib default: ignore CMD_FAILED/CMD_TIMEOUT status bits.
     * Only detect chip-not-found (0x00 or 0xFF).
     * The status byte is logged via SX126X_DEBUG but not fatal. */
    if (status == 0x00 || status == 0xFF) {
        return SX126X_ERR_CHIP_NOT_FOUND;
    }
    return SX126X_ERR_NONE;
}

/* ================================================================== */
/* Internal: send command and read back response bytes                */
/* ================================================================== */

static int16_t cmd_read(uint8_t cmd, uint8_t *resp, uint8_t resp_len)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[1] = {cmd};
    uint8_t status;
    spi_transact(cmd_buf, 1, NULL, resp, resp_len, &status);

    /* Same relaxed check as cmd_simple */
    if (status == 0x00 || status == 0xFF) {
        return SX126X_ERR_CHIP_NOT_FOUND;
    }
    return SX126X_ERR_NONE;
}

/* ================================================================== */
/* Public API                                                         */
/* ================================================================== */

int16_t sx126x_hal_init(const char *chip_type, void (*dio1_cb)(void))
{
    (void)chip_type;

    /* Power on */
    pinMode(PIN_LORA_PWR, OUTPUT);
    digitalWrite(PIN_LORA_PWR, HIGH);
    delay(10);

    /* GPIO init */
    pinMode(PIN_LORA_RST, OUTPUT);
    digitalWrite(PIN_LORA_RST, HIGH);
    pinMode(PIN_LORA_BUSY, INPUT);
    pinMode(PIN_LORA_DIO1, INPUT);
    pinMode(PIN_LORA_DIO3, INPUT); /* connected but unused — avoid interference */

    /* SPI init (includes CS) */
    sx126x_spi_init();

    /* Hardware reset sequence */
    digitalWrite(PIN_LORA_RST, LOW);
    delay(1);
    digitalWrite(PIN_LORA_RST, HIGH);

    /* Wait for module to stabilise */
    sx126x_hal_wait_on_busy();
    delay(10);

    /* Attach DIO1 interrupt if callback provided */
    if (dio1_cb != NULL) {
        attachInterrupt(digitalPinToInterrupt(PIN_LORA_DIO1), dio1_cb, RISING);
    }

    return SX126X_ERR_NONE;
}

void sx126x_hal_term(void)
{
    /* Detach DIO1 interrupt */
    detachInterrupt(digitalPinToInterrupt(PIN_LORA_DIO1));

    /* Module control pins → OUTPUT LOW to prevent leakage */
    pinMode(PIN_LORA_RST, OUTPUT);
    digitalWrite(PIN_LORA_RST, LOW);
    pinMode(PIN_LORA_BUSY, OUTPUT);
    digitalWrite(PIN_LORA_BUSY, LOW);
    pinMode(PIN_LORA_DIO1, OUTPUT);
    digitalWrite(PIN_LORA_DIO1, LOW);
    pinMode(PIN_LORA_DIO3, OUTPUT);
    digitalWrite(PIN_LORA_DIO3, LOW);

    /* SPI pins → OUTPUT LOW (via spi_deinit) */
    sx126x_spi_deinit();

    /* Power off */
    pinMode(PIN_LORA_PWR, OUTPUT);
    digitalWrite(PIN_LORA_PWR, LOW);
}

/* ================================================================== */
/* Register access                                                     */
/* ================================================================== */

int16_t sx126x_hal_read_reg(uint16_t addr, uint8_t *buf, uint8_t num_bytes)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[3] = {SX126X_CMD_READ_REGISTER, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    uint8_t status;
    spi_transact(cmd_buf, 3, NULL, buf, num_bytes, &status);

    /* RadioLib always returns ERR_NONE for register read/write,
     * as some registers may set error flags even when data is valid.
     * Status is only strictly checked for commands (cmd_simple). */
    return SX126X_ERR_NONE;
}

int16_t sx126x_hal_write_reg(uint16_t addr, const uint8_t *buf, uint8_t num_bytes)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[3] = {SX126X_CMD_WRITE_REGISTER, (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF)};
    uint8_t status;
    spi_transact(cmd_buf, 3, buf, NULL, num_bytes, &status);

    /* RadioLib: always return success for register write */
    return SX126X_ERR_NONE;
}

/* ================================================================== */
/* Buffer (FIFO) access                                                */
/* ================================================================== */

int16_t sx126x_hal_read_buffer(uint8_t offset, uint8_t *buf, uint8_t num_bytes)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[2] = {SX126X_CMD_READ_BUFFER, offset};
    uint8_t status;
    spi_transact(cmd_buf, 2, NULL, buf, num_bytes, &status);
    return sx126x_hal_parse_status(status);
}

int16_t sx126x_hal_write_buffer(uint8_t offset, const uint8_t *buf, uint8_t num_bytes)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[2] = {SX126X_CMD_WRITE_BUFFER, offset};
    uint8_t status;
    spi_transact(cmd_buf, 2, buf, NULL, num_bytes, &status);
    return sx126x_hal_parse_status(status);
}

/* ================================================================== */
/* Generic commands                                                    */
/* ================================================================== */

int16_t sx126x_hal_cmd(uint8_t cmd, const uint8_t *data, uint8_t data_len)
{
    return cmd_simple(cmd, data, data_len);
}

int16_t sx126x_hal_cmd_read(uint8_t cmd, uint8_t *resp, uint8_t resp_len)
{
    return cmd_read(cmd, resp, resp_len);
}

/* ================================================================== */
/* IRQ flags                                                           */
/* ================================================================== */

uint16_t sx126x_hal_get_irq_flags(void)
{
    sx126x_hal_wait_on_busy();

    uint8_t cmd_buf[1] = {SX126X_CMD_GET_IRQ_STATUS};
    uint8_t rx[2];
    uint8_t status;
    spi_transact(cmd_buf, 1, NULL, rx, 2, &status);

    return ((uint16_t)rx[0] << 8) | rx[1];
}

int16_t sx126x_hal_clear_irq_flags(uint16_t mask)
{
    uint8_t data[2] = {(uint8_t)(mask >> 8), (uint8_t)(mask & 0xFF)};
    return cmd_simple(SX126X_CMD_CLEAR_IRQ_STATUS, data, 2);
}

/* ================================================================== */
/* Packet status helpers                                               */
/* ================================================================== */

int16_t sx126x_hal_get_rx_buffer_status(uint8_t *payload_len, uint8_t *offset)
{
    uint8_t data[2];
    int16_t ret = cmd_read(SX126X_CMD_GET_RX_BUFFER_STATUS, data, 2);
    if (ret == SX126X_ERR_NONE) {
        if (payload_len)
            *payload_len = data[0];
        if (offset)
            *offset = data[1];
    }
    return ret;
}

uint32_t sx126x_hal_get_packet_status(void)
{
    uint8_t data[3];
    int16_t ret = cmd_read(SX126X_CMD_GET_PACKET_STATUS, data, 3);
    if (ret == SX126X_ERR_NONE) {
        return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
    }
    return 0;
}

int16_t sx126x_hal_get_device_errors(void)
{
    uint8_t data[2];
    int16_t ret = cmd_read(SX126X_CMD_GET_DEVICE_ERRORS, data, 2);
    if (ret == SX126X_ERR_NONE) {
        return ((int16_t)data[0] << 8) | data[1];
    }
    return ret;
}

/* ================================================================== */
/* Status parsing                                                      */
/* ================================================================== */

int16_t sx126x_hal_parse_status(uint8_t spi_status)
{
    SX126X_DBG_PRINTF("sx126x: parse_status raw=0x%02x (mode=0x%02x, cmd=0x%02x)", spi_status, spi_status & 0xE0,
                      spi_status & 0x0E);

    if (spi_status == 0x00 || spi_status == 0xFF) {
        SX126X_DBG_PRINTF("sx126x: CHIP_NOT_FOUND (status=0x%02x)", spi_status);
        return SX126X_ERR_CHIP_NOT_FOUND;
    }

    uint8_t cmd_status = spi_status & 0x0E;
    switch (cmd_status) {
    case SX126X_STATUS_CMD_TIMEOUT:
        SX126X_DBG_PRINTF("sx126x: SPI CMD TIMEOUT");
        return SX126X_ERR_SPI_CMD_TIMEOUT;
    case SX126X_STATUS_CMD_INVALID:
        SX126X_DBG_PRINTF("sx126x: SPI CMD INVALID");
        return SX126X_ERR_SPI_CMD_INVALID;
    case SX126X_STATUS_CMD_FAILED:
        SX126X_DBG_PRINTF("sx126x: SPI CMD FAILED");
        return SX126X_ERR_SPI_CMD_FAILED;
    default:
        SX126X_DBG_PRINTF("sx126x: status OK");
        return SX126X_ERR_NONE;
    }
}

#endif /* PIO_ENABLE_LORA_RA01S */
