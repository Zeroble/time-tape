#include "NetworkManager.h"
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "Config.h"
#include "ConfigCodec.h"
#include "WebLogger.h"

AsyncWebServer server(80);
AsyncWebSocket wsLog("/ws/log");

namespace
{
enum OtaTarget : uint8_t
{
    OTA_TARGET_NONE = 0,
    OTA_TARGET_FLASH = 1,
    OTA_TARGET_FS = 2
};

struct FwUploadContext
{
    String reason;
    String detail;
    bool success = false;
    bool rebooting = false;
    OtaTarget target = OTA_TARGET_NONE;
};

bool g_fwUploadInProgress = false;
bool g_fwRebootRequested = false;
unsigned long g_fwRebootRequestedAt = 0;
unsigned long g_fwUploadLastActivityAt = 0;
String g_fwUploadError;

constexpr unsigned long kFwRebootDelayMs = 500;
constexpr unsigned long kFwUploadTimeoutMs = 15000;

#if defined(U_SPIFFS)
constexpr int kFsUpdateCommand = U_SPIFFS;
#elif defined(U_FS)
constexpr int kFsUpdateCommand = U_FS;
#else
constexpr int kFsUpdateCommand = -1;
#endif

const char *targetTag(OtaTarget target)
{
    return (target == OTA_TARGET_FS) ? "FS" : "FW";
}

const char *targetSuccessMessage(OtaTarget target)
{
    return (target == OTA_TARGET_FS) ? "filesystem_update_complete" : "update_complete";
}

int targetUpdateCommand(OtaTarget target)
{
    if (target == OTA_TARGET_FLASH)
        return U_FLASH;
    if (target == OTA_TARGET_FS)
        return kFsUpdateCommand;
    return -1;
}

String updateErrorToString(uint8_t error)
{
#ifdef UPDATE_ERROR_OK
    if (error == UPDATE_ERROR_OK)
        return "ok";
#endif
#ifdef UPDATE_ERROR_WRITE
    if (error == UPDATE_ERROR_WRITE)
        return "write_failed";
#endif
#ifdef UPDATE_ERROR_ERASE
    if (error == UPDATE_ERROR_ERASE)
        return "erase_failed";
#endif
#ifdef UPDATE_ERROR_READ
    if (error == UPDATE_ERROR_READ)
        return "read_failed";
#endif
#ifdef UPDATE_ERROR_SPACE
    if (error == UPDATE_ERROR_SPACE)
        return "not_enough_space";
#endif
#ifdef UPDATE_ERROR_SIZE
    if (error == UPDATE_ERROR_SIZE)
        return "invalid_size";
#endif
#ifdef UPDATE_ERROR_STREAM
    if (error == UPDATE_ERROR_STREAM)
        return "stream_error";
#endif
#ifdef UPDATE_ERROR_MD5
    if (error == UPDATE_ERROR_MD5)
        return "md5_mismatch";
#endif
#ifdef UPDATE_ERROR_MAGIC_BYTE
    if (error == UPDATE_ERROR_MAGIC_BYTE)
        return "invalid_firmware";
#endif
#ifdef UPDATE_ERROR_ACTIVATE
    if (error == UPDATE_ERROR_ACTIVATE)
        return "activate_failed";
#endif
#ifdef UPDATE_ERROR_NO_PARTITION
    if (error == UPDATE_ERROR_NO_PARTITION)
        return "no_partition";
#endif
#ifdef UPDATE_ERROR_BAD_ARGUMENT
    if (error == UPDATE_ERROR_BAD_ARGUMENT)
        return "bad_argument";
#endif
#ifdef UPDATE_ERROR_ABORT
    if (error == UPDATE_ERROR_ABORT)
        return "aborted";
#endif
    return String("update_error_") + String(error);
}

void sendJsonError(AsyncWebServerRequest *request, int statusCode, const String &reason, const String &detail)
{
    JsonDocument doc;
    doc["status"] = "error";
    doc["reason"] = reason;
    doc["detail"] = detail;

    String body;
    serializeJson(doc, body);
    request->send(statusCode, "application/json", body);
}

bool hasBinExtension(const String &filename)
{
    String lower = filename;
    lower.toLowerCase();
    return lower.endsWith(".bin");
}

String getFirmwareVersion()
{
#ifdef APP_VERSION
    return String(APP_VERSION);
#elif defined(FW_VERSION)
    return String(FW_VERSION);
#else
    return "unknown";
#endif
}

void clearFwUploadContext(AsyncWebServerRequest *request)
{
    if (request->_tempObject != nullptr)
    {
        delete reinterpret_cast<FwUploadContext *>(request->_tempObject);
        request->_tempObject = nullptr;
    }
}

void setFwUploadFailure(FwUploadContext *ctx, const String &reason, const String &detail)
{
    ctx->reason = reason;
    ctx->detail = detail;
    g_fwUploadError = detail;
}

void finalizeOtaUploadRequest(AsyncWebServerRequest *request, OtaTarget target)
{
    FwUploadContext *ctx = reinterpret_cast<FwUploadContext *>(request->_tempObject);
    if (ctx == nullptr)
    {
        const String missing = (target == OTA_TARGET_FS) ? "filesystem file is missing" : "firmware file is missing";
        sendJsonError(request, 400, "no_file", missing);
        return;
    }

    if (ctx->success)
    {
        JsonDocument doc;
        doc["status"] = "ok";
        doc["message"] = targetSuccessMessage(target);
        doc["rebooting"] = ctx->rebooting;
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body);
    }
    else
    {
        const String reason = ctx->reason.length() ? ctx->reason : "upload_failed";
        const String detail = ctx->detail.length() ? ctx->detail : g_fwUploadError;
        sendJsonError(request, 400, reason, detail);
    }

