#ifndef FONT5X7_H_
#define FONT5X7_H_

#include <stdint.h>

#define FONT5X7_CHAR_W 5
#define FONT5X7_CHAR_H 7
#define FONT5X7_SPACING 6 /* 5px char + 1px gap */

/* Bitmap data for ASCII 0x20..0x7E, each 5 bytes (columns), MSB=top */
extern const uint8_t font5x7_data[95][5];

/* Render ASCII string into 1bpp framebuffer via set_pixel callback.
 *  set_pixel(buf, x, y) is called for each black pixel.
 */
typedef void (*font5x7_pixel_fn)(uint8_t *buf, int16_t x, int16_t y);

void font5x7_render(uint8_t *buf, int16_t x, int16_t y, const char *text, font5x7_pixel_fn set_pixel);

/* Measure rendered width in pixels */
int16_t font5x7_measure(const char *text);

#endif /* FONT5X7_H_ */
