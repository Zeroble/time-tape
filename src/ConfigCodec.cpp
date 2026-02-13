#include "ConfigCodec.h"
#include <cstring>

namespace
{
const char *payloadKindToString(PayloadKind kind)
{
    switch (kind)
    {
    case PAYLOAD_DDAY:
        return "dday";
    case PAYLOAD_COUNTER:
        return "counter";
    case PAYLOAD_TIMER:
        return "timer";
    case PAYLOAD_POMODORO:
        return "pomodoro";
    case PAYLOAD_NONE:
    default:
        return "none";
    }
}

PayloadKind payloadKindFromJson(const JsonVariantConst &kindValue)
{
    if (kindValue.is<int>())
    {
        int raw = kindValue.as<int>();
        if (raw >= (int)PAYLOAD_NONE && raw <= (int)PAYLOAD_POMODORO)
        {
            return (PayloadKind)raw;
        }
        return PAYLOAD_NONE;
    }

    const char *kindStr = kindValue | "none";
    if (strcmp(kindStr, "dday") == 0)
        return PAYLOAD_DDAY;
    if (strcmp(kindStr, "counter") == 0)
        return PAYLOAD_COUNTER;
    if (strcmp(kindStr, "timer") == 0)
        return PAYLOAD_TIMER;
    if (strcmp(kindStr, "pomodoro") == 0)
        return PAYLOAD_POMODORO;
    return PAYLOAD_NONE;
}

void normalizeRingPayload(RingConfig &ring)
{
    if (ring.mode == 4)
    {
        if (ring.payload.kind != PAYLOAD_DDAY)
            payloadSetDDay(ring.payload, 0);
        return;
    }
    if (ring.mode == MODE_COUNTER)
    {
        if (ring.payload.kind != PAYLOAD_COUNTER)
            payloadSetCounter(ring.payload, 0);
        return;
    }
    if (ring.mode == MODE_TIMER)
    {
        if (ring.payload.kind != PAYLOAD_TIMER)
            payloadSetTimer(ring.payload, 0, false);
        return;
    }
    if (ring.mode == MODE_POMODORO)
    {
        if (ring.payload.kind != PAYLOAD_POMODORO)
            payloadSetPomodoro(ring.payload, 0, 0, false);
        return;
    }
    payloadSetNone(ring.payload);
}

void normalizeSegmentPayload(SegmentConfig &segment)
{
    if (segment.mode == 5)
    {
        if (segment.payload.kind != PAYLOAD_DDAY)
            payloadSetDDay(segment.payload, 0);
        return;
    }
    payloadSetNone(segment.payload);
}

void payloadToJson(JsonObject obj, const ModePayload &payload)
{
    obj["kind"] = payloadKindToString(payload.kind);
    switch (payload.kind)
    {
    case PAYLOAD_DDAY:
        obj["ddayIndex"] = payload.value.ddayIndex;
        break;
    case PAYLOAD_COUNTER:
        obj["counterTarget"] = payload.value.counterTarget;
        break;
    case PAYLOAD_TIMER:
        obj["timerSeconds"] = payload.value.timer.totalSeconds;
        obj["displaySeconds"] = payload.value.timer.displaySeconds;
        break;
    case PAYLOAD_POMODORO:
        obj["workMinutes"] = payload.value.pomodoro.workMinutes;
        obj["restMinutes"] = payload.value.pomodoro.restMinutes;
        obj["displaySeconds"] = payload.value.pomodoro.displaySeconds;
        break;
    case PAYLOAD_NONE:
    default:
        break;
    }
}

void payloadFromJson(JsonObject obj, ModePayload &payload)
{
    if (obj.isNull())
    {
        payloadSetNone(payload);
        return;
    }

    PayloadKind kind = payloadKindFromJson(obj["kind"]);
    switch (kind)
    {
    case PAYLOAD_DDAY:
        payloadSetDDay(payload, obj["ddayIndex"] | 0);
        break;
    case PAYLOAD_COUNTER:
        payloadSetCounter(payload, obj["counterTarget"] | 0L);
        break;
    case PAYLOAD_TIMER:
        payloadSetTimer(payload, obj["timerSeconds"] | 0L, obj["displaySeconds"] | false);
        break;
    case PAYLOAD_POMODORO:
        payloadSetPomodoro(payload, obj["workMinutes"] | 0L, obj["restMinutes"] | 0L, obj["displaySeconds"] | false);
        break;
    case PAYLOAD_NONE:
    default:
        payloadSetNone(payload);
        break;
    }
}

void ringToJson(JsonObject obj, const RingConfig &ring)
{
    obj["mode"] = ring.mode;
    obj["colorMode"] = ring.colorMode;
    obj["colorFill"] = ring.colorFill;
    obj["colorFill2"] = ring.colorFill2;
    obj["colorEmpty"] = ring.colorEmpty;
    payloadToJson(obj["payload"].to<JsonObject>(), ring.payload);
}

void ringFromJson(JsonObject obj, RingConfig &ring)
{
    ring.mode = obj["mode"] | 0;
    ring.colorMode = obj["colorMode"] | 0;
    ring.colorFill = obj["colorFill"] | 0U;
    ring.colorFill2 = obj["colorFill2"] | 0U;
    ring.colorEmpty = obj["colorEmpty"] | 0U;
    payloadFromJson(obj["payload"], ring.payload);
    normalizeRingPayload(ring);
}

void segmentToJson(JsonObject obj, const SegmentConfig &segment)
{
    obj["mode"] = segment.mode;
    payloadToJson(obj["payload"].to<JsonObject>(), segment.payload);
}

void segmentFromJson(JsonObject obj, SegmentConfig &segment)
{
    segment.mode = obj["mode"] | 1;
    payloadFromJson(obj["payload"], segment.payload);
    normalizeSegmentPayload(segment);
}

bool presetFromV2Json(JsonObject pObj, Preset &preset)
{
    if (!pObj["inner"].is<JsonObject>() || !pObj["outer"].is<JsonObject>() || !pObj["segment"].is<JsonObject>())
    {
        return false;
    }

    ringFromJson(pObj["inner"].as<JsonObject>(), preset.inner);
    ringFromJson(pObj["outer"].as<JsonObject>(), preset.outer);
    segmentFromJson(pObj["segment"].as<JsonObject>(), preset.segment);
    return true;
}

void enforcePresetRules(Preset &preset)
{
    const bool innerInteractive = isInteractiveMode(preset.inner.mode);
    const bool outerInteractive = isInteractiveMode(preset.outer.mode);

    // 버튼 입력 충돌 방지: inner/outer 동시 interactive 금지
    if (innerInteractive && outerInteractive)
    {
        preset.outer.mode = 0;
        payloadSetNone(preset.outer.payload);
    }

    int activeInteractiveMode = MODE_NONE;
    if (isInteractiveMode(preset.inner.mode))
    {
        activeInteractiveMode = preset.inner.mode;
    }
    else if (isInteractiveMode(preset.outer.mode))
    {
        activeInteractiveMode = preset.outer.mode;
    }

    // 7세그 interactive 표시는 실제 active interactive mode와 일치할 때만 허용
    if ((preset.segment.mode == MODE_COUNTER || preset.segment.mode == MODE_TIMER || preset.segment.mode == MODE_POMODORO) &&
        preset.segment.mode != activeInteractiveMode)
    {
        preset.segment.mode = 0; // auto
        payloadSetNone(preset.segment.payload);
    }

    normalizeRingPayload(preset.inner);
    normalizeRingPayload(preset.outer);
    normalizeSegmentPayload(preset.segment);
}
} // namespace

