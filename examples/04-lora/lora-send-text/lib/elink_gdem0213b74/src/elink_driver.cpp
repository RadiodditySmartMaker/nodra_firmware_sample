#include "elink_driver.h"

#if defined(PIO_ENABLE_ELINK_GDEM0213B74)

#include "variant.h"
#include <Arduino.h>
#include <SPI.h>

/* ── SPI primitives (SPI1 = SPIM2, e-ink bus) ─────────────── */

static void write_cmd(uint8_t cmd)
{
    digitalWrite(PIN_EINK_CS, LOW);
    digitalWrite(PIN_EINK_DC, LOW); /* DC=0: command */
    SPI1.transfer(cmd);
    digitalWrite(PIN_EINK_CS, HIGH);
}

static void write_data(uint8_t data)
{
    digitalWrite(PIN_EINK_CS, LOW);
    digitalWrite(PIN_EINK_DC, HIGH); /* DC=1: data */
    SPI1.transfer(data);
    digitalWrite(PIN_EINK_CS, HIGH);
}

/* ── Busy wait (BUSY=HIGH = busy) ─────────────────────────── */

static void wait_busy(void)
{
    while (digitalRead(PIN_EINK_BUSY) == HIGH) {
    }
}

/* ── Hardware reset sequence ──────────────────────────────── */

static void reset(void)
{
    digitalWrite(PIN_EINK_RES, LOW);
    delay(10);
    digitalWrite(PIN_EINK_RES, HIGH);
    delay(10);
    wait_busy();
}

/* ── Public API ───────────────────────────────────────────── */

void elink_driver_set_pins_idle(void)
{
    pinMode(PIN_EINK_RES, INPUT);
    pinMode(PIN_EINK_BUSY, INPUT);
    pinMode(PIN_EINK_CS, INPUT);
    pinMode(PIN_EINK_DC, INPUT);
}

void elink_driver_init_full(void)
{
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);
    pinMode(PIN_EINK_DC, OUTPUT);
    pinMode(PIN_EINK_RES, OUTPUT);
    pinMode(PIN_EINK_BUSY, INPUT);

    SPI1.begin();
    SPI1.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    reset();

    /* SWRESET */
    write_cmd(0x12);
    wait_busy();

    /* Driver output control (HEIGHT-1 = 249 = 0xF9) */
    write_cmd(0x01);
    write_data(0xF9);
    write_data(0x00);
    write_data(0x00);

    /* Full RAM area setup (entry mode, bounds, address counters) */
    set_full_ram_area();

    /* Border waveform */
    write_cmd(0x3C);
    write_data(0x05);

    /* Display update control */
    write_cmd(0x21);
    write_data(0x00);
    write_data(0x80);

    /* Read built-in temperature sensor */
    write_cmd(0x18);
    write_data(0x80);

    wait_busy();
}

void elink_driver_power_on(void)
{
    write_cmd(0x22);
    write_data(0xF8); /* Enable analog + load LUT + initial */
    write_cmd(0x20);  /* Master activation */
    wait_busy();
}

void elink_driver_power_off(void)
{
    write_cmd(0x22);
    write_data(0x83); /* Disable analog + clock */
    write_cmd(0x20);  /* Master activation */
    wait_busy();

    SPI1.end();

    /* Drive all connected pins low to prevent ESD diode backpower
     * through the SSD1680 when the display power rail is off. */
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, LOW);
    pinMode(PIN_EINK_DC, OUTPUT);
    digitalWrite(PIN_EINK_DC, LOW);
    pinMode(PIN_EINK_RES, OUTPUT);
    digitalWrite(PIN_EINK_RES, LOW);
    pinMode(PIN_EINK_BUSY, OUTPUT);
    digitalWrite(PIN_EINK_BUSY, LOW);

    /* SCK and MOSI released from SPIM after SPI1.end(), reconfigure as
     * GPIO output low to prevent leakage through ESD protection diodes. */
    pinMode(PIN_EINK_SCK, OUTPUT);
    digitalWrite(PIN_EINK_SCK, LOW);
    pinMode(PIN_EINK_MOSI, OUTPUT);
    digitalWrite(PIN_EINK_MOSI, LOW);
}

/* ── RAM area setup ──────────────────────────────────────── */

void set_full_ram_area(void)
{
    /* Reset data entry mode, RAM bounds and address counters.
     * Must be called before every RAM write or partial refresh,
     * otherwise the SSD1680 internal address counter may be at an
     * incorrect position after a previous operation. */
    write_cmd(0x11);  /* Data entry mode */
    write_data(0x01); /* vertical, Y decrement */
    write_cmd(0x44);  /* RAM X start/end */
    write_data(0x00);
    write_data(ELINK_WIDTH / 8 - 1);      /* 15 */
    write_cmd(0x45);                      /* RAM Y start/end */
    write_data((ELINK_HEIGHT - 1) % 256); /* 0xF9 — Y start (249), decrement to Y end (0) */
    write_data((ELINK_HEIGHT - 1) / 256); /* 0x00 */
    write_data(0x00);                     /* Y end (0) */
    write_data(0x00);
    write_cmd(0x4E); /* RAM X counter */
    write_data(0x00);
    write_cmd(0x4F);                      /* RAM Y counter */
    write_data((ELINK_HEIGHT - 1) % 256); /* 0xF9 */
    write_data((ELINK_HEIGHT - 1) / 256); /* 0x00 */
}

static void write_full_image(uint8_t cmd, const uint8_t *buf)
{
    set_full_ram_area();
    write_cmd(cmd);
    for (uint32_t i = 0; i < (uint32_t)ELINK_WIDTH * (uint32_t)ELINK_HEIGHT / 8; i++) {
        write_data(buf[i]);
    }
}

void elink_driver_write_image(const uint8_t *buf)
{
    write_full_image(0x24, buf); /* Write RAM (black/white, current) */
}

void elink_driver_write_image_previous(const uint8_t *buf)
{
    write_full_image(0x26, buf); /* Write RAM (black/white, previous) */
}

void elink_driver_refresh(bool partial)
{
    if (partial) {
        /* Reset RAM area before partial update: the SSD1680 internal
         * address counter must point to the start of RAM, otherwise
         * 0x22/0xFC will only update a subset of the display. */
        set_full_ram_area();
    }

    write_cmd(0x22);
    write_data(partial ? 0xFC : 0xF7); /* 0xFC=partial, 0xF7=full */
    write_cmd(0x20);                   /* Master activation */
    wait_busy();
}

void elink_driver_deep_sleep(void)
{
    write_cmd(0x10);  /* Deep sleep mode */
    write_data(0x01); /* Enter deep sleep */
    delay(100);
}

#endif /* PIO_ENABLE_ELINK_GDEM0213B74 */
