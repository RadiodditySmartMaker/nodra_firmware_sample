#ifndef ELINK_DRIVER_H_
#define ELINK_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#define ELINK_WIDTH 128
#define ELINK_HEIGHT 250

/* Set GPIO to safe idle state (prevent backpower) */
void elink_driver_set_pins_idle(void);

/* Full init sequence: reset + register configuration */
void elink_driver_init_full(void);

/* Power on/off */
void elink_driver_power_on(void);
void elink_driver_power_off(void);

/* RAM area setup */
void set_full_ram_area(void);

/* Write full framebuffer (4000 bytes, 1bpp) to SSD1680 RAM (0x24, current) */
void elink_driver_write_image(const uint8_t *buf);

/* Write full framebuffer to SSD1680 previous RAM (0x26, for partial update diff) */
void elink_driver_write_image_previous(const uint8_t *buf);

/* Trigger display refresh: partial=false full, partial=true fast */
void elink_driver_refresh(bool partial);

/* Deep sleep (minimum power, wake by RST) */
void elink_driver_deep_sleep(void);

#endif /* ELINK_DRIVER_H_ */
