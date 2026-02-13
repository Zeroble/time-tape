#pragma once
#include <Arduino.h>
#include "Config.h"
#include "WebLogger.h"

struct ButtonState {
    uint8_t pin;
    bool lastReading;
    unsigned long lastDebounceTime;
    bool state;
    bool pressedEvent;
};

class ButtonManager {
    ButtonState buttons[4];
    const unsigned long debounceDelay = 50;

public:
    ButtonManager() {
        buttons[0] = {BTN_1, HIGH, 0, HIGH, false};
        buttons[1] = {BTN_2, HIGH, 0, HIGH, false};
        buttons[2] = {BTN_3, HIGH, 0, HIGH, false};
        buttons[3] = {BTN_4, HIGH, 0, HIGH, false};
    }

    void begin() {
        for (int i = 0; i < 4; i++) {
            pinMode(buttons[i].pin, INPUT_PULLUP);
        }
    }

    void update() {
        for (int i = 0; i < 4; i++) {
            bool reading = digitalRead(buttons[i].pin);
            if (reading != buttons[i].lastReading) {
                buttons[i].lastDebounceTime = millis();
            }

            if ((millis() - buttons[i].lastDebounceTime) > debounceDelay) {
                if (reading != buttons[i].state) {
                    buttons[i].state = reading;
                    if (buttons[i].state == LOW) {
                        buttons[i].pressedEvent = true;
                    }
                }
            }
            buttons[i].lastReading = reading;
        }
    }

    bool wasPressed(int pin) {
        for (int i = 0; i < 4; i++) {
            if (buttons[i].pin == pin) {
                if (buttons[i].pressedEvent) {
                    buttons[i].pressedEvent = false;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    // For debugging
    void checkButtons() {
        update();
        if (wasPressed(BTN_1)) webLog("Button 1 Pressed!");
        if (wasPressed(BTN_2)) webLog("Button 2 Pressed!");
        if (wasPressed(BTN_3)) webLog("Button 3 Pressed!");
        if (wasPressed(BTN_4)) webLog("Button 4 Pressed!");
    }
};
