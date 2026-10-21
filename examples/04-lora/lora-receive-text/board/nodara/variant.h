/*
 * nodara board variant — pin definitions (SSOT for this project).
 */

#ifndef _VARIANT_NODARA_
#define _VARIANT_NODARA_

#define VARIANT_MCK (64000000ul)

#define USE_LFXO

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PINS_COUNT (48)
#define NUM_DIGITAL_PINS (48)
#define NUM_ANALOG_INPUTS (6)
#define NUM_ANALOG_OUTPUTS (0)

/* buttons_and_leds — LEDs (GPIO_ACTIVE_LOW) */
#define PIN_LED_RED (32 + 3)   /* P1.03 */
#define PIN_LED_GREEN (32 + 1) /* P1.01 */
#define PIN_LED_BLUE (0 + 14)  /* P0.14 */

/* Adafruit Bluefruit52Lib uses this Arduino-compatible board alias for
 * its connection-status LED. */
#define LED_BLUE PIN_LED_BLUE

/* Arduino libraries, including InternalFileSystem, use this standard alias. */
#define LED_BUILTIN LED_BLUE

#define LED_STATE_ON 0

/* buttons_and_leds — buttons (GPIO_PULL_UP | GPIO_ACTIVE_LOW) */
#define PIN_BTN_F1 (32 + 11) /* P1.11 SW_F1 */
#define PIN_BTN_F2 (0 + 9)   /* P0.09 SW_F2 [NFC1] */
#define PIN_BTN_F3 (0 + 6)   /* P0.06 SW_F3 */
#define PIN_BTN_F4 (0 + 8)   /* P0.08 SW_F4 */
#define PIN_BTN_F5 (0 + 10)  /* P0.10 SW_F5 [NFC2] */
#define PIN_BTN_F6 (0 + 11)  /* P0.11 SW_F6 */

/* lora_ra01s — SPI3 */
#define PIN_LORA_PWR (0 + 13)
#define PIN_LORA_SCK (0 + 19)
#define PIN_LORA_MOSI (0 + 22)
#define PIN_LORA_MISO (0 + 23)
#define PIN_LORA_CS (0 + 24)
#define PIN_LORA_RST (0 + 25)
#define PIN_LORA_BUSY (0 + 17)
#define PIN_LORA_DIO1 (0 + 20)
#define PIN_LORA_DIO3 (0 + 21) /* Connected but not in use */

/* Adafruit core SPI0 — lora_ra01s */
#define PIN_SPI_MISO PIN_LORA_MISO
#define PIN_SPI_MOSI PIN_LORA_MOSI
#define PIN_SPI_SCK PIN_LORA_SCK

/* gnss_gp02 — UART1 @ 9600 */
#define PIN_GNSS_TX (32 + 8) /* P1.08 */
#define PIN_GNSS_RX (32 + 9) /* P1.09 */
#define PIN_GNSS_RST (32 + 5)
#define PIN_GNSS_PWR (0 + 12)
#define PIN_GNSS_STBY (32 + 7)
#define PIN_GNSS_PPS (32 + 4)
#define PIN_GNSS_REGION (32 + 2)

/* Adafruit core Serial1 (UART1) — gnss_gp02 */
#define PIN_SERIAL1_RX PIN_GNSS_RX
#define PIN_SERIAL1_TX PIN_GNSS_TX

/* elink_gdem0213b74 — SPI2 */
#define PIN_EINK_PWR (32 + 10)
#define PIN_EINK_SCK (0 + 31)
#define PIN_EINK_MOSI (0 + 29)
#define PIN_EINK_CS (0 + 30)
#define PIN_EINK_DC (0 + 28)
#define PIN_EINK_RES (0 + 2)
#define PIN_EINK_BUSY (0 + 3)

/* Adafruit core SPI1 — elink_gdem0213b74 */
#define PIN_SPI1_MISO (-1)
#define PIN_SPI1_MOSI PIN_EINK_MOSI
#define PIN_SPI1_SCK PIN_EINK_SCK

/* rtc_pcf8563 — I2C0 */
#define WIRE_INTERFACES_COUNT 1
#define PIN_WIRE_SDA (0 + 26)
#define PIN_WIRE_SCL (0 + 27)
#define PIN_RTC_CLKOUT (0 + 16)

/* ext_flash_py25q32 — SPI1 */
#define PIN_EXT_FLASH_SCK (32 + 14)
#define PIN_EXT_FLASH_MOSI (32 + 12)
#define PIN_EXT_FLASH_MISO (32 + 13)
#define PIN_EXT_FLASH_CS (32 + 15)
#define PIN_EXT_FLASH_WP (0 + 7)
#define PIN_EXT_FLASH_HOLD (0 + 5)

/* SPI_INTERFACES_COUNT: 0=SPI(SPIM3 LoRa), 1=SPI1(SPIM2 E-ink) */
#define SPI_INTERFACES_COUNT 2

/* battery_voltage */
#define PIN_BATTERY_ADC (0 + 4)  /* AIN2 */
#define PIN_BATTERY_PWR (0 + 15) /* ACTIVE_HIGH in DTS */

/* ext_uart_listen — UART0 RX-only P1.00 @ 1200 */
#define PIN_EXT_UART_RX (32 + 0)

#ifdef __cplusplus
}
#endif

#endif /* _VARIANT_NODARA_ */
