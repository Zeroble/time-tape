#include "NetworkManager.h"
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include "Config.h" // ConfigManager에서 만든 loadConfig()를 쓰기 위해 필요

AsyncWebServer server(80);

void setupNetwork()
{
	// LittleFS 마운트 확인
	if (!LittleFS.begin())
	{
		Serial.println("LittleFS Mount Failed, format...");
		LittleFS.format();
		LittleFS.begin();
	}

	// 부팅 시 설정 파일 읽어서 메모리(appConfig)에 적재
	loadConfig();

	WiFiManager wm;
	wm.setTimeout(180);

	if (!wm.autoConnect("TimeCrisis_Setup"))
	{
		Serial.println("WiFi Fail, Restarting...");
		ESP.restart();
	}

	if (MDNS.begin("timer"))
		Serial.println("mDNS started: http://timer.local");

	// 1. 설정 JSON 요청 (파일 읽어서 그대로 전송)
	server.on("/get-config", HTTP_GET, [](AsyncWebServerRequest *r)
			  {
        if (LittleFS.exists("/config.json")) {
            r->send(LittleFS, "/config.json", "application/json");
        } else {
            // 파일이 없으면 현재 메모리 값을 JSON으로 만들어 보내거나 빈값 전송
            // 여기서는 간단하게 빈 객체 전송 -> 클라이언트가 initDefault 처리
            r->send(200, "application/json", "{}"); 
        } });

	// 2. 설정 저장
	// AsyncWebServer의 바디 처리 핸들러를 사용하여 스트림으로 바로 저장
	server.on("/set-config", HTTP_POST,
			  // 요청 처리 (응답 전송)
			  [](AsyncWebServerRequest *r)
			  { r->send(200, "application/json", "{\"status\":\"ok\"}"); }, NULL,
			  // 바디 데이터 처리 (파일 저장)
			  [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
			  {
            // 파일을 overwrite 모드로 엽니다. (index 0일 때 새로 엶)
            static File file;
            if (index == 0) {
                file = LittleFS.open("/config.json", "w");
            }
            
            if (file) {
                file.write(data, len);
            }

            // 전송 완료 시 파일 닫고 설정 다시 로드
            if (index + len == total) {
                if (file) file.close();
                Serial.println("Config File Saved. Reloading...");
                loadConfig(); // <--- 저장된 파일 내용을 즉시 반영!
            } });

	server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

	server.begin();
	Serial.println("HTTP Server Started");
}
