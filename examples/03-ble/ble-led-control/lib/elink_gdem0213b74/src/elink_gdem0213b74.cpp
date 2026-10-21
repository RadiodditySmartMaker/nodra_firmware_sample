#include "elink_gdem0213b74.h"

#if defined(PIO_ENABLE_ELINK_GDEM0213B74)

#include "variant.h"
#include <Arduino.h>
#include <SPI.h>
#include <errno.h>

#include "elink_driver.h"
#include "font5x7.h"

#if defined(PIO_ENABLE_SHELL_MODULE)
#include <shell_module.h>
#endif /* PIO_ENABLE_SHELL_MODULE */

#if defined(PIO_ENABLE_BUTTON)
extern bool icon_demo_start(void);
extern void icon_demo_stop(void);
extern bool icon_demo_reset(void);
extern bool icon_demo_is_running(void);
#endif /* PIO_ENABLE_BUTTON */

/* ------------------------------------------------------------------ */
/* Constants                                                             */
/* ------------------------------------------------------------------ */

/* Use native coordinates (portrait: 128 gates × 250 sources).
 * Physical display is 250×122 landscape; orientation is cosmetic
 * for test patterns. */
#define EINK_W ELINK_WIDTH     /* 128 gates (total) */
#define EINK_H ELINK_HEIGHT    /* 250 sources */
#define EINK_VISIBLE_GATES 122 /* 128 total, 6 are dummy/unconnected */

/* Physical display dimensions (logical coords for rotation 0/2). */
#define DISP_W 250                /* horizontal columns (sources) */
#define DISP_H EINK_VISIBLE_GATES /* 122 visible rows (gates) */

/* Logical dimensions depend on rotation. */
#define DISP_LOGICAL_W(rot) ((rot) % 2 == 0 ? DISP_W : DISP_H)
#define DISP_LOGICAL_H(rot) ((rot) % 2 == 0 ? DISP_H : DISP_W)

/* Frame buffer: 16 bytes per row (128px, packed MSB first) × 250 rows.
 * Must be file-static (BSS) — loop stack is only ~1024 bytes. */
#define EINK_BUF_PITCH ((EINK_W + 7) / 8)
#define EINK_BUF_SIZE (EINK_BUF_PITCH * EINK_H)

/* PIN_EINK_PWR active level */
#define PIN_EINK_PWR_ON (LOW)

/* ------------------------------------------------------------------ */
/* Static globals                                                        */
/* ------------------------------------------------------------------ */

enum eink_state {
    EINK_STATE_OFF,
    EINK_STATE_READY,
};

static enum eink_state eink_state = EINK_STATE_OFF;

static uint8_t epaper_buf[EINK_BUF_SIZE];
static uint8_t eink_rotation; /* 0=normal, 1=90°CW, 2=180°, 3=270°CW */
/* ------------------------------------------------------------------ */
/* Display helpers                                                       */
/* ------------------------------------------------------------------ */

static int eink_require_ready(char *out, size_t out_len)
{
    if (eink_state != EINK_STATE_READY) {
        snprintf(out, out_len, "eink not ready; run 'eink start' first\r\n");
        return -1;
    }
    return 0;
}

static void eink_set_pixel_black(uint8_t *buf, int16_t dx, int16_t dy)
{
    /* Transform logical display (dx, dy) to buffer (gate, source).
     * Buffer layout: source = horizontal (0=left), gate = vertical (0=top). */
    int16_t gate, source;
    switch (eink_rotation) {
    case 0:
        gate = dy;
        source = dx;
        break;
    case 1: /* 90° CW */
        gate = DISP_H - 1 - dx;
        source = dy;
        break;
    case 2: /* 180° */
        gate = DISP_H - 1 - dy;
        source = DISP_W - 1 - dx;
        break;
    case 3: /* 270° CW (90° CCW) */
        gate = dx;
        source = DISP_W - 1 - dy;
        break;
    default:
        return;
    }

    if (gate < 0 || gate >= (int16_t)EINK_VISIBLE_GATES || source < 0 || source >= (int16_t)EINK_H) {
        return;
    }

    size_t idx = (size_t)source * EINK_BUF_PITCH + (size_t)(gate / 8);
    uint8_t bit = 7 - (gate % 8);
    buf[idx] &= ~(1 << bit);
}

