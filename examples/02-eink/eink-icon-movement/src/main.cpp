#include <Arduino.h>
#include <button.h>
#include <elink_gdem0213b74.h>

constexpr int16_t kIconSize = 16;
constexpr int16_t kStep = 8;
constexpr uint8_t kFullRefreshEveryMoves = 4;
constexpr uint8_t kButtonUp = 0;
constexpr uint8_t kButtonLeft = 1;
constexpr uint8_t kButtonRight = 2;
constexpr uint8_t kButtonDown = 3;

const uint8_t kIconBits[] = {
    0x03, 0xC0, 0x0C, 0x30, 0x10, 0x08, 0x20, 0x04,
    0x40, 0x02, 0x40, 0x02, 0x80, 0x01, 0x80, 0x01,
    0x80, 0x01, 0x80, 0x01, 0x40, 0x02, 0x40, 0x02,
    0x20, 0x04, 0x10, 0x08, 0x0C, 0x30, 0x03, 0xC0,
};

int16_t icon_x = 0;
int16_t icon_y = 0;
uint8_t moves_since_full_refresh = 0;

void draw_icon(bool full_refresh)
{
    elink_gdem0213b74_clear(false);
    elink_gdem0213b74_draw_bitmap(icon_x, icon_y, kIconSize, kIconSize, kIconBits);
    elink_gdem0213b74_refresh(full_refresh);
}

void move_icon(int16_t dx, int16_t dy)
{
    const int16_t max_x = elink_gdem0213b74_width() - kIconSize;
    const int16_t max_y = elink_gdem0213b74_height() - kIconSize;
    const int16_t next_x = constrain(icon_x + dx, 0, max_x);
    const int16_t next_y = constrain(icon_y + dy, 0, max_y);

    if (next_x == icon_x && next_y == icon_y) {
        return;
    }

    icon_x = next_x;
    icon_y = next_y;
    moves_since_full_refresh++;

    const bool full_refresh = moves_since_full_refresh >= kFullRefreshEveryMoves;
    draw_icon(full_refresh);

    if (full_refresh) {
        moves_since_full_refresh = 0;
    }
}

void on_button_release(uint8_t index, btn_event_t event, void *)
{
    if (event != BTN_EVENT_PRESS_UP) {
        return;
    }

    switch (index) {
    case kButtonUp:
        move_icon(0, -kStep);
        break;
    case kButtonDown:
        move_icon(0, kStep);
        break;
    case kButtonLeft:
        move_icon(-kStep, 0);
        break;
    case kButtonRight:
        move_icon(kStep, 0);
        break;
    default:
        break;
    }
}

void setup()
{
    elink_gdem0213b74_setup();
    button_setup();
    (void)elink_gdem0213b74_start();

    icon_x = (elink_gdem0213b74_width() - kIconSize) / 2;
    icon_y = (elink_gdem0213b74_height() - kIconSize) / 2;
    draw_icon(true);

    btn_register_cb(kButtonUp, BTN_EVENT_PRESS_UP, on_button_release, nullptr);
    btn_register_cb(kButtonDown, BTN_EVENT_PRESS_UP, on_button_release, nullptr);
    btn_register_cb(kButtonLeft, BTN_EVENT_PRESS_UP, on_button_release, nullptr);
    btn_register_cb(kButtonRight, BTN_EVENT_PRESS_UP, on_button_release, nullptr);
}

void loop()
{
    button_loop();
    delay(1);
}