void configToJson(JsonDocument &doc, const AppConfig &config)
{
    doc.clear();
    doc["version"] = 2;
    doc["curIdx"] = config.currentPresetIndex;
    doc["bri"] = config.brightness;
    doc["nEn"] = config.nightModeEnabled;
    doc["nS"] = config.nightStartHour;
    doc["nE"] = config.nightEndHour;
    doc["nB"] = config.nightBrightness;

    JsonArray presets = doc["presets"].to<JsonArray>();
    for (const Preset &p : config.presets)
    {
        JsonObject obj = presets.add<JsonObject>();
        ringToJson(obj["inner"].to<JsonObject>(), p.inner);
        ringToJson(obj["outer"].to<JsonObject>(), p.outer);
        segmentToJson(obj["segment"].to<JsonObject>(), p.segment);
    }

    JsonArray ddays = doc["ddays"].to<JsonArray>();
    for (const DDay &d : config.ddays)
    {
        JsonObject obj = ddays.add<JsonObject>();
        obj["n"] = d.name;
        obj["s"] = d.startDate;
        obj["t"] = d.targetDate;
    }
}

bool configFromJson(JsonDocument &doc, AppConfig &config)
{
    AppConfig parsed;
    parsed.currentPresetIndex = doc["curIdx"] | 0;
    parsed.brightness = doc["bri"] | 50;
    parsed.nightModeEnabled = doc["nEn"] | false;
    parsed.nightStartHour = doc["nS"] | 22;
    parsed.nightEndHour = doc["nE"] | 7;
    parsed.nightBrightness = doc["nB"] | 10;

    JsonArray presets = doc["presets"];
    if (presets.isNull())
    {
        return false;
    }

    for (JsonObject pObj : presets)
    {
        Preset p;
        if (!presetFromV2Json(pObj, p))
        {
            return false;
        }
        enforcePresetRules(p);
        parsed.presets.push_back(p);
    }

    JsonArray ddays = doc["ddays"];
    if (!ddays.isNull())
    {
        for (JsonObject dObj : ddays)
        {
            DDay d;
            d.name = dObj["n"] | "";
            d.startDate = dObj["s"] | "";
            d.targetDate = dObj["t"] | "";
            parsed.ddays.push_back(d);
        }
    }

    if (parsed.presets.empty())
    {
        return false;
    }
    if (parsed.currentPresetIndex < 0 || parsed.currentPresetIndex >= (int)parsed.presets.size())
    {
        parsed.currentPresetIndex = 0;
    }

    config = parsed;
    return true;
}
