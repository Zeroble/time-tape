#include "Config.h"
#include "ConfigCodec.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <vector>

AppConfig appConfig;
Preferences preferences;

namespace
{
const char *kPrefNs = "time-tape";
const char *kPrefKeyConfigBin = "config_bin";
const char *kPrefKeyConfigJson = "config";
}

void initDefaultConfig()
{
	appConfig.presets.clear();
	appConfig.ddays.clear();

	Preset p1;
	p1.inner.mode = 0;
	p1.inner.colorMode = 0;
	p1.inner.colorFill = 0xFF0000;
	p1.inner.colorFill2 = 0x00FF00;
	p1.inner.colorEmpty = 0;
	payloadSetNone(p1.inner.payload);

	p1.outer.mode = 0;
	p1.outer.colorMode = 0;
	p1.outer.colorFill = 0x0000FF;
	p1.outer.colorFill2 = 0xFFFF00;
	p1.outer.colorEmpty = 0;
	payloadSetNone(p1.outer.payload);

	p1.segment.mode = 1;
	payloadSetNone(p1.segment.payload);

	appConfig.presets.push_back(p1);
	appConfig.ddays.push_back({"새해", "2025-01-01", "2026-01-01"});
}

void saveConfigToFile()
{
    preferences.begin(kPrefNs, false);
    
    JsonDocument doc;
    configToJson(doc, appConfig);

    const size_t msgpackSize = measureMsgPack(doc);
    std::vector<uint8_t> msgpack(msgpackSize);
    serializeMsgPack(doc, msgpack.data(), msgpack.size());
    size_t written = preferences.putBytes(kPrefKeyConfigBin, msgpack.data(), msgpack.size());

    // 백업/하위호환용 JSON도 함께 저장
    String jsonStr;
    serializeJson(doc, jsonStr);
    size_t jsonWritten = preferences.putString(kPrefKeyConfigJson, jsonStr);

    if (written != msgpack.size() || jsonWritten == 0)
    {
        Serial.println("[Config] Warning: config save may be incomplete");
    }

    preferences.end();
}

void loadConfig()
{
    preferences.begin(kPrefNs, true);
    size_t msgpackSize = preferences.getBytesLength(kPrefKeyConfigBin);
    String jsonStr = preferences.getString(kPrefKeyConfigJson, "");
    preferences.end();

    if (msgpackSize > 0)
    {
        std::vector<uint8_t> msgpack(msgpackSize);
        preferences.begin(kPrefNs, true);
        size_t read = preferences.getBytes(kPrefKeyConfigBin, msgpack.data(), msgpack.size());
        preferences.end();

        if (read == msgpack.size())
        {
            JsonDocument doc;
            if (!deserializeMsgPack(doc, msgpack.data(), msgpack.size()))
            {
                AppConfig parsed;
                if (configFromJson(doc, parsed))
                {
                    appConfig = parsed;
                    return;
                }
            }
        }
    }

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

    AppConfig parsed;
    if (!configFromJson(doc, parsed))
    {
        initDefaultConfig();
        return;
    }

    appConfig = parsed;
}
