#include "managers/DisplayManager.h"
#include "managers/InteractiveManager.h"
#include "TimeLogic.h"
#include <Arduino.h>

DisplayManager::DisplayManager() 
    : _leds(NUM_LEDS_INNER, PIN_INNER, NUM_LEDS_OUTER, PIN_OUTER, NEO_GRB + NEO_KHZ800),
      _seg(SCLK_PIN, LOAD_PIN, SDI_PIN) {}
void DisplayManager::begin()
{
    _leds.begin();
    _leds.clear();
    _leds.show();
    _seg.begin();
    // 시작 시 비동기로 애니메이션 구동
    startBootAnimation();
}

static void bootAnimationTask(void *pvParameters)
{
    DisplayManager *dm = (DisplayManager *)pvParameters;
    LedDriver &leds = *(LedDriver *)((uint8_t *)dm + 0); // LedDriver 위치 (구조에 따라 주의)
    // 안전한 접근을 위해 DisplayManager 내부에 static wrapper나 friend class 고려 가능하나
    // 여기서는 로직 구현을 위해 멤버 함수를 태스크 내부 루프로 이동하는 방식으로 수정 제안
}

// 태스크 함수를 멤버 함수 호출 형태로 구현하기 위해 DisplayManager.cpp 수정
void DisplayManager::startBootAnimation()
{
    _isBooting = true;
    auto taskFn = [](void *p)
    {
        DisplayManager *self = (DisplayManager *)p;
        const byte segPatterns[] = {0b11111110, 0b11111101, 0b11111011, 0b11110111, 0b11101111, 0b11011111};
        int i = 0;
        self->_seg.drawRaw(0b11011100, 0b11100011, 0b11011100);
        while (self->_isBooting)
        {
            self->_leds.clear();

            // 무지개 색상 계산 (i값에 따라 변함)
            uint32_t rainbowColor = self->_leds.ColorHSV((i * 1024) % 65536, 255, 255);
            uint32_t rainbowColor2 = self->_leds.ColorHSV(((i + 10) * 1024) % 65536, 255, 255);

            // Outer Ring (정방향)
            int outerIdx = NUM_LEDS_INNER + (i % NUM_LEDS_OUTER);
            self->_leds.setPixelColor(outerIdx, rainbowColor);
            self->_leds.setPixelColor((i + 1) % NUM_LEDS_OUTER + NUM_LEDS_INNER, rainbowColor);

            // Inner Ring (역방향)
            int innerIdx = (NUM_LEDS_INNER - 1 - (i % NUM_LEDS_INNER));
            self->_leds.setPixelColor(innerIdx, rainbowColor2);

            // 7-Segment (무지개 색상과 동기화된 회전)
            // byte pSeg = segPatterns[i % 6];
            // self->_seg.drawRaw(pSeg, pSeg, pSeg);

            self->_leds.show();
            i++;
            vTaskDelay(pdMS_TO_TICKS(40));
        }
        vTaskDelete(NULL);
    };

    BaseType_t taskResult = pdFAIL;
#if defined(CONFIG_FREERTOS_NUMBER_OF_CORES) && (CONFIG_FREERTOS_NUMBER_OF_CORES > 1)
    taskResult = xTaskCreatePinnedToCore(taskFn, "bootTask", 4096, this, 1, &_bootTaskHandle, 1);
#else
    taskResult = xTaskCreate(taskFn, "bootTask", 4096, this, 1, &_bootTaskHandle);
#endif

    if (taskResult != pdPASS)
    {
        _isBooting = false;
        _bootTaskHandle = NULL;
    }
}

void DisplayManager::stopBootAnimation()
{
    _isBooting = false;
    // 태스크가 종료될 때까지 잠시 대기
    delay(100);
    _leds.clear();
    _leds.show();
}

