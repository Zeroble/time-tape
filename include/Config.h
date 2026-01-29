#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>

// 디데이 구조체
struct DDay
{
	String name;
	String startDate;
	String targetDate;
};

// 프리셋 구조체 (색상 모드 추가됨)
struct Preset
{
	String name;

	// Inner Ring
	int innerMode;
	int innerDDayIndex;
	int innerColorMode;		  // 0:단색, 1:무지개, 2:시간그라, 3:공간그라
	uint32_t innerColorFill;  // 시작 색 (또는 단색)
	uint32_t innerColorFill2; // 끝 색 (그라데이션용)
	uint32_t innerColorEmpty; // 빈 곳 색

	// Outer Ring
	int outerMode;
	int outerDDayIndex;
	int outerColorMode; // [신규]
	uint32_t outerColorFill;
	uint32_t outerColorFill2; // [신규]
	uint32_t outerColorEmpty;

	// 7-Segment
	int segMode;
	int segDDayIndex;
};

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
