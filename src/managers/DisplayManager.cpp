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
    xTaskCreatePinnedToCore(
        [](void *p)
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
        },
        "bootTask",
        4096,
        this,
        1,
        &_bootTaskHandle,
        1 // Core 1에서 실행
    );
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
    if (p.innerMode == MODE_POMODORO && interactiveManager.shouldBlink(MODE_POMODORO)) blink = true;
    else if (p.outerMode == MODE_POMODORO && interactiveManager.shouldBlink(MODE_POMODORO)) blink = true;

    if (blink) {
        // 1초 주기 깜빡임
        if ((millis() / 500) % 2 == 0) {
            _leds.show(); // Clear상태로 show -> 끄기
            // 7-Seg 표시 로직은 아래에서 계속 실행됨 (깜빡임과 무관하게 켜져있거나 꺼짐)
            // LED만 깜빡이는 것
            // continue; // 이렇게 하면 Segment도 안그려질 수 있음.
            // 그냥 LED 렌더링을 스킵하면 됨.
        } else {
             // 켜지는 턴 -> 아래 렌더링 로직 실행
             // 복잡하니 그냥 여기서 return 처리하면 7seg가 안됨.
             // 렌더링 로직 내부에서 blink 처리?
             // 가장 쉬운 방법: finalBrightness 조절?
             // 이미 clear() 호출됨. 렌더링 함수를 안 부르면 됨.
             goto RENDER_SEGMENT; 
        }
    }

    // 2. Inner Ring
    {
        float prog = 0.0f;
        if (p.innerMode >= 10) {
            prog = interactiveManager.getProgress(p.innerMode, NUM_LEDS_INNER);
        } else {
            String sDate = "", tDate = "";
            if (p.innerMode == 4 && p.id_dd < config.ddays.size())
            {
                sDate = config.ddays[p.id_dd].startDate;
                tDate = config.ddays[p.id_dd].targetDate;
            }
            prog = calculateProgress(p.innerMode, &t, sDate, tDate);
        }
        renderRing(0, NUM_LEDS_INNER, prog,
                   p.innerColorMode, p.innerColorFill, p.innerColorFill2, p.innerColorEmpty);
    }

    // 3. Outer Ring
    {
        float prog = 0.0f;
        if (p.outerMode >= 10) {
            prog = interactiveManager.getProgress(p.outerMode, NUM_LEDS_OUTER);
        } else {
            String sDate = "", tDate = "";
            if (p.outerMode == 4 && p.outerDDayIndex < config.ddays.size())
            {
                sDate = config.ddays[p.outerDDayIndex].startDate;
                tDate = config.ddays[p.outerDDayIndex].targetDate;
            }
            prog = calculateProgress(p.outerMode, &t, sDate, tDate);
        }
        renderRing(NUM_LEDS_INNER, NUM_LEDS_OUTER, prog,
                   p.outerColorMode, p.outerColorFill, p.outerColorFill2, p.outerColorEmpty);
    }
    
    _leds.show();

RENDER_SEGMENT:
    // 4. 7-Segment (기존 로직 유지하며 드라이버 호출)
    int displayNum = 0;
    int dpPos = 0;
    int mode = p.segMode;

    // 타이머나 뽀모도로가 '동작 중'이면 자동으로 해당 모드를 표시 (사용자 설정보다 우선)
    if (p.innerMode == MODE_TIMER && interactiveManager.isTimerRunning()) mode = MODE_TIMER;
    else if (p.innerMode == MODE_POMODORO && interactiveManager.isPomoRunning()) mode = MODE_POMODORO;
    else if (p.outerMode == MODE_TIMER && interactiveManager.isTimerRunning()) mode = MODE_TIMER;
    else if (p.outerMode == MODE_POMODORO && interactiveManager.isPomoRunning()) mode = MODE_POMODORO;
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
    else if (mode == 5 && p.segDDayIndex < config.ddays.size())
    {
        time_t target = parseDate(config.ddays[p.segDDayIndex].targetDate);
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
