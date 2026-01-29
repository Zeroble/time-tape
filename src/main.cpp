#include <Arduino.h>
#include "Config.h"
#include "managers/DisplayManager.h"
#include "NetworkManager.h"
#include "TimeLogic.h"

DisplayManager display;

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // 1. 설정 로드
  loadConfig();

  // 2. 하드웨어 초기화
  display.begin();

  // 3. 네트워크 연결 (WiFi -> mDNS -> WebServer)
  setupNetwork();

  // 4. 시간 동기화
  setupTime();
}

void loop()
{
  static unsigned long lastUpdate = 0;

  // 0.1초마다 디스플레이 갱신
  if (millis() - lastUpdate > 100)
  {
    lastUpdate = millis();
    display.update(appConfig);
  }
}
