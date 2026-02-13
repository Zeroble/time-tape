
#pragma once
#include <Arduino.h>

class ColorUtils {
public:
    static uint32_t blend(uint32_t c1, uint32_t c2, float r) {
        if (r <= 0.0f) return c1;
        if (r >= 1.0f) return c2;
        
        uint8_t r1 = (uint8_t)(c1 >> 16);
        uint8_t g1 = (uint8_t)(c1 >> 8);
        uint8_t b1 = (uint8_t)c1;
        
        uint8_t r2 = (uint8_t)(c2 >> 16);
        uint8_t g2 = (uint8_t)(c2 >> 8);
        uint8_t b2 = (uint8_t)c2;
        
        return ((uint32_t)(r1 + (r2 - r1) * r) << 16) |
               ((uint32_t)(g1 + (g2 - g1) * r) << 8) |
               (uint32_t)(b1 + (b2 - b1) * r);
    }
};
