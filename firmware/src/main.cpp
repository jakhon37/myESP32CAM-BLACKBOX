// test_IMU/firmware/src/main.cpp - ULTIMATE STABILITY VERSION
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "wifi_config.h"

// Default ESP32-C3 I2C pins
#define I2C_SDA 8
#define I2C_SCL 9

// --- Access Point Fallback Settings ---
const char* ap_ssid = "IMU-Visualizer-Fallback";
const char* ap_password = "password123";

Adafruit_MPU6050 mpu;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Global JSON buffer to avoid heap fragmentation
JsonDocument jsonDoc; // Using new ArduinoJson 7 syntax
char jsonBuffer[256];

// Low Pass Filter variables
float lpfAlpha = 0.8; // Sharper response (0.8 = Realtime, 0.2 = Smooth)
float filteredAX = 0, filteredAY = 0, filteredAZ = 0;

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("🚀 Mac Linked! ID: %u\n", client->id());
    // Reset filters on new connection
    filteredAX = filteredAY = filteredAZ = 0;
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("📡 Mac Unlinked! ID: %u\n", client->id());
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Initialize IMU
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!mpu.begin()) {
    Serial.println("❌ IMU Fail!");
    while (1) delay(10);
  }
  Serial.println("✅ IMU Ready");

  // Set sensor ranges for better stability
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // 2. Smart WiFi Logic (With Hard Switching)
  Serial.printf("Searching for %s...", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  unsigned long startAttempt = millis();
  bool connected = false;

  while (millis() - startAttempt < 8000) { // 8 second timeout
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(500);
    Serial.print(".");
  }

  if (connected) {
    Serial.println("\n✅ Network Found!");
    Serial.print("📍 IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ Network Not Found. Switching to AP-ONLY Mode.");
    WiFi.disconnect(); 
    WiFi.mode(WIFI_AP); 
    delay(100);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("📍 AP IP: "); Serial.println(WiFi.softAPIP());
    delay(500); // Wait for radio to stabilize
  }

  // 3. Setup Services (Only after IP is confirmed)
  if (MDNS.begin("imu")) {
    Serial.println("📡 mDNS Started: imu.local");
  }
  
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  
  // High-performance WebSocket settings
  // ws.setAuthentication("admin", "admin"); // Commented out for easier AP testing
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "IMU Stable Server Online");
  });

  server.begin();
  Serial.println("🔥 System Stabilized. Ready for Link.");
}

void loop() {
  ws.cleanupClients();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 20) { // Stable 50Hz
    lastMsg = millis();
    
    if (ws.count() > 0 && ws.availableForWriteAll()) {
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      // Apply Low Pass Filter (Alpha Filter)
      filteredAX = (lpfAlpha * a.acceleration.x) + ((1.0 - lpfAlpha) * filteredAX);
      filteredAY = (lpfAlpha * a.acceleration.y) + ((1.0 - lpfAlpha) * filteredAY);
      filteredAZ = (lpfAlpha * a.acceleration.z) + ((1.0 - lpfAlpha) * filteredAZ);

      jsonDoc.clear();
      jsonDoc["ax"] = filteredAX;
      jsonDoc["ay"] = filteredAY;
      jsonDoc["az"] = filteredAZ;
      jsonDoc["gx"] = g.gyro.x;
      jsonDoc["gy"] = g.gyro.y;
      jsonDoc["gz"] = g.gyro.z;

      serializeJson(jsonDoc, jsonBuffer);
      ws.textAll(jsonBuffer);
    }
  }
}