    clearFwUploadContext(request);
}

void handleOtaUploadChunk(AsyncWebServerRequest *request, OtaTarget target, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    FwUploadContext *ctx = reinterpret_cast<FwUploadContext *>(request->_tempObject);

    if (index == 0)
    {
        clearFwUploadContext(request);
        ctx = new FwUploadContext();
        request->_tempObject = ctx;
        g_fwUploadLastActivityAt = millis();
        ctx->target = target;

        if (g_fwUploadInProgress)
        {
            setFwUploadFailure(ctx, "upload_in_progress", "another upload is already running");
            return;
        }

        if (!hasBinExtension(filename))
        {
            setFwUploadFailure(ctx, "invalid_file_type", "only .bin files are allowed");
            return;
        }

        const int updateCommand = targetUpdateCommand(target);
        if (updateCommand < 0)
        {
            setFwUploadFailure(ctx, "unsupported_target", "filesystem OTA is not supported in this build");
            return;
        }

        g_fwUploadError = "";
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, updateCommand))
        {
            setFwUploadFailure(ctx, "begin_failed", updateErrorToString(Update.getError()));
            return;
        }

        g_fwUploadInProgress = true;
        webLogf("[%s] Upload started: %s", targetTag(target), filename.c_str());
    }

    if (ctx == nullptr || ctx->reason.length())
    {
        return;
    }

    if (!g_fwUploadInProgress)
    {
        setFwUploadFailure(ctx, "upload_state_error", "upload was not initialized");
        return;
    }

    g_fwUploadLastActivityAt = millis();

    if (len > 0)
    {
        const size_t written = Update.write(data, len);
        if (written != len)
        {
            setFwUploadFailure(ctx, "write_failed", updateErrorToString(Update.getError()));
            Update.abort();
            g_fwUploadInProgress = false;
            return;
        }
    }

    if (final)
    {
        if (Update.end(true))
        {
            ctx->success = true;
            if (target == OTA_TARGET_FLASH)
            {
                ctx->rebooting = true;
                g_fwRebootRequested = true;
                g_fwRebootRequestedAt = millis();
            }
            webLogf("[%s] Upload complete", targetTag(target));
        }
        else
        {
            setFwUploadFailure(ctx, "end_failed", updateErrorToString(Update.getError()));
            Update.abort();
            webLogf("[%s] Upload failed: %s", targetTag(target), ctx->detail.c_str());
        }
        g_fwUploadInProgress = false;
    }
}
} // namespace

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

void networkLoop()
{
    const unsigned long now = millis();

    if (g_fwUploadInProgress && (now - g_fwUploadLastActivityAt) > kFwUploadTimeoutMs)
    {
        Update.abort();
        g_fwUploadInProgress = false;
        g_fwUploadError = "upload_timeout";
        webLog("[FW] Upload timeout");
    }

    if (g_fwRebootRequested && (now - g_fwRebootRequestedAt) >= kFwRebootDelayMs)
    {
        g_fwRebootRequested = false;
        webLog("[FW] Rebooting now");
        ESP.restart();
    }
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
        webLog("mDNS started: http://tape.local");

    server.addHandler(&wsLog);

    server.on("/get-config", HTTP_GET, [](AsyncWebServerRequest *r)
              {
        JsonDocument doc;
        configToJson(doc, appConfig);

        String response;
        serializeJson(doc, response);
        r->send(200, "application/json", response); });

    server.on("/set-config", HTTP_POST, [](AsyncWebServerRequest *r) {}, NULL, [](AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
              {
            if (index == 0) {
                r->_tempObject = new String();
            }

            String *body = reinterpret_cast<String *>(r->_tempObject);
            body->reserve(total);
            body->concat(reinterpret_cast<const char *>(data), len);

            if (index + len == total) {
                JsonDocument doc;
                if (deserializeJson(doc, *body)) {
                    delete body;
                    r->_tempObject = nullptr;
                    r->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_json\"}");
                    return;
                }

                AppConfig parsed;
                if (!configFromJson(doc, parsed)) {
                    delete body;
                    r->_tempObject = nullptr;
                    r->send(400, "application/json", "{\"status\":\"error\",\"reason\":\"invalid_schema\"}");
                    return;
                }

                appConfig = parsed;
                saveConfigToFile();

                delete body;
                r->_tempObject = nullptr;
                r->send(200, "application/json", "{\"status\":\"ok\"}");
            } });

    server.on("/fw-info", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        JsonDocument doc;
        doc["chipModel"] = ESP.getChipModel();
        doc["chipRev"] = ESP.getChipRevision();
        doc["freeSketchSpace"] = ESP.getFreeSketchSpace();
        doc["uploadInProgress"] = g_fwUploadInProgress;
        doc["fsUploadSupported"] = (kFsUpdateCommand >= 0);
        doc["version"] = getFirmwareVersion();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response); });

    server.on("/fw-upload", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        finalizeOtaUploadRequest(request, OTA_TARGET_FLASH); },
              [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
        handleOtaUploadChunk(request, OTA_TARGET_FLASH, filename, index, data, len, final); });

    server.on("/fs-upload", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        finalizeOtaUploadRequest(request, OTA_TARGET_FS); },
              [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
              {
        handleOtaUploadChunk(request, OTA_TARGET_FS, filename, index, data, len, final); });

    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    server.begin();
}
