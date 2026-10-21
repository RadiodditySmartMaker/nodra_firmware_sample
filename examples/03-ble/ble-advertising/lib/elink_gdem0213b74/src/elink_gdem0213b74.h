#ifndef ELINK_GDEM0213B74_H_
#define ELINK_GDEM0213B74_H_

#include <stdint.h>

#if defined(PIO_ENABLE_ELINK_GDEM0213B74)

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void elink_gdem0213b74_setup(void);
int32_t elink_gdem0213b74_loop(void);
bool elink_gdem0213b74_start(void);
void elink_gdem0213b74_stop(void);
bool elink_gdem0213b74_is_ready(void);
int16_t elink_gdem0213b74_width(void);
int16_t elink_gdem0213b74_height(void);
void elink_gdem0213b74_clear(bool black);
void elink_gdem0213b74_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap);
void elink_gdem0213b74_show_text(const char *text);
void elink_gdem0213b74_show_text_centered(const char *text);
void elink_gdem0213b74_refresh(bool full);

#ifdef __cplusplus
}
#endif

#endif /* PIO_ENABLE_ELINK_GDEM0213B74 */

#endif /* ELINK_GDEM0213B74_H_ */
