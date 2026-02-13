#include "managers/InteractiveManager.h"

InteractiveManager interactiveManager;

static const ModePayload *findPayloadForMode(const Preset &p, int mode)
{
    if (p.inner.mode == mode)
    {
        return &p.inner.payload;
    }
    if (p.outer.mode == mode)
    {
        return &p.outer.payload;
    }
    return nullptr;
}

static long getCounterTargetFromPayload(const ModePayload *payload)
{
    if (payload && payload->kind == PAYLOAD_COUNTER && payload->value.counterTarget > 0)
    {
        return payload->value.counterTarget;
    }
    return 100;
}

static long getTimerSecondsFromPayload(const ModePayload *payload)
{
    if (payload && payload->kind == PAYLOAD_TIMER && payload->value.timer.totalSeconds > 0)
    {
        return payload->value.timer.totalSeconds;
    }
    return 60;
}

static bool getTimerDisplaySecondsFromPayload(const ModePayload *payload)
{
    if (payload && payload->kind == PAYLOAD_TIMER)
    {
        return payload->value.timer.displaySeconds;
    }
    return false;
}

static void getPomodoroConfigFromPayload(const ModePayload *payload, long &workMin, long &restMin, bool &displaySeconds)
{
    workMin = 25;
    restMin = 5;
    displaySeconds = false;

    if (!payload || payload->kind != PAYLOAD_POMODORO)
    {
        return;
    }

    if (payload->value.pomodoro.workMinutes > 0)
    {
        workMin = payload->value.pomodoro.workMinutes;
    }
    if (payload->value.pomodoro.restMinutes > 0)
    {
        restMin = payload->value.pomodoro.restMinutes;
    }
    displaySeconds = payload->value.pomodoro.displaySeconds;
}

static long getCounterTarget(const Preset &p)
{
    return getCounterTargetFromPayload(findPayloadForMode(p, MODE_COUNTER));
}

static long getTimerSeconds(const Preset &p)
{
    return getTimerSecondsFromPayload(findPayloadForMode(p, MODE_TIMER));
}

static void getPomodoroMinutes(const Preset &p, long &workMin, long &restMin)
{
    bool ignoredDisplaySeconds = false;
    getPomodoroConfigFromPayload(findPayloadForMode(p, MODE_POMODORO), workMin, restMin, ignoredDisplaySeconds);
}

static bool getPomodoroDisplaySeconds(const Preset &p)
{
    long ignoredWorkMin = 25;
    long ignoredRestMin = 5;
    bool displaySeconds = false;
    getPomodoroConfigFromPayload(findPayloadForMode(p, MODE_POMODORO), ignoredWorkMin, ignoredRestMin, displaySeconds);
    return displaySeconds;
}

InteractiveManager::InteractiveManager() {}

void InteractiveManager::begin()
{
    // Init values
    _counterValue = 0;
}

unsigned long InteractiveManager::getElapsed(unsigned long start, unsigned long accumulated, bool running)
{
    if (running)
    {
        return (millis() - start) + accumulated;
    }
    return accumulated;
}

void InteractiveManager::update()
{
    if (appConfig.presets.empty())
        return;
    const Preset &p = appConfig.presets[appConfig.currentPresetIndex];

    // Pomodoro logic
    long workMin = 25;
    long restMin = 5;
    getPomodoroMinutes(p, workMin, restMin);
    unsigned long workDur = workMin * 60UL * 1000UL;
    unsigned long restDur = restMin * 60UL * 1000UL;

    if (_pomoState == POMO_WORK && _pomoRunning)
    {
        unsigned long elapsed = getElapsed(_pomoStartTime, _pomoAccumulated, true);
        if (elapsed >= workDur)
        {
            _pomoRunning = false;
            _pomoState = POMO_WAIT_REST;
            _pomoAccumulated = 0; // Reset for next phase
            webLog("[POMO] Work finished. Waiting for Rest.");
        }
    }
    else if (_pomoState == POMO_REST && _pomoRunning)
    {
        unsigned long elapsed = getElapsed(_pomoStartTime, _pomoAccumulated, true);
        if (elapsed >= restDur)
        {
            _pomoRunning = false;
            _pomoState = POMO_WAIT_WORK;
            _pomoAccumulated = 0;
            webLog("[POMO] Rest finished. Waiting for Work.");
        }
    }
}

void InteractiveManager::handleButton1(int mode)
{
    if (mode == MODE_COUNTER)
    {
        // Decrease
        _counterValue--;
        if (_counterValue < 0)
            _counterValue = 0;
        webLogf("[Counter] Value: %ld", _counterValue);
    }
    else if (mode == MODE_TIMER)
    {
        // Reset
        _timerRunning = false;
        _accumulatedTime = 0;
        _timerFinished = false;
        webLog("[Timer] Reset");
    }
    else if (mode == MODE_POMODORO)
    {
        // Reset Logic: Reset current session
        _pomoRunning = false;
        _pomoAccumulated = 0;
        _pomoStartTime = millis();
        webLog("[Pomodoro] Reset current session");
    }
}