void DisplayManager::displayIP(uint32_t ipAddress)
{
    // IPAddress를 4개의 옥텟으로 분리 (Little Endian 기준)
    uint8_t octets[4];
    octets[0] = ipAddress & 0xFF;
    octets[1] = (ipAddress >> 8) & 0xFF;
    octets[2] = (ipAddress >> 16) & 0xFF;
    octets[3] = (ipAddress >> 24) & 0xFF;

    // "IP " 표시 (I: 0b11111001, P: 0b10001100)
    _seg.drawRaw(0b11111001, 0b10001100, 0xFF);
    delay(1000);

    for (int i = 0; i < 4; i++)
    {
        _seg.drawNumber(octets[i], 0, false);
        delay(800);
        // 마지막 옥텟이 아니면 깜빡임으로 구분
        if (i < 3) {
            _seg.drawNumber(0, 0, true);
            delay(200);
        }
    }
    delay(500);
}

void DisplayManager::displayPreset(int presetIndex)
{
    // P (Common Anode: 0x8C)
    // Space (0xFF)
    // Number
    int num = (presetIndex + 1) % 10;
    byte pNum = _seg.digitPatterns[num];
    _seg.drawRaw(0x8C, 0xFF, pNum);
}

void DisplayManager::displayTemporaryValue(int value)
{
    _seg.drawNumber(min(value, 999), 0, false);
}

IEffect *DisplayManager::getEffect(int mode)
{
    switch (mode)
    {
    case 1:
        return &_rainbowEffect;
    case 2:
        return &_timeGradEffect;
    case 3:
        return &_spaceGradEffect;
    default:
        return &_solidEffect;
    }
}

void DisplayManager::renderRing(int startIdx, int count, float progress, int colorMode, uint32_t c1, uint32_t c2, uint32_t cEmpty)
{
    IEffect *effect = getEffect(colorMode);
    effect->render(_leds, startIdx, count, progress, c1, c2, cEmpty);
}

