#include "server.h"
#include "esp_http_server.h"
#include "esp_camera.h"
#include <Arduino.h>
#include "SD_MMC.h"
#include "index_html.h"
#include "recorder.h"

static httpd_handle_t server = NULL;
#define FLASH_GPIO 4
#define STATUS_GPIO 33

// ---- helpers ----
static void setCameraResolution(const char* size) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return;
    if      (strcmp(size, "QQVGA") == 0) s->set_framesize(s, FRAMESIZE_QQVGA);
    else if (strcmp(size, "QVGA")  == 0) s->set_framesize(s, FRAMESIZE_QVGA);
    else if (strcmp(size, "VGA")   == 0) s->set_framesize(s, FRAMESIZE_VGA);
    else if (strcmp(size, "SVGA")  == 0) s->set_framesize(s, FRAMESIZE_SVGA);
    else if (strcmp(size, "XGA")   == 0) s->set_framesize(s, FRAMESIZE_XGA);
}

static bool sdInit() {
    if (!SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode (frees GPIO)
        Serial.println("[SD] Mount failed");
        return false;
    }
    uint8_t type = SD_MMC.cardType();
    if (type == CARD_NONE) {
        Serial.println("[SD] No card");
        return false;
    }
    Serial.printf("[SD] OK — %.1f GB\n",
                  (float)SD_MMC.totalBytes() / (1024*1024*1024));
    return true;
}

// ---- / (index) ----
static esp_err_t indexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static uint32_t fps_delay = 100; // default ~10 FPS
static bool ledOn = false;

// ---- /fps ----
static esp_err_t fpsHandler(httpd_req_t* req) {
    char query[32];
    char val_str[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "val", val_str, sizeof(val_str)) == ESP_OK) {
        int val = atoi(val_str);
        if (val == 0) fps_delay = 0;
        else         fps_delay = 1000 / val;
        Serial.printf("[SRV] FPS Delay → %d ms\n", fps_delay);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// ---- /record ----
static esp_err_t recordHandler(httpd_req_t* req) {
    char query[64];
    char action[16];
    char policy_str[16] = "stop";

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "action", action, sizeof(action)) == ESP_OK) {
            if (strcmp(action, "start") == 0) {
                httpd_query_key_value(query, "policy", policy_str, sizeof(policy_str));
                StoragePolicy policy = (strcmp(policy_str, "overwrite") == 0) ? 
                                        StoragePolicy::OVERWRITE_OLDEST : 
                                        StoragePolicy::STOP_WHEN_FULL;
                recorderStart(policy);
            } else if (strcmp(action, "stop") == 0) {
                recorderStop();
            }
        }
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf), 
             "{\"recording\":%s,\"duration\":%lu,\"file\":\"%s\"}",
             recorderIsRecording() ? "true" : "false",
             recorderGetDuration(),
             recorderGetFilename().c_str());
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ---- /stream ----
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY =
    "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t streamHandler(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    Serial.println("[SRV] Stream started");

    while (true) {
        uint32_t start_time = millis();

        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[CAM] Frame capture failed");
            break;
        }

        // If recording, we need a "duplicate" of the buffer 
        // since we want to send it to the client AND save it to SD.
        // The recorder takes ownership and returns the FB, 
        // so for the client we'll send the data FIRST.
        
        char part_buf[64];
        size_t hlen = snprintf(part_buf, sizeof(part_buf),
                               STREAM_PART, fb->len);

        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY,
                                    strlen(STREAM_BOUNDARY));
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);

        if (recorderIsRecording()) {
            recorderPushFrame(fb); // Recorder will call esp_camera_fb_return
        } else {
            esp_camera_fb_return(fb);
        }

        if (res != ESP_OK) {
            Serial.printf("[SRV] Stream send failed: %d\n", res);
            break;
        }

        // FPS Control & Yield
        uint32_t elapsed = millis() - start_time;
        if (fps_delay > elapsed) {
            delay(fps_delay - elapsed);
        } else {
            delay(1); // minimal yield
        }
    }
    Serial.println("[SRV] Stream stopped");
    return res;
}

// ---- /capture ----
static esp_err_t captureHandler(httpd_req_t* req) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }

    // save to SD if available
    if (SD_MMC.cardType() != CARD_NONE) {
        String path = "/snap_" + String(millis()) + ".jpg";
        File f = SD_MMC.open(path, FILE_WRITE);
        if (f) {
            f.write(fb->buf, fb->len);
            f.close();
            Serial.printf("[SD] Saved %s\n", path.c_str());
        }
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition",
                       "inline; filename=capture.jpg");
    httpd_resp_send(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

// ---- /led ----
static esp_err_t ledHandler(httpd_req_t* req) {
    char query[64];
    char state[8];
    char type[16] = "flash"; // default
    bool on = false;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "type", type, sizeof(type)) != ESP_OK) {
            strcpy(type, "flash");
        }
        if (httpd_query_key_value(query, "state", state, sizeof(state)) == ESP_OK) {
            on = (strcmp(state, "on") == 0);
            
            if (strcmp(type, "status") == 0) {
                // Status LED is Active LOW
                digitalWrite(STATUS_GPIO, on ? LOW : HIGH);
                Serial.printf("[SRV] Status LED → %s\n", on ? "ON" : "OFF");
            } else {
                // Flash LED is Active HIGH
                digitalWrite(FLASH_GPIO, on ? HIGH : LOW);
                Serial.printf("[SRV] Flash LED → %s\n", on ? "ON" : "OFF");
            }
        }
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    char res_buf[64];
    snprintf(res_buf, sizeof(res_buf), "{\"led\":\"%s\",\"type\":\"%s\"}", on ? "on" : "off", type);
    httpd_resp_sendstr(req, res_buf);
    return ESP_OK;
}

