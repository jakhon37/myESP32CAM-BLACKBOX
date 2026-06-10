#include "network.h"
#include <Arduino.h>

static NetworkMode current_mode;

NetworkMode networkInit(const char* sta_ssid, const char* sta_pass,
                        const char* ap_ssid,  const char* ap_pass,
                        uint32_t timeout_ms)
{
    Serial.printf("[NET] Trying STA: %s\n", sta_ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(sta_ssid, sta_pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeout_ms) {
            Serial.println("[NET] STA failed — switching to AP mode");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            WiFi.softAP(ap_ssid, ap_pass);
            Serial.printf("[NET] AP started — SSID: %s\n", ap_ssid);
            Serial.printf("[NET] AP IP: %s\n",
                          WiFi.softAPIP().toString().c_str());
            current_mode = NetworkMode::AP;
            return current_mode;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("[NET] STA connected — IP: %s\n",
                  WiFi.localIP().toString().c_str());

    // Sync NTP Time
    Serial.println("[NET] Syncing time...");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    // Optionally wait for sync, but we'll let it happen in background
    
    current_mode = NetworkMode::STA;
    return current_mode;
}

String networkIP() {
    if (current_mode == NetworkMode::STA)
        return WiFi.localIP().toString();
    return WiFi.softAPIP().toString();
}

NetworkMode networkMode() { return current_mode; }