void InteractiveManager::handleButton2(int mode)
{
    if (mode == MODE_COUNTER)
    {
        // Increase
        _counterValue++;
        webLogf("[Counter] Value: %ld", _counterValue);
    }
    else if (mode == MODE_TIMER)
    {
        // Start / Pause
        if (_timerRunning)
        {
            // Pause
            _timerRunning = false;
            _accumulatedTime += (millis() - _timerStartTime);
            webLog("[Timer] Paused");
        }
        else
        {
            // Resume/Start
            _timerRunning = true;
            _timerStartTime = millis();
            webLog("[Timer] Started");
        }
    }
    else if (mode == MODE_POMODORO)
    {
        if (_pomoState == POMO_WAIT_REST)
        {
            _pomoState = POMO_REST;
            _pomoRunning = true;
            _pomoStartTime = millis();
            _pomoAccumulated = 0;
            webLog("[Pomodoro] Starting Rest");
        }
        else if (_pomoState == POMO_WAIT_WORK)
        {
            _pomoState = POMO_WORK;
            _pomoRunning = true;
            _pomoStartTime = millis();
            _pomoAccumulated = 0;
            webLog("[Pomodoro] Starting Work");
        }
        else
        {
            // Work or Rest
            if (_pomoRunning)
            {
                _pomoRunning = false;
                _pomoAccumulated += (millis() - _pomoStartTime);
                webLog("[Pomodoro] Paused");
            }
            else
            {
                _pomoRunning = true;
                _pomoStartTime = millis();
                webLog("[Pomodoro] Resumed");
            }
        }
    }
}

void InteractiveManager::resetCounter()
{
    _counterValue = 0;
    webLog("[Counter] Reset");
}

float InteractiveManager::getProgress(const RingConfig &ring)
{
    if (ring.mode == MODE_COUNTER)
    {
        long target = getCounterTargetFromPayload(&ring.payload);
        if (target <= 0)
            return 0.0f;
        return (float)_counterValue / (float)target;
    }
    else if (ring.mode == MODE_TIMER)
    {
        long targetBox = getTimerSecondsFromPayload(&ring.payload) * 1000L;
        unsigned long elapsed = getElapsed(_timerStartTime, _accumulatedTime, _timerRunning);
        if (elapsed >= (unsigned long)targetBox)
            return 1.0f;
        return (float)elapsed / (float)targetBox;
    }
    else if (ring.mode == MODE_POMODORO)
    {
        long workMin = 25;
        long restMin = 5;
        bool ignoredDisplaySeconds = false;
        getPomodoroConfigFromPayload(&ring.payload, workMin, restMin, ignoredDisplaySeconds);
        unsigned long workDur = workMin * 60UL * 1000UL;
        unsigned long restDur = restMin * 60UL * 1000UL;

        unsigned long duration = (_pomoState == POMO_WORK) ? workDur : restDur;
        if (_pomoState == POMO_WAIT_REST || _pomoState == POMO_WAIT_WORK)
            return 1.0f;

        unsigned long elapsed = getElapsed(_pomoStartTime, _pomoAccumulated, _pomoRunning);
        if (duration == 0)
            return 0.0f;
        return (float)elapsed / (float)duration;
    }
    return 0.0f;
}

int InteractiveManager::getDisplayNumber(int mode)
{
    if (appConfig.presets.empty())
        return 0;
    const Preset &p = appConfig.presets[appConfig.currentPresetIndex];

    if (mode == MODE_COUNTER)
    {
        return (int)_counterValue;
    }
    else if (mode == MODE_TIMER)
    {
        long target = getTimerSeconds(p);
        bool displaySeconds = getTimerDisplaySecondsFromPayload(findPayloadForMode(p, MODE_TIMER));
        unsigned long elapsed = getElapsed(_timerStartTime, _accumulatedTime, _timerRunning);
        long remainingSeconds = target - (elapsed / 1000);
        if (remainingSeconds < 0)
            remainingSeconds = 0;
        if (displaySeconds)
        {
            return (int)remainingSeconds;
        }
        return (remainingSeconds + 59) / 60; // Minutes ceil
    }
    else if (mode == MODE_POMODORO)
    {
        long workMin = 25;
        long restMin = 5;
        getPomodoroMinutes(p, workMin, restMin);
        bool displaySeconds = getPomodoroDisplaySeconds(p);
        unsigned long workDur = workMin * 60UL * 1000UL;
        unsigned long restDur = restMin * 60UL * 1000UL;

        unsigned long duration = (_pomoState == POMO_WORK) ? workDur : restDur;
        if (_pomoState == POMO_WAIT_REST || _pomoState == POMO_WAIT_WORK)
            return 0;

        unsigned long elapsed = getElapsed(_pomoStartTime, _pomoAccumulated, _pomoRunning);
        long remainingMS = duration - elapsed;
        if (remainingMS < 0)
            remainingMS = 0;
        if (displaySeconds)
        {
            return (int)((remainingMS + 999) / 1000); // Seconds ceil
        }
        return (int)((remainingMS + 59999) / 60000); // Minutes ceil
    }
    return 0;
}

bool InteractiveManager::shouldBlink(int mode)
{
    if (mode == MODE_POMODORO)
    {
        return (_pomoState == POMO_WAIT_REST || _pomoState == POMO_WAIT_WORK);
    }
    return false;
}
