#pragma once
#include "drivers/LedDriver.h"
#include "drivers/SegmentDriver.h"
#include "graphics/Effects.h"
#include "Config.h"

class DisplayManager {
public:
    DisplayManager();
    void begin();
    void update(const AppConfig& config);
    void startBootAnimation(); // 비동기 시작
    void stopBootAnimation();  // 종료
    void displayIP(uint32_t ipAddress); // IP 표시
    void displayPreset(int presetIndex);
    void displayTemporaryValue(int value);
    bool isBooting() const { return _isBooting; }

private:
    LedDriver _leds;
    SegmentDriver _seg;
    bool _isBooting = false;
    TaskHandle_t _bootTaskHandle = NULL;
    
    // 효과 전략들
    SolidEffect _solidEffect;
    RainbowEffect _rainbowEffect;
    TimeGradientEffect _timeGradEffect;
    SpaceGradientEffect _spaceGradEffect;

    IEffect* getEffect(int mode);
    void renderRing(int startIdx, int count, float progress, int colorMode, uint32_t c1, uint32_t c2, uint32_t cEmpty);
};
