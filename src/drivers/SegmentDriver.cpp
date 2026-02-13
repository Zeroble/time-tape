#include "drivers/SegmentDriver.h"

SegmentDriver::SegmentDriver(int sclkPin, int loadPin, int sdiPin)
    : _sclkPin(sclkPin), _loadPin(loadPin), _sdiPin(sdiPin) {}

void SegmentDriver::begin() {
    pinMode(_sclkPin, OUTPUT);
    pinMode(_loadPin, OUTPUT);
    pinMode(_sdiPin, OUTPUT);
}

void SegmentDriver::drawRaw(byte h, byte t, byte o) {
    digitalWrite(_loadPin, LOW);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, o);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, t);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, h);
    digitalWrite(_loadPin, HIGH);
}

void SegmentDriver::drawNumber(int num, int dpPos, bool isOff) {
    if (isOff) {
        digitalWrite(_loadPin, LOW);
        shiftOut(_sdiPin, _sclkPin, MSBFIRST, 0xFF);
        shiftOut(_sdiPin, _sclkPin, MSBFIRST, 0xFF);
        shiftOut(_sdiPin, _sclkPin, MSBFIRST, 0xFF);
        digitalWrite(_loadPin, HIGH);
        return;
    }

    int h = (num / 100) % 10;
    int t = (num / 10) % 10;
    int o = num % 10;

    byte pH = digitPatterns[h];
    byte pT = digitPatterns[t];
    byte pO = digitPatterns[o];

    if (dpPos == 2) pH &= 0x7F;
    if (dpPos == 1) pT &= 0x7F;

    digitalWrite(_loadPin, LOW);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, pO);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, pT);
    shiftOut(_sdiPin, _sclkPin, MSBFIRST, pH);
    digitalWrite(_loadPin, HIGH);
}

void SegmentDriver::test() {
    // Implement test pattern if needed
}
