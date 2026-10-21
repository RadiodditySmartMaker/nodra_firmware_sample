/*
 * nodara — Arduino pin map (linear P0/P1 numbering).
 * Pin comments cite NCS ncs_pro overlay source files.
 */

#include "variant.h"
#include "nrf.h"
#include "wiring_constants.h"
#include "wiring_digital.h"

const uint32_t g_ADigitalPinMap[] = {
    /* P0.00–P0.31 */
    0xff, /*  0 P0.00 XL1 — crystal, do not use */
    0xff, /*  1 P0.01 XL2 — crystal, do not use */
    2,    /*  2 P0.02 elink_gdem0213b74.overlay RES */
    3,    /*  3 P0.03 elink_gdem0213b74.overlay BUSY */
    4,    /*  4 P0.04 battery_voltage.overlay ADC AIN2 */
    5,    /*  5 P0.05 ext_flash_py25q32.overlay HOLD */
    6,    /*  6 P0.06 buttons_and_leds.overlay SW_F3 */
    7,    /*  7 P0.07 ext_flash_py25q32.overlay WP */
    8,    /*  8 P0.08 buttons_and_leds.overlay SW_F4 */
    9,    /*  9 P0.09 buttons_and_leds.overlay SW_F2 */
    10,   /* 10 P0.10 buttons_and_leds.overlay SW_F5 */
    11,   /* 11 P0.11 buttons_and_leds.overlay SW_F6 */
    12,   /* 12 P0.12 gnss_gp02.overlay PWR */
    13,   /* 13 P0.13 lora_ra01s.overlay PWR */
    14,   /* 14 P0.14 buttons_and_leds.overlay LED blue */
    15,   /* 15 P0.15 battery_voltage.overlay batt-power */
    16,   /* 16 P0.16 rtc_pcf8563.overlay CLKOUT readback */
    17,   /* 17 P0.17 lora_ra01s.overlay BUSY */
    18,   /* 18 P0.18 SWD reset (UICR gpio-as-nreset) */
    19,   /* 19 P0.19 lora_ra01s.overlay SPI3 SCK */
    20,   /* 20 P0.20 lora_ra01s.overlay DIO1 */
    21,   /* 21 P0.21 — */
    22,   /* 22 P0.22 lora_ra01s.overlay SPI3 MOSI */
    23,   /* 23 P0.23 lora_ra01s.overlay SPI3 MISO */
    24,   /* 24 P0.24 lora_ra01s.overlay SPI3 CS */
    25,   /* 25 P0.25 lora_ra01s.overlay RESET */
    26,   /* 26 P0.26 rtc_pcf8563.overlay I2C SDA */
    27,   /* 27 P0.27 rtc_pcf8563.overlay I2C SCL */
    28,   /* 28 P0.28 elink_gdem0213b74.overlay DC */
    29,   /* 29 P0.29 elink_gdem0213b74.overlay SPI2 MOSI */
    30,   /* 30 P0.30 elink_gdem0213b74.overlay SPI2 CS */
    31,   /* 31 P0.31 elink_gdem0213b74.overlay SPI2 SCK */

    /* P1.00–P1.15 */
    32, /* 32 P1.00 ext_uart_listen.overlay UART0 RX */
    33, /* 33 P1.01 buttons_and_leds.overlay LED green */
    34, /* 34 P1.02 gnss_gp02.overlay REGION */
    35, /* 35 P1.03 buttons_and_leds.overlay LED red */
    36, /* 36 P1.04 gnss_gp02.overlay PPS */
    37, /* 37 P1.05 gnss_gp02.overlay RST */
    38, /* 38 P1.06 — */
    39, /* 39 P1.07 gnss_gp02.overlay STBY */
    40, /* 40 P1.08 gnss_gp02.overlay UART1 TX */
    41, /* 41 P1.09 gnss_gp02.overlay UART1 RX */
    42, /* 42 P1.10 elink_gdem0213b74.overlay PWR */
    43, /* 43 P1.11 buttons_and_leds.overlay SW_F1 */
    44, /* 44 P1.12 ext_flash_py25q32.overlay SPI1 MOSI */
    45, /* 45 P1.13 ext_flash_py25q32.overlay SPI1 MISO */
    46, /* 46 P1.14 ext_flash_py25q32.overlay SPI1 SCK */
    47, /* 47 P1.15 ext_flash_py25q32.overlay SPI1 CS */
};

void initVariant(void)
{
    pinMode(PIN_LED_RED, OUTPUT);
    digitalWrite(PIN_LED_RED, HIGH);

    pinMode(PIN_LED_GREEN, OUTPUT);
    digitalWrite(PIN_LED_GREEN, HIGH);

    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_BLUE, HIGH);
}
