#include "DisplayManager.h"
#include "TimeLogic.h"
#include "Config.h"

Adafruit_NeoPixel pixels(NUM_LEDS_TOTAL, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// [수정됨] 사용자가 수정한 올바른 비트 패턴
const byte digitPatterns[] = {
    0b11000000, 0b11111001, 0b10100100, 0b10110000, 0b10011001,
    0b10010010, 0b10000010, 0b11111000, 0b10000000, 0b10010000};

// 함수 원형 수정 (매개변수 추가)
void show7Seg(int num, int dpPos, bool isOff);
void drawSmoothRing(int startIdx, int count, float progress, int cMode, uint32_t c1, uint32_t c2, uint32_t cEmpty);
uint32_t blendColor(uint32_t c1, uint32_t c2, float r);
uint32_t getRainbowColor(int pos, int total);

void setupDisplay()
{
    pixels.begin();
    pixels.clear();
    pixels.show();
    pinMode(SCLK_PIN, OUTPUT);
    pinMode(LOAD_PIN, OUTPUT);
    pinMode(SDI_PIN, OUTPUT);
}

void updateDisplay(AppConfig config)
{
    struct tm t;
    if (!getLocalTimeInfo(&t))
        return;
    if (config.presets.empty())
        return;

    int idx = config.currentPresetIndex;
    if (idx >= config.presets.size())
        idx = 0;
    Preset p = config.presets[idx];

    // 야간 모드 로직 (기존 동일)
    int finalBrightness = config.brightness;
    if (config.nightModeEnabled)
    {
        int h = t.tm_hour;
        bool isNight = false;
        if (config.nightStartHour > config.nightEndHour)
        {
            if (h >= config.nightStartHour || h < config.nightEndHour)
                isNight = true;
        }
        else
        {
            if (h >= config.nightStartHour && h < config.nightEndHour)
                isNight = true;
        }
        if (isNight)
            finalBrightness = config.nightBrightness;
    }
    pixels.setBrightness(finalBrightness);
    pixels.clear();

    // --- Inner Ring ---
    String sDate = "", tDate = "";
    if (p.innerMode == 4 && p.innerDDayIndex < config.ddays.size())
    {
        sDate = config.ddays[p.innerDDayIndex].startDate;
        tDate = config.ddays[p.innerDDayIndex].targetDate;
    }
    drawSmoothRing(NUM_LEDS_DUMMY, NUM_LEDS_INNER,
                   calculateProgress(p.innerMode, &t, sDate, tDate),
                   p.innerColorMode, p.innerColorFill, p.innerColorFill2, p.innerColorEmpty);

    // --- Outer Ring ---
    sDate = "";
    tDate = "";
    if (p.outerMode == 4 && p.outerDDayIndex < config.ddays.size())
    {
        sDate = config.ddays[p.outerDDayIndex].startDate;
        tDate = config.ddays[p.outerDDayIndex].targetDate;
    }
    drawSmoothRing(NUM_LEDS_DUMMY + NUM_LEDS_INNER, NUM_LEDS_OUTER,
                   calculateProgress(p.outerMode, &t, sDate, tDate),
                   p.outerColorMode, p.outerColorFill, p.outerColorFill2, p.outerColorEmpty);

    pixels.show();

    // --- 7 Segment ---
    // (기존 7세그먼트 로직 그대로 유지)
    int displayNum = 0;
    int dpPos = 0;
    int mode = p.segMode;
    if (mode == 0)
        mode = 1; // Default Year

    if (mode == 1)
    {
        int total = isLeap(t.tm_year + 1900) ? 366 : 365;
        displayNum = total - t.tm_yday;
        dpPos = 0;
    }
    else if (mode == 2)
    {
        int daysInM = getDaysInMonth(t.tm_mon, t.tm_year + 1900);
        float passed = (t.tm_mday - 1) + (t.tm_hour / 24.0) + (t.tm_min / 1440.0);
        float left = (float)daysInM - passed;
        displayNum = (int)(left * 10);
        if (displayNum < 0)
            displayNum = 0;
        dpPos = 1;
    }
    else if (mode == 3)
    {
        int wday = (t.tm_wday + 6) % 7;
        float passed = wday + (t.tm_hour / 24.0) + (t.tm_min / 1440.0);
        float left = 7.0 - passed;
        displayNum = (int)(left * 100);
        if (displayNum < 0)
            displayNum = 0;
        dpPos = 2;
    }
    else if (mode == 4)
    {
        float left = 24.0 - (t.tm_hour + t.tm_min / 60.0);
        displayNum = (int)(left * 10);
        if (displayNum < 0)
            displayNum = 0;
        dpPos = 1;
    }
    else if (mode == 5)
    {
        if (p.segDDayIndex < config.ddays.size())
        {
            time_t target = parseDate(config.ddays[p.segDDayIndex].targetDate);
            time_t now = mktime(&t);
            double diff = difftime(target, now);
            displayNum = (int)(diff / 86400.0);
            if (displayNum < 0)
                displayNum = 0;
        }
        dpPos = 0;
    }
    else if (mode == 6)
    {
        int totalDays;
        float passedDays;
        getQuarterInfo(&t, totalDays, passedDays);
        float left = (float)totalDays - passedDays;
        displayNum = (int)(left * 10);
        if (displayNum < 0)
            displayNum = 0;
        dpPos = 1;
    }

    if (displayNum > 999)
        displayNum = 999;
    show7Seg(displayNum, dpPos, false);
}

// [핵심] 링 그리기 로직 개선
void drawSmoothRing(int startIdx, int count, float progress, int cMode, uint32_t c1, uint32_t c2, uint32_t cEmpty)
{
    float currentPos = progress * count;

    // 모드 2: 진행도 그라데이션 (Time Gradient) - 전체 색상이 시간에 따라 변함
    uint32_t solidColor = c1;
    if (cMode == 2)
    {
        solidColor = blendColor(c1, c2, progress);
    }

    for (int i = 0; i < count; i++)
    {
        int idx = startIdx + i;
        uint32_t col = cEmpty; // 기본은 빈 색

        // 채워질 색상 결정
        uint32_t targetColor = solidColor;

        if (cMode == 1)
        {
            // 모드 1: 무지개 (Rainbow)
            // 전체 링을 0~65535 Hue로 매핑
            targetColor = pixels.ColorHSV(i * 65536L / count, 255, 255);
        }
        else if (cMode == 3)
        {
            // 모드 3: 고정 그라데이션 (Space Gradient)
            // 픽셀 위치(i)에 따라 c1 -> c2로 변함
            float ratio = (float)i / (float)(count - 1);
            targetColor = blendColor(c1, c2, ratio);
        }

        // 픽셀 채우기 로직 (안티앨리어싱 포함)
        if (currentPos >= i + 1)
        {
            // 완전히 채워진 픽셀
            col = targetColor;
        }
        else if (currentPos > i)
        {
            // 경계면 픽셀 (부드러운 처리)
            // 배경색(cEmpty)과 타겟색(targetColor)을 비율만큼 섞음
            col = blendColor(cEmpty, targetColor, currentPos - i);
        }

        pixels.setPixelColor(idx, col);
    }
}

// 두 색상 섞기 (ratio 0.0 ~ 1.0)
uint32_t blendColor(uint32_t c1, uint32_t c2, float r)
{
    if (r <= 0.0)
        return c1;
    if (r >= 1.0)
        return c2;
    uint8_t r1 = (c1 >> 16) & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = c1 & 0xFF;
    uint8_t r2 = (c2 >> 16) & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = c2 & 0xFF;

    return pixels.Color(
        r1 + (r2 - r1) * r,
        g1 + (g2 - g1) * r,
        b1 + (b2 - b1) * r);
}

void show7Seg(int num, int dpPos, bool isOff)
{
    if (isOff)
    {
        digitalWrite(LOAD_PIN, LOW);
        shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, 0xFF);
        shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, 0xFF);
        shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, 0xFF);
        digitalWrite(LOAD_PIN, HIGH);
        return;
    }
    int h = (num / 100) % 10;
    int t = (num / 10) % 10;
    int o = num % 10;
    byte pH = digitPatterns[h];
    byte pT = digitPatterns[t];
    byte pO = digitPatterns[o];
    if (dpPos == 2)
        pH &= 0x7F;
    if (dpPos == 1)
        pT &= 0x7F;
    digitalWrite(LOAD_PIN, LOW);
    shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, pO);
    shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, pT);
    shiftOut(SDI_PIN, SCLK_PIN, MSBFIRST, pH);
    digitalWrite(LOAD_PIN, HIGH);
}
