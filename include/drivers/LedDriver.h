#pragma once
#include <Adafruit_NeoPixel.h>

class LedDriver {
public:
    LedDriver(uint16_t numLeds, int16_t pin, neoPixelType type);
    void begin();
    void show();
    void clear();
    void setBrightness(uint8_t brightness);
    void setPixelColor(uint16_t n, uint32_t c);
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    uint32_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val);
    uint16_t numPixels() const;

private:
    Adafruit_NeoPixel pixels;
};
