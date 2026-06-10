#pragma once
#include <WiFi.h>

enum class NetworkMode { STA, AP };

NetworkMode networkInit(const char* sta_ssid, const char* sta_pass,
                        const char* ap_ssid,  const char* ap_pass,
                        uint32_t timeout_ms = 10000);

String networkIP();
NetworkMode networkMode();