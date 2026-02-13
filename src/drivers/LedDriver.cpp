#include "drivers/LedDriver.h"

LedDriver::LedDriver(uint16_t innerCount, int16_t innerPin, uint16_t outerCount, int16_t outerPin, neoPixelType type)
    : _inner(innerCount, innerPin, type), 
      _outer(outerCount, outerPin, type),
      _innerCount(innerCount),
      _outerCount(outerCount) {}

void LedDriver::begin() {
    _inner.begin();
    _outer.begin();
}

void LedDriver::show() {
    _inner.show();
    _outer.show();
}

void LedDriver::clear() {
    _inner.clear();
    _outer.clear();
}

void LedDriver::setBrightness(uint8_t brightness) {
    _inner.setBrightness(brightness);
    _outer.setBrightness(brightness);
}

// n이 0 ~ (innerCount-1) 이면 안쪽, 그 이상이면 바깥쪽으로 자동 매핑
void LedDriver::setPixelColor(uint16_t n, uint32_t c) {
    if (n < _innerCount) {
        _inner.setPixelColor(n, c);
    } else {
        _outer.setPixelColor(n - _innerCount, c);
    }
}

uint32_t LedDriver::Color(uint8_t r, uint8_t g, uint8_t b) {
    return _inner.Color(r, g, b); // 어느쪽을 쓰든 동일
}

uint32_t LedDriver::ColorHSV(uint16_t hue, uint8_t sat, uint8_t val) {
    return _inner.ColorHSV(hue, sat, val);
}