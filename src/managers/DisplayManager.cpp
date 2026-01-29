#include "managers/DisplayManager.h"
#include "TimeLogic.h"
#include <Arduino.h>

DisplayManager::DisplayManager() 
    : _leds(NUM_LEDS_TOTAL, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800),
      _seg(SCLK_PIN, LOAD_PIN, SDI_PIN) {}

void DisplayManager::begin() {
    _leds.begin();
    _leds.clear();
    _leds.show();
    _seg.begin();
}

IEffect* DisplayManager::getEffect(int mode) {
    switch (mode) {
        case 1:  return &_rainbowEffect;
        case 2:  return &_timeGradEffect;
        case 3:  return &_spaceGradEffect;
        default: return &_solidEffect;
    }
}

void DisplayManager::renderRing(int startIdx, int count, float progress, int colorMode, uint32_t c1, uint32_t c2, uint32_t cEmpty) {
    IEffect* effect = getEffect(colorMode);
    effect->render(_leds, startIdx, count, progress, c1, c2, cEmpty);
}

void DisplayManager::update(const AppConfig& config) {
    struct tm t;
    if (!getLocalTimeInfo(&t)) return;
    if (config.presets.empty()) return;

    int idx = config.currentPresetIndex;
    if (idx >= config.presets.size()) idx = 0;
    const Preset& p = config.presets[idx];

    // 1. 밝기 설정
    int finalBrightness = config.brightness;
    if (config.nightModeEnabled) {
        int h = t.tm_hour;
        bool isNight = (config.nightStartHour > config.nightEndHour) 
            ? (h >= config.nightStartHour || h < config.nightEndHour)
            : (h >= config.nightStartHour && h < config.nightEndHour);
        if (isNight) finalBrightness = config.nightBrightness;
    }
    _leds.setBrightness(finalBrightness);
    _leds.clear();

    // 2. Inner Ring
    String sDate = "", tDate = "";
    if (p.innerMode == 4 && p.innerDDayIndex < config.ddays.size()) {
        sDate = config.ddays[p.innerDDayIndex].startDate;
        tDate = config.ddays[p.innerDDayIndex].targetDate;
    }
    renderRing(NUM_LEDS_DUMMY, NUM_LEDS_INNER, 
               calculateProgress(p.innerMode, &t, sDate, tDate),
               p.innerColorMode, p.innerColorFill, p.innerColorFill2, p.innerColorEmpty);

    // 3. Outer Ring
    sDate = "", tDate = "";
    if (p.outerMode == 4 && p.outerDDayIndex < config.ddays.size()) {
        sDate = config.ddays[p.outerDDayIndex].startDate;
        tDate = config.ddays[p.outerDDayIndex].targetDate;
    }
    renderRing(NUM_LEDS_DUMMY + NUM_LEDS_INNER, NUM_LEDS_OUTER,
               calculateProgress(p.outerMode, &t, sDate, tDate),
               p.outerColorMode, p.outerColorFill, p.outerColorFill2, p.outerColorEmpty);

    _leds.show();

    // 4. 7-Segment (기존 로직 유지하며 드라이버 호출)
    int displayNum = 0;
    int dpPos = 0;
    int mode = (p.segMode == 0) ? 1 : p.segMode;

    if (mode == 1) {
        int total = isLeap(t.tm_year + 1900) ? 366 : 365;
        displayNum = total - t.tm_yday;
    } else if (mode == 2) {
        int daysInM = getDaysInMonth(t.tm_mon, t.tm_year + 1900);
        float left = (float)daysInM - ((t.tm_mday - 1) + (t.tm_hour / 24.0) + (t.tm_min / 1440.0));
        displayNum = (int)(max(0.0f, left) * 10);
        dpPos = 1;
    } else if (mode == 3) {
        int wday = (t.tm_wday + 6) % 7;
        float left = 7.0 - (wday + (t.tm_hour / 24.0) + (t.tm_min / 1440.0));
        displayNum = (int)(max(0.0f, left) * 100);
        dpPos = 2;
    } else if (mode == 4) {
        float left = 24.0 - (t.tm_hour + t.tm_min / 60.0);
        displayNum = (int)(max(0.0f, left) * 10);
        dpPos = 1;
    } else if (mode == 5 && p.segDDayIndex < config.ddays.size()) {
        time_t target = parseDate(config.ddays[p.segDDayIndex].targetDate);
        displayNum = (int)(difftime(target, mktime(&t)) / 86400.0);
        if (displayNum < 0) displayNum = 0;
    } else if (mode == 6) {
        int totalDays; float passedDays;
        getQuarterInfo(&t, totalDays, passedDays);
        displayNum = (int)(max(0.0f, (float)totalDays - passedDays) * 10);
        dpPos = 1;
    }

    _seg.drawNumber(min(displayNum, 999), dpPos, false);
}
