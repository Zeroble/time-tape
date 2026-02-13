#pragma once

#include <ArduinoJson.h>
#include "Config.h"

void configToJson(JsonDocument &doc, const AppConfig &config);
bool configFromJson(JsonDocument &doc, AppConfig &config);
