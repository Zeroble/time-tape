#include <Arduino.h>
#include "Config.h"
#include "managers/DisplayManager.h"
#include "managers/ButtonManager.h"
#include "managers/InteractiveManager.h"
#include "NetworkManager.h"
#include "TimeLogic.h"
#include "WebLogger.h"

// OTA
#include <ArduinoOTA.h>
#include <WiFi.h>

DisplayManager display;
ButtonManager buttons;

static void setupOTA()
{
  // WiFi 연결이 되어 있어야 함
  if (WiFi.status() != WL_CONNECTED)
  {
    webLog("[OTA] WiFi not connected, OTA will not start");
    return;
  }

  ArduinoOTA
      .onStart([]()
               {
                 String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
                 webLogf("[OTA] Start updating %s", type.c_str());
                 // OTA 중에는 타이밍 민감할 수 있으니 필요하면 디스플레이 갱신/애니메이션을 멈추는 것도 방법
               })
      .onEnd([]()
             { webLog("\n[OTA] End"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { webLogf("[OTA] Progress: %u%%", (progress * 100U) / total); })
      .onError([](ota_error_t error)
               {
      webLogf("\n[OTA] Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) webLog("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) webLog("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) webLog("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) webLog("Receive Failed");
      else if (error == OTA_END_ERROR) webLog("End Failed"); });

  ArduinoOTA.begin();
  webLogf("[OTA] Ready. IP: %s", WiFi.localIP().toString().c_str());
}

void setup()
{
  Serial.begin(115200);
  webLog("hello");
  delay(1000);

  // 1. 설정 로드
  loadConfig();

  // 2. 하드웨어 초기화
  display.begin();
  buttons.begin();
  interactiveManager.begin();

  // 3. 네트워크 연결 (WiFi -> mDNS -> WebServer)
  setupNetwork();

  // 3.5 OTA 시작 (WiFi 연결 이후!)
  setupOTA();

  // 4. 시간 동기화
  setupTime();

  // 5. 애니메이션 종료 (네트워크/시간 준비 완료 후)
  delay(1000);
  display.stopBootAnimation();

  // 6. IP 표시
  if (WiFi.status() == WL_CONNECTED) {
    display.displayIP((uint32_t)WiFi.localIP());
  }
}

void loop()
{
  // OTA 패킷 처리(가장 중요)
  ArduinoOTA.handle();
  networkLoop();

  // 버튼 상태 업데이트
  buttons.update();
  
  // 인터랙티브 로직 업데이트
  interactiveManager.update();

  static unsigned long lastPresetChangeTime = 0;
  static unsigned long lastInteractionTime = 0;
  static int interactionMode = MODE_NONE;
  bool presetChanged = false;

  // 인터랙티브 모드 확인 (Inner 우선, Outer 차선)
  int activeInteractiveMode = MODE_NONE;
  if (!appConfig.presets.empty()) {
      const Preset& p = appConfig.presets[appConfig.currentPresetIndex];
      if (isInteractiveMode(p.inner.mode)) activeInteractiveMode = p.inner.mode;
      else if (isInteractiveMode(p.outer.mode)) activeInteractiveMode = p.outer.mode;
  }

  bool btn1Pressed = buttons.wasPressed(BTN_1);
  bool btn2Pressed = buttons.wasPressed(BTN_2);

  // 카운터 모드에서 버튼 1+2 동시 입력 시 카운터 초기화
  if (activeInteractiveMode == MODE_COUNTER && btn1Pressed && btn2Pressed) {
      interactiveManager.resetCounter();
      lastInteractionTime = millis();
      interactionMode = MODE_COUNTER;
  } else {
      // 버튼 1 처리 (특수 모드 Reset/Decrease)
      if (btn1Pressed) {
          if (activeInteractiveMode != MODE_NONE) {
              interactiveManager.handleButton1(activeInteractiveMode);
              // 카운터일 경우 잠시 값 표시 트리거
              if (activeInteractiveMode == MODE_COUNTER) {
                  lastInteractionTime = millis();
                  interactionMode = MODE_COUNTER;
              }
          } else {
              webLog("Btn 1 pressed (No Action)");
          }
      }

      // 버튼 2 처리 (특수 모드 Start/Increase)
      if (btn2Pressed) {
          if (activeInteractiveMode != MODE_NONE) {
              interactiveManager.handleButton2(activeInteractiveMode);
              // 카운터일 경우 잠시 값 표시 트리거
              if (activeInteractiveMode == MODE_COUNTER) {
                  lastInteractionTime = millis();
                  interactionMode = MODE_COUNTER;
              }
          } else {
              webLog("Btn 2 pressed (No Action)");
          }
      }
  }

  // 버튼 3: 이전 프리셋
  if (buttons.wasPressed(BTN_3)) {
    if (appConfig.presets.size() > 0) {
      appConfig.currentPresetIndex--;
      if (appConfig.currentPresetIndex < 0) {
        appConfig.currentPresetIndex = appConfig.presets.size() - 1;
      }
      webLogf("[Button] Preset Changed: %d", appConfig.currentPresetIndex);
      saveConfigToFile(); // 변경사항 저장
      presetChanged = true;
    }
  }

  // 버튼 4: 다음 프리셋
  if (buttons.wasPressed(BTN_4)) {
    if (appConfig.presets.size() > 0) {
      appConfig.currentPresetIndex++;
      if (appConfig.currentPresetIndex >= appConfig.presets.size()) {
        appConfig.currentPresetIndex = 0;
      }
      webLogf("[Button] Preset Changed: %d", appConfig.currentPresetIndex);
      saveConfigToFile(); // 변경사항 저장
      presetChanged = true;
    }
  }

  if (presetChanged) {
      lastPresetChangeTime = millis();
      // 즉시 반영을 위해 강제로 업데이트 로직 실행 유도 가능하나,
      // 아래 루프에서 자연스럽게 처리됨.
      // 다만 버튼 반응성을 위해 여기서 즉시 그려주는 것도 좋음.
      display.update(appConfig);
      display.displayPreset(appConfig.currentPresetIndex);
  }

  static unsigned long lastUpdate = 0;

  // 0.1초마다 디스플레이 갱신
  if (millis() - lastUpdate > 100)
  {
    lastUpdate = millis();
    display.update(appConfig);
    
    // 프리셋 변경 후 1.5초 동안은 프리셋 번호 표시 (덮어쓰기)
    if (millis() - lastPresetChangeTime < 1500) {
        display.displayPreset(appConfig.currentPresetIndex);
    }
    // 카운터 값 변경 후 1.5초 동안 값 표시 (덮어쓰기)
    else if (millis() - lastInteractionTime < 1500 && interactionMode == MODE_COUNTER) {
        display.displayTemporaryValue(interactiveManager.getDisplayNumber(MODE_COUNTER));
    }
  }

  // 타이트 루프에서 WiFi/OTA 작업이 굶지 않게(권장)
  delay(0);
}
