#include "drivers/LedDriver.h"

LedDriver::LedDriver(uint16_t numLeds, int16_t pin, neoPixelType type)
    : pixels(numLeds, pin, type) {}

void LedDriver::begin() {
    pixels.begin();
}

void LedDriver::show() {
    pixels.show();
}

void LedDriver::clear() {
    pixels.clear();
}

void LedDriver::setBrightness(uint8_t brightness) {
    pixels.setBrightness(brightness);
}

void LedDriver::setPixelColor(uint16_t n, uint32_t c) {
    pixels.setPixelColor(n, c);
}

void LedDriver::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(n, r, g, b);
}

uint32_t LedDriver::Color(uint8_t r, uint8_t g, uint8_t b) {
    return pixels.Color(r, g, b);
}

uint32_t LedDriver::ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
    return pixels.ColorHSV(hue, sat, val);
}

uint16_t LedDriver::numPixels() const {
    return pixels.numPixels();
}
