#pragma once
#include "graphics/IEffect.h"
#include "graphics/ColorUtils.h"

// Mode 0: Solid Fill
class SolidEffect : public IEffect {
public:
    void render(LedDriver& leds, int startIdx, int count, float progress, uint32_t c1, uint32_t c2, uint32_t cEmpty) override {
        float currentPos = progress * count;
        
        for (int i = 0; i < count; i++) {
            int idx = startIdx + i;
            uint32_t col = cEmpty;
            
            if (currentPos >= i + 1) {
                col = c1;
            } else if (currentPos > i) {
                // Anti-aliasing for the edge
                col = ColorUtils::blend(cEmpty, c1, currentPos - i);
            }
            leds.setPixelColor(idx, col);
        }
    }
};

// Mode 1: Rainbow
class RainbowEffect : public IEffect {
public:
    void render(LedDriver& leds, int startIdx, int count, float progress, uint32_t c1, uint32_t c2, uint32_t cEmpty) override {
        float currentPos = progress * count;

        for (int i = 0; i < count; i++) {
            int idx = startIdx + i;
            uint32_t col = cEmpty;
            // Map position to Hue (0-65535)
            uint32_t targetColor = leds.ColorHSV(i * 65536L / count, 255, 255);

            if (currentPos >= i + 1) {
                col = targetColor;
            } else if (currentPos > i) {
                col = ColorUtils::blend(cEmpty, targetColor, currentPos - i);
            }
            leds.setPixelColor(idx, col);
        }
    }
};

// Mode 2: Time Gradient (Whole ring changes color over time/progress)
class TimeGradientEffect : public IEffect {
public:
    void render(LedDriver& leds, int startIdx, int count, float progress, uint32_t c1, uint32_t c2, uint32_t cEmpty) override {
        float currentPos = progress * count;
        uint32_t solidColor = ColorUtils::blend(c1, c2, progress);

        for (int i = 0; i < count; i++) {
            int idx = startIdx + i;
            uint32_t col = cEmpty;

            if (currentPos >= i + 1) {
                col = solidColor;
            } else if (currentPos > i) {
                col = ColorUtils::blend(cEmpty, solidColor, currentPos - i);
            }
            leds.setPixelColor(idx, col);
        }
    }
};

// Mode 3: Space Gradient (Start is c1, End is c2)
class SpaceGradientEffect : public IEffect {
public:
    void render(LedDriver& leds, int startIdx, int count, float progress, uint32_t c1, uint32_t c2, uint32_t cEmpty) override {
        float currentPos = progress * count;

        for (int i = 0; i < count; i++) {
            int idx = startIdx + i;
            uint32_t col = cEmpty;
            
            float ratio = (float)i / (float)(count - 1);
            uint32_t targetColor = ColorUtils::blend(c1, c2, ratio);

            if (currentPos >= i + 1) {
                col = targetColor;
            } else if (currentPos > i) {
                col = ColorUtils::blend(cEmpty, targetColor, currentPos - i);
            }
            leds.setPixelColor(idx, col);
        }
    }
};