// ---- /resolution ----
static esp_err_t resolutionHandler(httpd_req_t* req) {
    char query[32];
    char size[16];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "size", size, sizeof(size)) == ESP_OK) {
        setCameraResolution(size);
        Serial.printf("[CAM] Resolution → %s\n", size);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// ---- /sdinfo ----
static esp_err_t sdinfoHandler(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    char buf[128];
    if (SD_MMC.cardType() == CARD_NONE) {
        snprintf(buf, sizeof(buf), "{\"mounted\":false}");
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"mounted\":true,\"total_mb\":%llu,\"used_mb\":%llu}",
                 SD_MMC.totalBytes() / (1024*1024),
                 SD_MMC.usedBytes()  / (1024*1024));
    }
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// ---- /download ----
static esp_err_t downloadHandler(httpd_req_t* req) {
    char query[64];
    char filename[64];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    String path = "/" + String(filename);
    if (!SD_MMC.exists(path)) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    File f = SD_MMC.open(path);
    if (!f) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    if (path.endsWith(".avi")) httpd_resp_set_type(req, "video/x-msvideo");
    else if (path.endsWith(".jpg")) httpd_resp_set_type(req, "image/jpeg");
    else httpd_resp_set_type(req, "application/octet-stream");

    char content_disposition[128];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=%s", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    uint8_t buf[1024];
    size_t len;
    while ((len = f.read(buf, sizeof(buf))) > 0) {
        if (httpd_resp_send_chunk(req, (const char*)buf, len) != ESP_OK) {
            f.close();
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    f.close();
    return ESP_OK;
}

// ---- /delete ----
static esp_err_t deleteHandler(httpd_req_t* req) {
    char query[64];
    char filename[64];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "file", filename, sizeof(filename)) == ESP_OK) {
        String path = "/" + String(filename);
        if (SD_MMC.remove(path)) {
            Serial.printf("[SD] Deleted: %s\n", path.c_str());
        }
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

// ---- /sdfiles ----
static esp_err_t sdfilesHandler(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    if (SD_MMC.cardType() == CARD_NONE) {
        httpd_resp_sendstr(req, "{\"files\":[]}");
        return ESP_OK;
    }

    String json = "{\"files\":[";
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    bool first = true;
    while (file) {
        if (!file.isDirectory()) {
            if (!first) json += ",";
            json += "\"" + String(file.name()) + "\"";
            first = false;
        }
        file = root.openNextFile();
    }
    json += "]}";
    httpd_resp_sendstr(req, json.c_str());
    return ESP_OK;
}

// ---- /status ----
static esp_err_t statusHandler(httpd_req_t* req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"status\":\"ok\",\"free_heap\":%lu,\"psram\":%s}",
             ESP.getFreeHeap(),
             psramFound() ? "true" : "false");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

void serverStart() {
    sdInit();
    recorderInit();

    // Init LEDs after SD to avoid pin conflicts
    pinMode(FLASH_GPIO, OUTPUT);
    digitalWrite(FLASH_GPIO, LOW);

    pinMode(STATUS_GPIO, OUTPUT);
    digitalWrite(STATUS_GPIO, HIGH); // Active LOW, so HIGH = OFF

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.lru_purge_enable = true;
    
    // Performance Tweak: Increase priority and stack
    config.task_priority = 5; // Standard is 5, but we ensure it's prioritized
    config.stack_size = 8192; // Increase stack for more stable handling

    httpd_uri_t uris[] = {
        { "/",           HTTP_GET, indexHandler,      NULL },
        { "/stream",     HTTP_GET, streamHandler,     NULL },
        { "/capture",    HTTP_GET, captureHandler,    NULL },
        { "/led",        HTTP_GET, ledHandler,        NULL },
        { "/fps",        HTTP_GET, fpsHandler,        NULL },
        { "/record",     HTTP_GET, recordHandler,     NULL },
        { "/download",   HTTP_GET, downloadHandler,   NULL },
        { "/delete",     HTTP_GET, deleteHandler,     NULL },
        { "/resolution", HTTP_GET, resolutionHandler, NULL },
        { "/sdinfo",     HTTP_GET, sdinfoHandler,     NULL },
        { "/sdfiles",    HTTP_GET, sdfilesHandler,    NULL },
        { "/status",     HTTP_GET, statusHandler,     NULL },
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        for (auto& uri : uris)
            httpd_register_uri_handler(server, &uri);
        Serial.println("[SRV] Started on port 80");
    }
}