static void eink_blit_text(uint8_t *buf, int16_t x, int16_t y, const char *text)
{
    font5x7_render(buf, x, y, text, eink_set_pixel_black);
}

static void eink_push_full(void)
{
    elink_driver_write_image(epaper_buf);
    elink_driver_refresh(false);
}

static void eink_push_fast(void)
{
    elink_driver_write_image(epaper_buf);
    elink_driver_refresh(true);
    /* Sync previous buffer so the next partial refresh has correct baseline */
    elink_driver_write_image_previous(epaper_buf);
}

static void eink_force_white_clear(void)
{
    memset(epaper_buf, 0x00, EINK_BUF_SIZE);
    eink_push_full();
    memset(epaper_buf, 0xFF, EINK_BUF_SIZE);
    eink_push_full();
}

bool elink_gdem0213b74_start(void)
{
    if (eink_state == EINK_STATE_READY) {
        return true;
    }

    pinMode(PIN_EINK_PWR, OUTPUT);
    digitalWrite(PIN_EINK_PWR, PIN_EINK_PWR_ON);
    delay(100);

    elink_driver_init_full();

    eink_rotation = 0;
    memset(epaper_buf, 0xFF, EINK_BUF_SIZE);
    eink_push_full();
    elink_driver_write_image_previous(epaper_buf);

    eink_state = EINK_STATE_READY;
    return true;
}

void elink_gdem0213b74_stop(void)
{
    if (eink_state == EINK_STATE_READY) {
        elink_driver_power_off();
    }

    pinMode(PIN_EINK_RES, INPUT);
    pinMode(PIN_EINK_BUSY, INPUT);
    pinMode(PIN_EINK_CS, INPUT);
    pinMode(PIN_EINK_DC, INPUT);

    pinMode(PIN_EINK_PWR, OUTPUT);
    digitalWrite(PIN_EINK_PWR, !PIN_EINK_PWR_ON);

    eink_state = EINK_STATE_OFF;
    eink_rotation = 0;
}

bool elink_gdem0213b74_is_ready(void) { return eink_state == EINK_STATE_READY; }

int16_t elink_gdem0213b74_width(void) { return (int16_t)DISP_LOGICAL_W(eink_rotation); }

int16_t elink_gdem0213b74_height(void) { return (int16_t)DISP_LOGICAL_H(eink_rotation); }

void elink_gdem0213b74_clear(bool black)
{
    if (eink_state != EINK_STATE_READY) {
        return;
    }

    memset(epaper_buf, black ? 0x00 : 0xFF, EINK_BUF_SIZE);
}

void elink_gdem0213b74_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t *bitmap)
{
    if (eink_state != EINK_STATE_READY || bitmap == NULL || w <= 0 || h <= 0) {
        return;
    }

    const int16_t bytes_per_row = (w + 7) / 8;
    for (int16_t row = 0; row < h; row++) {
        for (int16_t col = 0; col < w; col++) {
            const uint8_t src = bitmap[row * bytes_per_row + col / 8];
            const uint8_t bit = (uint8_t)(7 - (col % 8));
            if (src & (1u << bit)) {
                eink_set_pixel_black(epaper_buf, x + col, y + row);
            }
        }
    }
}

