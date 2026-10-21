#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <string.h>
#include <variant.h>

namespace {

constexpr uint8_t kPanelWidth = 128;
constexpr uint8_t kPanelHeight = 250;
constexpr uint8_t kVisibleRows = 122;
constexpr uint8_t kBytesPerRow = kPanelWidth / 8;
constexpr uint16_t kBufferSize = kBytesPerRow * kPanelHeight;
constexpr uint8_t kFontWidth = 5;
constexpr uint8_t kFontHeight = 7;
constexpr uint8_t kFontSpacing = 6;
constexpr int16_t kDisplayWidth = 250;
constexpr int16_t kDisplayHeight = kVisibleRows;
constexpr uint8_t kPowerOn = LOW;

bool display_ready = false;
uint8_t framebuffer[kBufferSize];

/* 5x7 uppercase ASCII font: enough for the English messages in this example. */
const uint8_t kFont[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* space */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* F */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* G */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* M */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /* W */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* Y */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z */
};

const uint8_t *glyph_for(char character)
{
    if (character >= 'a' && character <= 'z') {
        character -= 'a' - 'A';
    }
    if (character == ' ') {
        return kFont[0];
    }
    if (character == ':') {
        return kFont[1];
    }
    if (character >= 'A' && character <= 'Z') {
        return kFont[3 + character - 'A'];
    }
    return kFont[2];
}

void write_command(uint8_t command)
{
    digitalWrite(PIN_EINK_CS, LOW);
    digitalWrite(PIN_EINK_DC, LOW);
    SPI1.transfer(command);
    digitalWrite(PIN_EINK_CS, HIGH);
}

void write_data(uint8_t value)
{
    digitalWrite(PIN_EINK_CS, LOW);
    digitalWrite(PIN_EINK_DC, HIGH);
    SPI1.transfer(value);
    digitalWrite(PIN_EINK_CS, HIGH);
}

void wait_until_ready()
{
    while (digitalRead(PIN_EINK_BUSY) == HIGH) {
    }
}

void reset_panel()
{
    digitalWrite(PIN_EINK_RES, LOW);
    delay(10);
    digitalWrite(PIN_EINK_RES, HIGH);
    delay(10);
    wait_until_ready();
}

void set_full_ram_area()
{
    write_command(0x11);
    write_data(0x01);
    write_command(0x44);
    write_data(0x00);
    write_data(kPanelWidth / 8 - 1);
    write_command(0x45);
    write_data((kPanelHeight - 1) % 256);
    write_data((kPanelHeight - 1) / 256);
    write_data(0x00);
    write_data(0x00);
    write_command(0x4E);
    write_data(0x00);
    write_command(0x4F);
    write_data((kPanelHeight - 1) % 256);
    write_data((kPanelHeight - 1) / 256);
}

void initialize_panel()
{
    pinMode(PIN_EINK_CS, OUTPUT);
    digitalWrite(PIN_EINK_CS, HIGH);
    pinMode(PIN_EINK_DC, OUTPUT);
    pinMode(PIN_EINK_RES, OUTPUT);
    pinMode(PIN_EINK_BUSY, INPUT);

    SPI1.begin();
    SPI1.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    reset_panel();

    write_command(0x12);
    wait_until_ready();
    write_command(0x01);
    write_data(0xF9);
    write_data(0x00);
    write_data(0x00);
    set_full_ram_area();
    write_command(0x3C);
    write_data(0x05);
    write_command(0x21);
    write_data(0x00);
    write_data(0x80);
    write_command(0x18);
    write_data(0x80);
    wait_until_ready();
}

void write_framebuffer(uint8_t command)
{
    set_full_ram_area();
    write_command(command);
    for (uint16_t index = 0; index < kBufferSize; ++index) {
        write_data(framebuffer[index]);
    }
}

void refresh(bool fast)
{
    if (fast) {
        set_full_ram_area();
    }
    write_command(0x22);
    write_data(fast ? 0xFC : 0xF7);
    write_command(0x20);
    wait_until_ready();
}

void set_black_pixel(int16_t x, int16_t y)
{
    if (x < 0 || x >= kDisplayWidth || y < 0 || y >= kDisplayHeight) {
        return;
    }

    const uint16_t offset = static_cast<uint16_t>(x) * kBytesPerRow + static_cast<uint16_t>(y / 8);
    framebuffer[offset] &= ~(1U << (7 - (y % 8)));
}

int16_t text_width(const char *text)
{
    int16_t width = 0;
    while (*text++ != '\0') {
        width += kFontSpacing;
    }
    return width == 0 ? 0 : width - 1;
}

void draw_text(int16_t x, int16_t y, const char *text)
{
    for (const char *letter = text; *letter != '\0'; ++letter) {
        const uint8_t *glyph = glyph_for(*letter);
        for (uint8_t column = 0; column < kFontWidth; ++column) {
            for (uint8_t row = 0; row < kFontHeight; ++row) {
                if (glyph[column] & (1U << row)) {
                    set_black_pixel(x + column, y + row);
                }
            }
        }
        x += kFontSpacing;
    }
}

void refresh_fast()
{
    write_framebuffer(0x24);
    refresh(true);
    write_framebuffer(0x26);
}

} // namespace

void display_setup()
{
    pinMode(PIN_EINK_RES, INPUT);
    pinMode(PIN_EINK_BUSY, INPUT);
    pinMode(PIN_EINK_CS, INPUT);
    pinMode(PIN_EINK_DC, INPUT);
    pinMode(PIN_EINK_PWR, OUTPUT);
    digitalWrite(PIN_EINK_PWR, !kPowerOn);
}

void display_start()
{
    if (display_ready) {
        return;
    }

    digitalWrite(PIN_EINK_PWR, kPowerOn);
    delay(100);
    initialize_panel();
    memset(framebuffer, 0xFF, sizeof(framebuffer));
    write_framebuffer(0x24);
    refresh(false);
    write_framebuffer(0x26);
    display_ready = true;
}

void display_show_text(const char *text)
{
    if (!display_ready || text == nullptr) {
        return;
    }

    constexpr int16_t kLineSpacing = 1;
    constexpr int16_t kMaxCharacters = (kDisplayWidth - 16) / kFontSpacing;
    char line[kMaxCharacters + 1] = {};
    int16_t line_count = 1;
    int16_t line_length = 0;

    for (const char *letter = text; *letter != '\0'; ++letter) {
        if (*letter == '\r') {
            continue;
        }
        if (*letter == '\n' || ++line_length > kMaxCharacters) {
            ++line_count;
            line_length = *letter == '\n' ? 0 : 1;
        }
    }

    memset(framebuffer, 0xFF, sizeof(framebuffer));
    int16_t y = (kDisplayHeight - (line_count * kFontHeight + (line_count - 1) * kLineSpacing)) / 2;
    line_length = 0;

    for (const char *letter = text; *letter != '\0'; ++letter) {
        if (*letter == '\r') {
            continue;
        }
        if (*letter == '\n' || line_length == kMaxCharacters) {
            line[line_length] = '\0';
            draw_text((kDisplayWidth - text_width(line)) / 2, y, line);
            y += kFontHeight + kLineSpacing;
            line_length = 0;
        }
        if (*letter != '\n') {
            line[line_length++] = *letter;
        }
    }

    line[line_length] = '\0';
    draw_text((kDisplayWidth - text_width(line)) / 2, y, line);
    refresh_fast();
}
