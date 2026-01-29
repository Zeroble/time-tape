#pragma once
#include "drivers/LedDriver.h"

class IEffect {
public:
    virtual ~IEffect() {}
    
    // Updates the effect state. 
    // progress: 0.0 to 1.0 (how much the ring is filled)
    // startIdx, count: range of LEDs to affect
    virtual void render(LedDriver& leds, int startIdx, int count, float progress, uint32_t c1, uint32_t c2, uint32_t cEmpty) = 0;
};
