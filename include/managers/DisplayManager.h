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

private:
    LedDriver _leds;
    SegmentDriver _seg;
    
    // 효과 전략들
    SolidEffect _solidEffect;
    RainbowEffect _rainbowEffect;
    TimeGradientEffect _timeGradEffect;
    SpaceGradientEffect _spaceGradEffect;

    IEffect* getEffect(int mode);
    void renderRing(int startIdx, int count, float progress, int colorMode, uint32_t c1, uint32_t c2, uint32_t cEmpty);
};