void elink_gdem0213b74_show_text(const char *text)
{
    if (eink_state != EINK_STATE_READY || text == NULL) {
        return;
    }

    constexpr int16_t kLineSpacing = 1;
    constexpr int16_t kMaxCharsPerLine = (DISP_W - 16) / FONT5X7_SPACING;
    char line[kMaxCharsPerLine + 1];
    int16_t line_count = 1;
    int16_t line_length = 0;

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\r') {
            continue;
        }
        if (*p == '\n') {
            line_count++;
            line_length = 0;
        } else if (++line_length > kMaxCharsPerLine) {
            line_count++;
            line_length = 1;
        }
    }

    const int16_t text_height = line_count * FONT5X7_CHAR_H + (line_count - 1) * kLineSpacing;
    int16_t y = (elink_gdem0213b74_height() - text_height) / 2;
    line_length = 0;

    elink_gdem0213b74_clear(false);

    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '\r') {
            continue;
        }

        if (*p == '\n' || line_length == kMaxCharsPerLine) {
            line[line_length] = '\0';
            const int16_t x = (elink_gdem0213b74_width() - font5x7_measure(line)) / 2;
            eink_blit_text(epaper_buf, x, y, line);
            y += FONT5X7_CHAR_H + kLineSpacing;
            line_length = 0;
        }

        if (*p == '\n') {
            continue;
        }

        line[line_length++] = *p;
    }

    line[line_length] = '\0';
    const int16_t x = (elink_gdem0213b74_width() - font5x7_measure(line)) / 2;
    eink_blit_text(epaper_buf, x, y, line);

    elink_gdem0213b74_refresh(true);
}

void elink_gdem0213b74_show_text_centered(const char *text)
{
    if (eink_state != EINK_STATE_READY || text == NULL) {
        return;
    }

    const int16_t text_width = font5x7_measure(text);
    const int16_t x = (elink_gdem0213b74_width() - text_width) / 2;
    const int16_t y = (elink_gdem0213b74_height() - FONT5X7_CHAR_H) / 2;

    elink_gdem0213b74_clear(false);
    eink_blit_text(epaper_buf, x, y, text);
    elink_gdem0213b74_refresh(true);
}

void elink_gdem0213b74_refresh(bool full)
{
    if (eink_state != EINK_STATE_READY) {
        return;
    }

    if (full) {
        eink_push_full();
    } else {
        eink_push_fast();
    }
}
/* ------------------------------------------------------------------ */
/* Shell commands                                                       */
/* ------------------------------------------------------------------ */

#if defined(PIO_ENABLE_SHELL_MODULE)

/* argv[0] is "eink", argv[1] is the subcommand.
 * Sub-commands like "eink show white" have argv[2] as the sub-sub-cmd. */

static int cmd_eink_start(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    if (eink_state == EINK_STATE_READY) {
        snprintf(out, out_len, "eink already started\r\n");
        return 0;
    }

    (void)elink_gdem0213b74_start();
    snprintf(out, out_len, "FT: eink started: white screen refreshed\r\n");
    return 0;
}

static int cmd_eink_stop(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;

    elink_gdem0213b74_stop();
    snprintf(out, out_len, "FT: eink stopped\r\n");
    return 0;
}

static int cmd_eink_show_white(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int ret = eink_require_ready(out, out_len);
    if (ret) {
        return ret;
    }

    eink_force_white_clear();

    snprintf(out, out_len, "FT: eink: full white\r\n");
    return 0;
}

static int cmd_eink_show_black(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int ret = eink_require_ready(out, out_len);
    if (ret) {
        return ret;
    }

    memset(epaper_buf, 0x00, EINK_BUF_SIZE);
    eink_push_full();

    snprintf(out, out_len, "FT: eink: full black\r\n");
    return 0;
}

static int cmd_eink_show_stripes(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    int ret = eink_require_ready(out, out_len);
    if (ret) {
        return ret;
    }

    /* 8-px stripes in logical x direction (rotation-aware via
     * eink_set_pixel_black).  Start white, then draw black stripes. */
    memset(epaper_buf, 0xFF, EINK_BUF_SIZE);
    int16_t lw = (int16_t)DISP_LOGICAL_W(eink_rotation);
    int16_t lh = (int16_t)DISP_LOGICAL_H(eink_rotation);
    for (int16_t dy = 0; dy < lh; dy++) {
        for (int16_t dx = 0; dx < lw; dx++) {
            if ((dx / 8) % 2 == 0) {
                eink_set_pixel_black(epaper_buf, dx, dy);
            }
        }
    }

    eink_push_full();

    snprintf(out, out_len, "FT: eink: vertical stripes\r\n");
    return 0;
}

