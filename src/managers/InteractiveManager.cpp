#include "managers/InteractiveManager.h"

InteractiveManager interactiveManager;

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
    // Work = specialValue (min), Rest = specialValue2 (min)
    unsigned long workDur = (p.specialValue > 0 ? p.specialValue : 25) * 60 * 1000;
    unsigned long restDur = (p.specialValue2 > 0 ? p.specialValue2 : 5) * 60 * 1000;

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

float InteractiveManager::getProgress(int mode, int ringSize)
{
    if (appConfig.presets.empty())
        return 0.0f;
    const Preset &p = appConfig.presets[appConfig.currentPresetIndex];

    if (mode == MODE_COUNTER)
    {
        long target = p.specialValue > 0 ? p.specialValue : 100;
        return (float)_counterValue / (float)target;
    }
    else if (mode == MODE_TIMER)
    {
        long targetBox = (p.specialValue > 0 ? p.specialValue : 60) * 1000;
        unsigned long elapsed = getElapsed(_timerStartTime, _accumulatedTime, _timerRunning);
        if (elapsed >= (unsigned long)targetBox)
            return 1.0f;
        return (float)elapsed / (float)targetBox;
    }
    else if (mode == MODE_POMODORO)
    {
        unsigned long workDur = (p.specialValue > 0 ? p.specialValue : 25) * 60 * 1000;
        unsigned long restDur = (p.specialValue2 > 0 ? p.specialValue2 : 5) * 60 * 1000;

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
        long target = p.specialValue > 0 ? p.specialValue : 60;
        unsigned long elapsed = getElapsed(_timerStartTime, _accumulatedTime, _timerRunning);
        long remainingSeconds = target - (elapsed / 1000);
        if (remainingSeconds < 0)
            remainingSeconds = 0;
        return (remainingSeconds + 59) / 60; // Minutes ceil
    }
    else if (mode == MODE_POMODORO)
    {
        unsigned long workDur = (p.specialValue > 0 ? p.specialValue : 25) * 60 * 1000;
        unsigned long restDur = (p.specialValue2 > 0 ? p.specialValue2 : 5) * 60 * 1000;

        unsigned long duration = (_pomoState == POMO_WORK) ? workDur : restDur;
        if (_pomoState == POMO_WAIT_REST || _pomoState == POMO_WAIT_WORK)
            return 0;

        unsigned long elapsed = getElapsed(_pomoStartTime, _pomoAccumulated, _pomoRunning);
        long remainingMS = duration - elapsed;
        if (remainingMS < 0)
            remainingMS = 0;
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
