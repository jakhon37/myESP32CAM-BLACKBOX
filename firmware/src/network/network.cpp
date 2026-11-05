#include "network.h"
#include <Arduino.h>

// While on the hotspot, retry the home network this often, and for this long each time
#define STA_RETRY_INTERVAL_MS 30000
#define STA_RETRY_WINDOW_MS   10000

static NetworkMode current_mode;
static const char* retry_ssid;
static const char* retry_pass;
static uint32_t    last_retry_ms;
static bool        retrying = false;

static void onStaConnected() {
    Serial.println();
    Serial.printf("[NET] STA connected — IP: %s\n",
                  WiFi.localIP().toString().c_str());

    // Sync NTP Time
    Serial.println("[NET] Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    // Optionally wait for sync, but we'll let it happen in background

    current_mode = NetworkMode::STA;
}

NetworkMode networkInit(const char* sta_ssid, const char* sta_pass,
                        const char* ap_ssid,  const char* ap_pass,
                        uint32_t timeout_ms)
{
    retry_ssid = sta_ssid;
    retry_pass = sta_pass;

    Serial.printf("[NET] Trying STA: %s\n", sta_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(sta_ssid, sta_pass);
    // Power saving delays incoming packets by hundreds of ms, which slows the stream
    WiFi.setSleep(false);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("[NET] STA failed — switching to AP mode");
            // One attempt per retry, so the radio stays on the hotspot's channel in between
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            WiFi.softAP(ap_ssid, ap_pass);
            Serial.printf("[NET] AP started — SSID: %s\n", ap_ssid);
            Serial.printf("[NET] AP IP: %s\n",
                          WiFi.softAPIP().toString().c_str());
            last_retry_ms = millis();
            current_mode = NetworkMode::AP;
            return current_mode;
        }
        delay(500);
        Serial.print(".");
    }

    onStaConnected();
    return current_mode;
}

void networkLoop() {
    if (current_mode != NetworkMode::AP) return;

    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setAutoReconnect(true);
        WiFi.mode(WIFI_STA);
        retrying = false;
        onStaConnected();
    } else if (retrying) {
        if (millis() - last_retry_ms > STA_RETRY_WINDOW_MS) {
            Serial.println("[NET] STA retry failed");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            retrying = false;
            last_retry_ms = millis();
        }
    } else if (millis() - last_retry_ms > STA_RETRY_INTERVAL_MS &&
               WiFi.softAPgetStationNum() == 0) {
        // Not while someone is on the hotspot, because joining scans other channels
        Serial.printf("[NET] Retrying STA: %s\n", retry_ssid);
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(retry_ssid, retry_pass);
        last_retry_ms = millis();
        retrying = true;
    }
}

String networkIP() {
    if (current_mode == NetworkMode::STA)
        return WiFi.localIP().toString();
    return WiFi.softAPIP().toString();
}

NetworkMode networkMode() { return current_mode; }