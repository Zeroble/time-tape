#include "Config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

AppConfig globalConfig;

void loadConfig()
{
	if (!LittleFS.begin())
		LittleFS.begin(true);

	File file = LittleFS.open("/config.json", "r");
	if (!file)
	{
		// 기본값 생성
		Preset p1;
		p1.name = "Default";
		globalConfig.presets.push_back(p1);
		globalConfig.ddays.push_back({"X-mas", "2024-01-01", "2024-12-25"});
		return;
	}

	JsonDocument doc;
	DeserializationError error = deserializeJson(doc, file);
	file.close();

	if (error)
		return;

	globalConfig.currentPresetIndex = doc["curIdx"] | 0;
	globalConfig.brightness = doc["bri"] | 50;
	globalConfig.nightModeEnabled = doc["nEn"] | false;
	globalConfig.nightStartHour = doc["nS"] | 22;
	globalConfig.nightEndHour = doc["nE"] | 7;
	globalConfig.nightBrightness = doc["nB"] | 10;

	globalConfig.presets.clear();
	JsonArray arrP = doc["presets"];
	for (JsonObject obj : arrP)
	{
		Preset p;
		p.name = obj["n"].as<String>();
		p.innerMode = obj["im"];
		p.innerDDayIndex = obj["id"];
		p.innerColorFill = obj["icf"];
		p.innerColorEmpty = obj["ice"];
		p.outerMode = obj["om"];
		p.outerDDayIndex = obj["od"];
		p.outerColorFill = obj["ocf"];
		p.outerColorEmpty = obj["oce"];
		p.segMode = obj["sm"];
		p.segDDayIndex = obj["sd"];
		globalConfig.presets.push_back(p);
	}

	globalConfig.ddays.clear();
	JsonArray arrD = doc["ddays"];
	for (JsonObject obj : arrD)
	{
		DDay d;
		d.name = obj["n"].as<String>();
		d.startDate = obj["s"].as<String>();
		d.targetDate = obj["t"].as<String>();
		globalConfig.ddays.push_back(d);
	}
}

void saveConfig()
{
	JsonDocument doc;
	doc["curIdx"] = globalConfig.currentPresetIndex;
	doc["bri"] = globalConfig.brightness;
	doc["nEn"] = globalConfig.nightModeEnabled;
	doc["nS"] = globalConfig.nightStartHour;
	doc["nE"] = globalConfig.nightEndHour;
	doc["nB"] = globalConfig.nightBrightness;

	JsonArray arrP = doc.createNestedArray("presets");
	for (const auto &p : globalConfig.presets)
	{
		JsonObject obj = arrP.createNestedObject();
		obj["n"] = p.name;
		obj["im"] = p.innerMode;
		obj["id"] = p.innerDDayIndex;
		obj["icf"] = p.innerColorFill;
		obj["ice"] = p.innerColorEmpty;
		obj["om"] = p.outerMode;
		obj["od"] = p.outerDDayIndex;
		obj["ocf"] = p.outerColorFill;
		obj["oce"] = p.outerColorEmpty;
		obj["sm"] = p.segMode;
		obj["sd"] = p.segDDayIndex;
	}

	JsonArray arrD = doc.createNestedArray("ddays");
	for (const auto &d : globalConfig.ddays)
	{
		JsonObject obj = arrD.createNestedObject();
		obj["n"] = d.name;
		obj["s"] = d.startDate;
		obj["t"] = d.targetDate;
	}

	File file = LittleFS.open("/config.json", "w");
	serializeJson(doc, file);
	file.close();
}