static int cmd_eink_text(int argc, char *argv[], char *out, size_t out_len)
{
    /* argv: eink text <x> <y> <full|fast> "<text>" → 6 total */
    if (argc < 6) {
        snprintf(out, out_len, "usage: eink text <x> <y> <full|fast> \"<text>\"\r\n");
        return -1;
    }

    int ret = eink_require_ready(out, out_len);
    if (ret) {
        return ret;
    }

    long x = strtol(argv[2], NULL, 10);
    long y = strtol(argv[3], NULL, 10);
    if (x < 0 || x >= (int16_t)DISP_LOGICAL_W(eink_rotation) || y < 0 || y >= (int16_t)DISP_LOGICAL_H(eink_rotation)) {
        snprintf(out, out_len, "invalid position (x 0..%d, y 0..%d)\r\n", DISP_LOGICAL_W(eink_rotation) - 1,
                 DISP_LOGICAL_H(eink_rotation) - 1);
        return -1;
    }

    bool full = (strcmp(argv[4], "full") == 0);
    if (!full && strcmp(argv[4], "fast") != 0) {
        snprintf(out, out_len, "mode must be 'full' or 'fast'\r\n");
        return -1;
    }

    eink_blit_text(epaper_buf, (int16_t)x, (int16_t)y, argv[5]);

    if (full) {
        eink_push_full();
        snprintf(out, out_len, "FT: eink: text (full)\r\n");
    } else {
        eink_push_fast();
        snprintf(out, out_len, "FT: eink: text (fast)\r\n");
    }

    return 0;
}

#if defined(PIO_ENABLE_EXT_FLASH_PY25Q32)
#include "cnfont_layout.h"
#include "ext_flash_py25q32.h"

static const char *cmd_ctext_next_utf8(const char *p, int *char_len)
{
    uint8_t lead = (uint8_t)*p;

    if (lead < 0x80) {
        *char_len = 1; /* ASCII */
    } else if (lead < 0xC0) {
        *char_len = 1; /* stray continuation byte */
    } else if (lead < 0xE0) {
        *char_len = 2; /* 2-byte sequence */
    } else if (lead < 0xF0) {
        *char_len = 3; /* 3-byte sequence (CJK) */
    } else {
        *char_len = 4; /* 4-byte sequence */
    }

    /* Guard against truncated string */
    for (int i = 1; i < *char_len; i++) {
        if (p[i] == '\0') {
            *char_len = i;
            break;
        }
    }
    return p + *char_len;
}

