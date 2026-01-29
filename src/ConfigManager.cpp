#include "Config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

AppConfig appConfig;

void initDefaultConfig()
{
	appConfig.presets.clear();
	appConfig.ddays.clear();

	// 기본 프리셋 생성 (무지개/그라데이션 예시 포함)
	Preset p1;
	p1.name = "기본 모드";
	p1.innerMode = 0;
	p1.innerDDayIndex = 0;
	p1.innerColorMode = 0;
	p1.innerColorFill = 0xFF0000;
	p1.innerColorFill2 = 0x00FF00;
	p1.innerColorEmpty = 0;
	p1.outerMode = 0;
	p1.outerDDayIndex = 0;
	p1.outerColorMode = 0;
	p1.outerColorFill = 0x0000FF;
	p1.outerColorFill2 = 0xFFFF00;
	p1.outerColorEmpty = 0;
	p1.segMode = 1;
	p1.segDDayIndex = 0;

	appConfig.presets.push_back(p1);

	DDay d1 = {"새해", "2025-01-01", "2026-01-01"};
	appConfig.ddays.push_back(d1);
}

void loadConfig()
{
	if (!LittleFS.begin())
	{
		Serial.println("LittleFS Error");
		return;
	}

	File file = LittleFS.open("/config.json", "r");
	if (!file)
	{
		Serial.println("No config file, using default.");
		initDefaultConfig();
		return;
	}

	// JSON 파싱 (용량을 넉넉하게 4KB)
	DynamicJsonDocument doc(4096);
	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error)
	{
		Serial.println("JSON Parse Error, using default.");
		initDefaultConfig();
		return;
	}

	// 1. 기본 설정 파싱
	appConfig.currentPresetIndex = doc["curIdx"] | 0;
	appConfig.brightness = doc["bri"] | 50;
	appConfig.nightModeEnabled = doc["nEn"] | false;
	appConfig.nightStartHour = doc["nS"] | 22;
	appConfig.nightEndHour = doc["nE"] | 7;
	appConfig.nightBrightness = doc["nB"] | 10;

	// 2. 프리셋 파싱 (여기에 새 색상 모드 로직이 들어감)
	appConfig.presets.clear();
	JsonArray presets = doc["presets"];
	for (JsonObject pObj : presets)
	{
		Preset p;
		p.name = pObj["n"].as<String>();

		// Inner
		p.innerMode = pObj["im"];
		p.innerDDayIndex = pObj["id"];
		p.innerColorMode = pObj["icm"] | 0; // <--- [중요] 모드 읽기
		p.innerColorFill = pObj["icf"];
		p.innerColorFill2 = pObj["icf2"] | 0x00FF00; // <--- [중요] 2번 색상 읽기
		p.innerColorEmpty = pObj["ice"];

		// Outer
		p.outerMode = pObj["om"];
		p.outerDDayIndex = pObj["od"];
		p.outerColorMode = pObj["ocm"] | 0; // <--- [중요]
		p.outerColorFill = pObj["ocf"];
		p.outerColorFill2 = pObj["ocf2"] | 0xFFFF00; // <--- [중요]
		p.outerColorEmpty = pObj["oce"];

		// Seg
		p.segMode = pObj["sm"];
		p.segDDayIndex = pObj["sd"];

		appConfig.presets.push_back(p);
	}

	// 3. 디데이 파싱
	appConfig.ddays.clear();
	JsonArray ddays = doc["ddays"];
	for (JsonObject dObj : ddays)
	{
		DDay d;
		d.name = dObj["n"].as<String>();
		d.startDate = dObj["s"].as<String>();
		d.targetDate = dObj["t"].as<String>();
		appConfig.ddays.push_back(d);
	}

	Serial.println("Config Loaded Successfully");
}
