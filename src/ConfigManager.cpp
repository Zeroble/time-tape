#include "Config.h"
#include <ArduinoJson.h>
#include <Preferences.h>

AppConfig appConfig;
Preferences preferences;

void initDefaultConfig()
{
	appConfig.presets.clear();
	appConfig.ddays.clear();

	Preset p1;
	p1.innerMode = 0;
	p1.id_dd = 0;
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
    p1.specialValue = 0;
    p1.specialValue2 = 0;

	appConfig.presets.push_back(p1);
	appConfig.ddays.push_back({"새해", "2025-01-01", "2026-01-01"});
}

void saveConfigToFile()
{
    preferences.begin("time-tape", false);
    
    JsonDocument doc;
    doc["curIdx"] = appConfig.currentPresetIndex;
    doc["bri"] = appConfig.brightness;
    doc["nEn"] = appConfig.nightModeEnabled;
    doc["nS"] = appConfig.nightStartHour;
    doc["nE"] = appConfig.nightEndHour;
    doc["nB"] = appConfig.nightBrightness;

    JsonArray arrP = doc["presets"].to<JsonArray>();
    for (const auto &p : appConfig.presets)
    {
        JsonObject obj = arrP.add<JsonObject>();
        obj["im"] = p.innerMode;
        obj["id_dd"] = p.id_dd;
        obj["icm"] = p.innerColorMode;
        obj["icf"] = p.innerColorFill;
        obj["icf2"] = p.innerColorFill2;
        obj["ice"] = p.innerColorEmpty;
        obj["om"] = p.outerMode;
        obj["od"] = p.outerDDayIndex;
        obj["ocm"] = p.outerColorMode;
        obj["ocf"] = p.outerColorFill;
        obj["ocf2"] = p.outerColorFill2;
        obj["oce"] = p.outerColorEmpty;
        obj["sm"] = p.segMode;
        obj["sd"] = p.segDDayIndex;
        obj["sv"] = p.specialValue;
        obj["sv2"] = p.specialValue2;
    }

    JsonArray arrD = doc["ddays"].to<JsonArray>();
    for (const auto &d : appConfig.ddays)
    {
        JsonObject obj = arrD.add<JsonObject>();
        obj["n"] = d.name;
        obj["s"] = d.startDate;
        obj["t"] = d.targetDate;
    }

    String jsonStr;
    serializeJson(doc, jsonStr);
    preferences.putString("config", jsonStr);
    preferences.end();
}

void loadConfig()
{
    preferences.begin("time-tape", true);
    String jsonStr = preferences.getString("config", "");
    preferences.end();

    if (jsonStr == "")
    {
        initDefaultConfig();
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error)
    {
        initDefaultConfig();
        return;
    }

    appConfig.currentPresetIndex = doc["curIdx"] | 0;
    appConfig.brightness = doc["bri"] | 50;
    appConfig.nightModeEnabled = doc["nEn"] | false;
    appConfig.nightStartHour = doc["nS"] | 22;
    appConfig.nightEndHour = doc["nE"] | 7;
    appConfig.nightBrightness = doc["nB"] | 10;

    appConfig.presets.clear();
    JsonArray presets = doc["presets"];
    for (JsonObject pObj : presets)
    {
        Preset p;
        p.innerMode = pObj["im"] | 0;
        p.id_dd = pObj["id_dd"] | 0;
        p.innerColorMode = pObj["icm"] | 0;
        p.innerColorFill = pObj["icf"] | 0xFF0000;
        p.innerColorFill2 = pObj["icf2"] | 0x00FF00;
        p.innerColorEmpty = pObj["ice"] | 0;
        p.outerMode = pObj["om"] | 0;
        p.outerDDayIndex = pObj["od"] | 0;
        p.outerColorMode = pObj["ocm"] | 0;
        p.outerColorFill = pObj["ocf"] | 0x0000FF;
        p.outerColorFill2 = pObj["ocf2"] | 0xFFFF00;
        p.outerColorEmpty = pObj["oce"] | 0;
        p.segMode = pObj["sm"] | 1;
        p.segDDayIndex = pObj["sd"] | 0;
        p.specialValue = pObj["sv"] | 0;
        p.specialValue2 = pObj["sv2"] | 0;
        appConfig.presets.push_back(p);
    }

    appConfig.ddays.clear();
    JsonArray ddays = doc["ddays"];
    for (JsonObject dObj : ddays)
    {
        DDay d;
        d.name = dObj["n"] | "";
        d.startDate = dObj["s"] | "";
        d.targetDate = dObj["t"] | "";
        appConfig.ddays.push_back(d);
    }
}