static int cmd_eink_ctext(int argc, char *argv[], char *out, size_t out_len)
{
    /* argv: eink ctext <x> <y> <full|fast> <text> → 6 total */
    if (argc < 6) {
        snprintf(out, out_len, "usage: eink ctext <x> <y> <full|fast> <text>\r\n");
        return -1;
    }

    int ret = eink_require_ready(out, out_len);
    if (ret) {
        return ret;
    }

    long x = strtol(argv[2], NULL, 10);
    long y = strtol(argv[3], NULL, 10);
    if (x < 0 || x >= (int16_t)DISP_LOGICAL_W(eink_rotation) || y < 0 || y >= (int16_t)DISP_LOGICAL_H(eink_rotation)) {
        snprintf(out, out_len, "invalid position (x 0..%d, y 0..%d)\r\n", DISP_LOGICAL_W(eink_rotation) - 1,
                 DISP_LOGICAL_H(eink_rotation) - 1);
        return -1;
    }

    bool full = (strcmp(argv[4], "full") == 0);
    if (!full && strcmp(argv[4], "fast") != 0) {
        snprintf(out, out_len, "mode must be 'full' or 'fast'\r\n");
        return -1;
    }

    const char *p = argv[5];
    int16_t cur_x = (int16_t)x;
    int16_t cur_y = (int16_t)y;
    int missing_count = 0;
    size_t out_used = 0;

    while (*p != '\0') {
        int char_len;
        const char *next = cmd_ctext_next_utf8(p, &char_len);

        if (char_len == 3) {
            /* 3-byte UTF-8: CJK character — pass raw bytes as 4-byte key */
            char utf8_key[4] = {p[0], p[1], p[2], '\0'};

            uint8_t bitmap[CNFONT_BITMAP_SIZE];
            int lookup_ret = ext_flash_py25q32_cnfont_lookup_glyph(utf8_key, bitmap);

            if (lookup_ret == 0) {
                /* Blit cnfont bitmap (1bpp, 1=black) to frame buffer */
                for (unsigned row = 0; row < CNFONT_GLYPH_HEIGHT; row++) {
                    for (unsigned col = 0; col < CNFONT_GLYPH_WIDTH; col++) {
                        unsigned byte_index = row * ((CNFONT_GLYPH_WIDTH + 7) / 8) + col / 8;
                        unsigned bit_index = 7 - (col % 8);
                        if (bitmap[byte_index] & (1 << bit_index)) {
                            eink_set_pixel_black(epaper_buf, cur_x + (int16_t)col, cur_y + (int16_t)row);
                        }
                    }
                }
            } else if (lookup_ret < 0) {
                missing_count++;
                size_t remain = out_len - out_used;
                if (remain > 40) {
                    int n = snprintf(out + out_used, remain, "missing: %s ", utf8_key);
                    if (n > 0) {
                        out_used += (size_t)n;
                    }
                }
            }

            cur_x += CNFONT_GLYPH_WIDTH;
        }
        /* Non-3-byte (ASCII, 2-byte, 4-byte): skip */

        p = next;
    }

    if (full) {
        eink_push_full();
    } else {
        eink_push_fast();
    }

    size_t remain = out_len - out_used;
    if (missing_count > 0) {
        snprintf(out + out_used, remain, "\r\nFT: eink: ctext (%s, %d missing)\r\n", full ? "full" : "fast", missing_count);
    } else {
        snprintf(out + out_used, remain, "FT: eink: ctext (%s)\r\n", full ? "full" : "fast");
    }
    return 0;
}
#endif /* PIO_ENABLE_EXT_FLASH_PY25Q32 */

static int cmd_eink_rotate(int argc, char *argv[], char *out, size_t out_len)
{
    /* argv: eink rotate <0|1|2|3> → 4 total */
    if (argc < 3) {
        snprintf(out, out_len,
                 "usage: eink rotate <0|1|2|3> (0=normal, 1=90CW, "
                 "2=180, 3=270CW)\r\n");
        return -1;
    }

    int rot = atoi(argv[2]);
    if (rot < 0 || rot > 3) {
        snprintf(out, out_len, "rotation must be 0..3\r\n");
        return -1;
    }

    eink_rotation = (uint8_t)rot;
    snprintf(out, out_len, "eink rotation set to %d\r\n", rot);
    return 0;
}

static int cmd_eink_icon_mov(int argc, char *argv[], char *out, size_t out_len)
{
#if !defined(PIO_ENABLE_BUTTON)
    (void)argc;
    (void)argv;
    snprintf(out, out_len, "icon_mov requires button module\r\n");
    return -1;
#else
    if (argc < 3) {
        snprintf(out, out_len, "usage: eink icon_mov <start|stop|reset>\r\n");
        return -1;
    }

    if (strcmp(argv[2], "start") == 0) {
        if (icon_demo_start()) {
            snprintf(out, out_len, "FT: eink: icon_mov started, icon centered (F1/F2/F3/F4 = up/down/left/right)\r\n");
            return 0;
        }

        snprintf(out, out_len, "failed to start icon_mov\r\n");
        return -1;
    }

    if (strcmp(argv[2], "stop") == 0) {
        if (!icon_demo_is_running()) {
            snprintf(out, out_len, "icon_mov is not running\r\n");
            return -1;
        }

        icon_demo_stop();
        snprintf(out, out_len, "FT: eink: icon_mov stopped\r\n");
        return 0;
    }

    if (strcmp(argv[2], "reset") == 0) {
        if (icon_demo_reset()) {
            snprintf(out, out_len, "FT: eink: icon_mov reset to center\r\n");
            return 0;
        }

        snprintf(out, out_len, "icon_mov is not running\r\n");
        return -1;
    }

    snprintf(out, out_len, "usage: eink icon_mov <start|stop|reset>\r\n");
    return -1;
#endif
}

