#pragma once
#include <Arduino.h>

class SegmentDriver {
public:
    SegmentDriver(int sclkPin, int loadPin, int sdiPin);
    void begin();
    void drawNumber(int num, int dpPos, bool isOff);
    void drawRaw(byte h, byte t, byte o); // 세그먼트 직접 제어 추가
    void test();

    const byte digitPatterns[10] = {
        0b11000000, 0b11111001, 0b10100100, 0b10110000, 0b10011001,
        0b10010010, 0b10000010, 0b11111000, 0b10000000, 0b10010000
    };

private:
    int _sclkPin;
    int _loadPin;
    int _sdiPin;
};
