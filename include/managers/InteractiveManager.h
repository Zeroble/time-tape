#pragma once
#include <Arduino.h>
#include "Config.h"
#include "WebLogger.h"

class InteractiveManager
{
public:
    InteractiveManager();
    void begin();
    void update(); // Called in loop

    // Button Inputs
    void handleButton1(int mode); // Reset / Decrease
    void handleButton2(int mode); // Start / Increase / Pause
    void resetCounter();

    // Data Access for Display
    float getProgress(const RingConfig &ring);
    int getDisplayNumber(int mode);
    bool shouldBlink(int mode); // For Pomodoro waiting state

    bool isTimerRunning() { return _timerRunning; }
    bool isPomoRunning() { return _pomoRunning; }

private:
    // Counter State
    long _counterValue = 0;

    // Timer State
    unsigned long _timerStartTime = 0;
    unsigned long _timerPauseTime = 0;
    unsigned long _accumulatedTime = 0; // Paused duration
    bool _timerRunning = false;
    bool _timerFinished = false;

    // Pomodoro State
    enum PomoState
    {
        POMO_WORK,
        POMO_WAIT_REST,
        POMO_REST,
        POMO_WAIT_WORK
    };
    PomoState _pomoState = POMO_WORK;
    unsigned long _pomoStartTime = 0;
    unsigned long _pomoPauseTime = 0;
    unsigned long _pomoAccumulated = 0;
    bool _pomoRunning = false;

    // Helper
    unsigned long getElapsed(unsigned long start, unsigned long accumulated, bool running);
};

extern InteractiveManager interactiveManager;