void DisplayManager::update(const AppConfig &config)
{
    struct tm t;
    if (!getLocalTimeInfo(&t))
        return;
    if (config.presets.empty())
        return;

    int idx = config.currentPresetIndex;
    if (idx >= config.presets.size())
        idx = 0;
    const Preset &p = config.presets[idx];

    // 1. 밝기 설정
    int finalBrightness = config.brightness;
    if (config.nightModeEnabled)
    {
        int h = t.tm_hour;
        bool isNight = (config.nightStartHour > config.nightEndHour)
                           ? (h >= config.nightStartHour || h < config.nightEndHour)
                           : (h >= config.nightStartHour && h < config.nightEndHour);
        if (isNight)
            finalBrightness = config.nightBrightness;
    }
    _leds.setBrightness(finalBrightness);
    _leds.clear();

    // Blink Logic for Pomodoro (Global check if any ring is Pomodoro)
    bool blink = false;
    if (p.inner.mode == MODE_POMODORO && interactiveManager.shouldBlink(MODE_POMODORO)) blink = true;
    else if (p.outer.mode == MODE_POMODORO && interactiveManager.shouldBlink(MODE_POMODORO)) blink = true;

    // 뽀모도로 대기 상태에서는 LED만 깜빡이고 7-Seg는 계속 표시한다.
    bool skipLedRender = blink && ((millis() / 500) % 2 == 0);
    if (!skipLedRender)
    {
        // 2. Inner Ring
        {
            float prog = 0.0f;
            if (isInteractiveMode(p.inner.mode)) {
                prog = interactiveManager.getProgress(p.inner);
            } else {
                String sDate = "", tDate = "";
                if (p.inner.mode == 4 &&
                    p.inner.payload.kind == PAYLOAD_DDAY &&
                    p.inner.payload.value.ddayIndex < (int)config.ddays.size())
                {
                    int ddayIndex = p.inner.payload.value.ddayIndex;
                    sDate = config.ddays[ddayIndex].startDate;
                    tDate = config.ddays[ddayIndex].targetDate;
                }
                prog = calculateProgress(p.inner.mode, &t, sDate, tDate);
            }
            renderRing(0, NUM_LEDS_INNER, prog,
                    p.inner.colorMode, p.inner.colorFill, p.inner.colorFill2, p.inner.colorEmpty);
        }

        // 3. Outer Ring
        {
            float prog = 0.0f;
            if (isInteractiveMode(p.outer.mode)) {
                prog = interactiveManager.getProgress(p.outer);
            } else {
                String sDate = "", tDate = "";
                if (p.outer.mode == 4 &&
                    p.outer.payload.kind == PAYLOAD_DDAY &&
                    p.outer.payload.value.ddayIndex < (int)config.ddays.size())
                {
                    int ddayIndex = p.outer.payload.value.ddayIndex;
                    sDate = config.ddays[ddayIndex].startDate;
                    tDate = config.ddays[ddayIndex].targetDate;
                }
                prog = calculateProgress(p.outer.mode, &t, sDate, tDate);
            }
            renderRing(NUM_LEDS_INNER, NUM_LEDS_OUTER, prog,
                    p.outer.colorMode, p.outer.colorFill, p.outer.colorFill2, p.outer.colorEmpty);
        }
    }
    _leds.show();

    // 4. 7-Segment (기존 로직 유지하며 드라이버 호출)
    int displayNum = 0;
    int dpPos = 0;
    int mode = p.segment.mode;

    // 타이머나 뽀모도로가 '동작 중'이면 자동으로 해당 모드를 표시 (사용자 설정보다 우선)
    if (p.inner.mode == MODE_TIMER && interactiveManager.isTimerRunning()) mode = MODE_TIMER;
    else if (p.inner.mode == MODE_POMODORO && interactiveManager.isPomoRunning()) mode = MODE_POMODORO;
    else if (p.outer.mode == MODE_TIMER && interactiveManager.isTimerRunning()) mode = MODE_TIMER;
    else if (p.outer.mode == MODE_POMODORO && interactiveManager.isPomoRunning()) mode = MODE_POMODORO;
    else {
        // 동작 중인 것이 없으면 기존 설정값(0일 경우 1로 처리) 사용
        if (mode == 0) mode = 1;
    }

    if (mode >= 10) {
        // Interactive Modes
        displayNum = interactiveManager.getDisplayNumber(mode);
    }
    else if (mode == 1)
    {
        int total = isLeap(t.tm_year + 1900) ? 366 : 365;
        displayNum = total - t.tm_yday;
    }
    else if (mode == 2)
    {
        int daysInM = getDaysInMonth(t.tm_mon, t.tm_year + 1900);
        float left = (float)daysInM - ((t.tm_mday - 1) + (t.tm_hour / 24.0) + (t.tm_min / 1440.0));
        displayNum = (int)(max(0.0f, left) * 10);
        dpPos = 1;
    }
    else if (mode == 3)
    {
        int wday = (t.tm_wday + 6) % 7;
        float left = 7.0 - (wday + (t.tm_hour / 24.0) + (t.tm_min / 1440.0));
        displayNum = (int)(max(0.0f, left) * 100);
        dpPos = 2;
    }
    else if (mode == 4)
    {
        float left = 24.0 - (t.tm_hour + t.tm_min / 60.0);
        displayNum = (int)(max(0.0f, left) * 10);
        dpPos = 1;
    }
    else if (mode == 5 &&
             p.segment.payload.kind == PAYLOAD_DDAY &&
             p.segment.payload.value.ddayIndex < (int)config.ddays.size())
    {
        int ddayIndex = p.segment.payload.value.ddayIndex;
        time_t target = parseDate(config.ddays[ddayIndex].targetDate);
        displayNum = (int)(difftime(target, mktime(&t)) / 86400.0);
        if (displayNum < 0)
            displayNum = 0;
    }
    else if (mode == 6)
    {
        int totalDays;
        float passedDays;
        getQuarterInfo(&t, totalDays, passedDays);
        displayNum = (int)(max(0.0f, (float)totalDays - passedDays) * 10);
        dpPos = 1;
    }

    _seg.drawNumber(min(displayNum, 999), dpPos, false);
}
