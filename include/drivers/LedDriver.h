#pragma once
#include <Adafruit_NeoPixel.h>

class LedDriver {
public:
    LedDriver(uint16_t innerCount, int16_t innerPin, uint16_t outerCount, int16_t outerPin, neoPixelType type);
    void begin();
    void show();
    void clear();
    void setBrightness(uint8_t brightness);
    
    // 이 메서드들은 내부적으로 어느 스트립인지 판단해서 호출하거나 통합 제어합니다.
    void setPixelColor(uint16_t n, uint32_t c); 
    
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    uint32_t ColorHSV(uint16_t hue, uint8_t sat, uint8_t val);

private:
    Adafruit_NeoPixel _inner;
    Adafruit_NeoPixel _outer;
    uint16_t _innerCount;
    uint16_t _outerCount;
};