static int cmd_eink_help(int argc, char *argv[], char *out, size_t out_len)
{
    (void)argc;
    (void)argv;
    snprintf(out, out_len,
             "eink commands:\r\n"
             "  eink start\r\n"
             "  eink stop\r\n"
             "  eink show white|black|stripes\r\n"
             "  eink icon_mov <start|stop|reset>\r\n"
             "  eink text <x> <y> full|fast \"<text>\"\r\n"
             "  eink rotate <0|1|2|3>\r\n"
#if defined(PIO_ENABLE_EXT_FLASH_PY25Q32)
             "  eink ctext <x> <y> full|fast <text>\r\n"
#endif /* PIO_ENABLE_EXT_FLASH_PY25Q32 */
    );
    return 0;
}

static int cmd_eink_dispatch(int argc, char *argv[], char *out, size_t out_len)
{
    /* argv[0] = "eink", argv[1] = subcommand (if present) */
    if (argc < 2) {
        return cmd_eink_help(argc, argv, out, out_len);
    }

    if (strcmp(argv[1], "start") == 0) {
        return cmd_eink_start(argc, argv, out, out_len);
    }
    if (strcmp(argv[1], "stop") == 0) {
        return cmd_eink_stop(argc, argv, out, out_len);
    }
    if (strcmp(argv[1], "show") == 0) {
        if (argc < 3) {
            snprintf(out, out_len, "usage: eink show white|black|stripes\r\n");
            return -1;
        }
        if (strcmp(argv[2], "white") == 0) {
            return cmd_eink_show_white(argc, argv, out, out_len);
        }
        if (strcmp(argv[2], "black") == 0) {
            return cmd_eink_show_black(argc, argv, out, out_len);
        }
        if (strcmp(argv[2], "stripes") == 0) {
            return cmd_eink_show_stripes(argc, argv, out, out_len);
        }
        snprintf(out, out_len, "unknown show type: %s\r\n", argv[2]);
        return -1;
    }
    if (strcmp(argv[1], "text") == 0) {
        return cmd_eink_text(argc, argv, out, out_len);
    }
    if (strcmp(argv[1], "icon_mov") == 0) {
        return cmd_eink_icon_mov(argc, argv, out, out_len);
    }
    if (strcmp(argv[1], "rotate") == 0) {
        return cmd_eink_rotate(argc, argv, out, out_len);
    }
#if defined(PIO_ENABLE_EXT_FLASH_PY25Q32)
    if (strcmp(argv[1], "ctext") == 0) {
        return cmd_eink_ctext(argc, argv, out, out_len);
    }
#endif /* PIO_ENABLE_EXT_FLASH_PY25Q32 */

    return cmd_eink_help(argc, argv, out, out_len);
}

#endif /* PIO_ENABLE_SHELL_MODULE */

/* ------------------------------------------------------------------ */
/* Module init                                                          */
/* ------------------------------------------------------------------ */

int32_t elink_gdem0213b74_loop(void)
{
    return INT32_MAX;
}

void elink_gdem0213b74_setup(void)
{
    /* Safe GPIO state: all inputs, prevent backpower through ESD diodes */
    pinMode(PIN_EINK_RES, INPUT);
    pinMode(PIN_EINK_BUSY, INPUT);
    pinMode(PIN_EINK_CS, INPUT);
    pinMode(PIN_EINK_DC, INPUT);

    /* Power off */
    pinMode(PIN_EINK_PWR, OUTPUT);
    digitalWrite(PIN_EINK_PWR, !PIN_EINK_PWR_ON);

    eink_state = EINK_STATE_OFF;

#if defined(PIO_ENABLE_SHELL_MODULE)
    shell_register_cmd("eink", cmd_eink_dispatch, "E-ink GDEM0213B74 test commands");
#endif /* PIO_ENABLE_SHELL_MODULE */
}
#endif /* PIO_ENABLE_ELINK_GDEM0213B74 */
