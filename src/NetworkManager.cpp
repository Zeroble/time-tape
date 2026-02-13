#include "NetworkManager.h"
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "WebLogger.h"

AsyncWebServer server(80);
AsyncWebSocket wsLog("/ws/log");

void webLog(const String &msg)
{
    Serial.println(msg);
    wsLog.textAll(msg);
}

void webLogf(const char *format, ...)
{
    char loc_buf[128];
    va_list arg;
    va_start(arg, format);
    vsnprintf(loc_buf, sizeof(loc_buf), format, arg);
    va_end(arg);
    webLog(String(loc_buf));
}

void setupNetwork()
{
    if (!LittleFS.begin())
    {
        LittleFS.format();
        LittleFS.begin();
    }

    loadConfig();

    WiFiManager wm;
    wm.setTimeout(180);
    if (!wm.autoConnect("timetape_setup"))
        ESP.restart();

    if (MDNS.begin("tape"))
        webLog("mDNS started: http://timer.local");

    server.addHandler(&wsLog);

    server.on("/get-config", HTTP_GET, [](AsyncWebServerRequest *r)
              {
        JsonDocument doc;
        doc["curIdx"] = appConfig.currentPresetIndex;
        doc["bri"] = appConfig.brightness;
        doc["nEn"] = appConfig.nightModeEnabled;
        doc["nS"] = appConfig.nightStartHour;
        doc["nE"] = appConfig.nightEndHour;
        doc["nB"] = appConfig.nightBrightness;

        JsonArray arrP = doc["presets"].to<JsonArray>();
        for (const auto &p : appConfig.presets) {
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
        for (const auto &d : appConfig.ddays) {
            JsonObject obj = arrD.add<JsonObject>();
            obj["n"] = d.name;
            obj["s"] = d.startDate;
            obj["t"] = d.targetDate;
        }

        String response;
        serializeJson(doc, response);
        r->send(200, "application/json", response); });

    server.on("/set-config", HTTP_POST, [](AsyncWebServerRequest *r)
              { r->send(200, "application/json", "{\"status\":\"ok\"}"); }, NULL, [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
              {
            static String body;
            if (index == 0) body = "";
            for (size_t i = 0; i < len; i++) body += (char)data[i];

            if (index + len == total) {
                JsonDocument doc;
                if (deserializeJson(doc, body)) return; 
                
                appConfig.currentPresetIndex = doc["curIdx"] | 0;
                appConfig.brightness = doc["bri"] | 50;
                appConfig.nightModeEnabled = doc["nEn"] | false;
                appConfig.nightStartHour = doc["nS"] | 22;
                appConfig.nightEndHour = doc["nE"] | 7;
                appConfig.nightBrightness = doc["nB"] | 10;

                appConfig.presets.clear();
                JsonArray presets = doc["presets"];
                for (JsonObject pObj : presets) {
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
                for (JsonObject dObj : ddays) {
                    DDay d;
                    d.name = dObj["n"] | "";
                    d.startDate = dObj["s"] | "";
                    d.targetDate = dObj["t"] | "";
                    appConfig.ddays.push_back(d);
                }
                saveConfigToFile();
            } });

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    server.begin();
}
