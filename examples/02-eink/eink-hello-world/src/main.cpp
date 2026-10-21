#include <Arduino.h>
#include <elink_gdem0213b74.h>

void setup()
{
    elink_gdem0213b74_setup();
    (void)elink_gdem0213b74_start();
    elink_gdem0213b74_show_text_centered("HELLO,WORLD!");
}

void loop()
{
    delay(1000);
}
