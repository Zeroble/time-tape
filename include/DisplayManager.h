#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// --- 핀 설정 ---
#define PIN_NEOPIXEL 4
#define SCLK_PIN 8
#define LOAD_PIN 9
#define SDI_PIN 7

// --- LED 개수 (물리적 연결: 8 -> 16 -> 24) ---
#define NUM_LEDS_DUMMY 0  // 0~7번 (항상 꺼짐, 전압 레벨용)
#define NUM_LEDS_INNER 16 // 8~23번
#define NUM_LEDS_OUTER 24 // 24~47번
#define NUM_LEDS_TOTAL (NUM_LEDS_DUMMY + NUM_LEDS_INNER + NUM_LEDS_OUTER)

void setupDisplay();
void updateDisplay(AppConfig config);
