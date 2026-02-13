#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>

// --- 하드웨어 핀 및 LED 설정 ---
#define PIN_INNER 4
#define PIN_OUTER 3
#define SDI_PIN 7
#define SCLK_PIN 8
#define LOAD_PIN 9

// --- 버튼 핀 설정 ---
#define BTN_1 5
#define BTN_2 6
#define BTN_3 10
#define BTN_4 20

#define NUM_LEDS_INNER 16
#define NUM_LEDS_OUTER 24

// --- 모드 상수 정의 ---
#define MODE_NONE 0
// 기존 1~6은 유지 (코드 내 매직 넘버 사용 중)
#define MODE_COUNTER 10
#define MODE_TIMER 11
#define MODE_POMODORO 12

// --- 모드 payload kind ---
enum PayloadKind : uint8_t
{
	PAYLOAD_NONE = 0,
	PAYLOAD_DDAY = 1,
	PAYLOAD_COUNTER = 2,
	PAYLOAD_TIMER = 3,
	PAYLOAD_POMODORO = 4
};

// 디데이 구조체
struct DDay
{
	String name;
	String startDate;
	String targetDate;
};

struct TimerPayload
{
	long totalSeconds;
	bool displaySeconds;
};

struct PomodoroPayload
{
	long workMinutes;
	long restMinutes;
	bool displaySeconds;
};

union ModePayloadValue
{
	int ddayIndex;
	long counterTarget;
	TimerPayload timer;
	PomodoroPayload pomodoro;
	ModePayloadValue() : ddayIndex(0) {}
};

struct ModePayload
{
	PayloadKind kind = PAYLOAD_NONE;
	ModePayloadValue value;
};

struct RingConfig
{
	int mode = 0;
	int colorMode = 0; // 0:단색, 1:무지개, 2:시간그라, 3:공간그라
	uint32_t colorFill = 0;
	uint32_t colorFill2 = 0;
	uint32_t colorEmpty = 0;
	ModePayload payload;
};

struct SegmentConfig
{
	int mode = 1;
	ModePayload payload;
};

struct Preset
{
	RingConfig inner;
	RingConfig outer;
	SegmentConfig segment;
};

inline void payloadSetNone(ModePayload &payload)
{
	payload.kind = PAYLOAD_NONE;
	payload.value.ddayIndex = 0;
}

inline void payloadSetDDay(ModePayload &payload, int ddayIndex)
{
	payload.kind = PAYLOAD_DDAY;
	payload.value.ddayIndex = ddayIndex;
}

inline void payloadSetCounter(ModePayload &payload, long counterTarget)
{
	payload.kind = PAYLOAD_COUNTER;
	payload.value.counterTarget = counterTarget;
}

inline void payloadSetTimer(ModePayload &payload, long totalSeconds, bool displaySeconds = false)
{
	payload.kind = PAYLOAD_TIMER;
	payload.value.timer.totalSeconds = totalSeconds;
	payload.value.timer.displaySeconds = displaySeconds;
}

inline void payloadSetPomodoro(ModePayload &payload, long workMinutes, long restMinutes, bool displaySeconds = false)
{
	payload.kind = PAYLOAD_POMODORO;
	payload.value.pomodoro.workMinutes = workMinutes;
	payload.value.pomodoro.restMinutes = restMinutes;
	payload.value.pomodoro.displaySeconds = displaySeconds;
}

inline bool isInteractiveMode(int mode)
{
	return mode >= MODE_COUNTER;
}

// 전체 설정 구조체
struct AppConfig
{
	int currentPresetIndex = 0;
	std::vector<Preset> presets;
	std::vector<DDay> ddays;

	int brightness = 50;
	bool nightModeEnabled = false;
	int nightStartHour = 22;
	int nightEndHour = 7;
	int nightBrightness = 10;
};

// 전역 변수 및 함수 선언
extern AppConfig appConfig;
void loadConfig();		  // 설정 불러오기
void saveConfigToFile();  // (필요시) 현재 설정을 파일로 저장
void initDefaultConfig(); // 기본값 초기화

#endif
