#include <Arduino.h>
#include "../include/config.h"
#include "camera/camera.h"
#include "network/network.h"
#include "server/server.h"
#include "esp_system.h"


void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32-CAM ===");

    esp_reset_reason_t reason = esp_reset_reason();
    switch(reason) {
        case ESP_RST_BROWNOUT: Serial.println("[RESET] Brownout"); break;
        case ESP_RST_PANIC:    Serial.println("[RESET] Panic/crash"); break;
        case ESP_RST_WDT:      Serial.println("[RESET] Watchdog"); break;
        case ESP_RST_SW:       Serial.println("[RESET] Software"); break;
        default: Serial.printf("[RESET] Reason: %d\n", reason); break;
    }

    if (!cameraInit()) {
        Serial.println("Camera failed — halting");
        while (true) delay(1000);
    }

    auto mode = networkInit(STA_SSID, STA_PASS,
                            AP_SSID,  AP_PASS,
                            10000);

    String ip = networkIP();

    if (mode == NetworkMode::AP) {
        Serial.println("=== AP MODE ===");
        Serial.printf("Connect to Wi-Fi: %s / %s\n", AP_SSID, AP_PASS);
    } else {
        Serial.println("=== STA MODE ===");
    }

    Serial.printf("Stream:   http://%s/stream\n",  ip.c_str());
    Serial.printf("Snapshot: http://%s/capture\n", ip.c_str());
    Serial.printf("Status:   http://%s/status\n",  ip.c_str());

    serverStart();
}

void loop() {
    delay(10000);
}