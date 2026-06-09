# MPU6050 + ESP32-C3 Visualization Tool Plan

## 1. Objective
Create a standalone, web-based experimentation platform for the MPU6050 IMU using an ESP32-C3. This will allow real-time monitoring and visualization of motion data over a local network.

## 2. Hardware Architecture
- **Microcontroller:** ESP32-C3 (configured via PlatformIO).
- **Sensor:** MPU6050 (Accel + Gyro).
- **Connection:** I2C (Default SDA: GPIO 8, SCL: GPIO 9 - configurable).
- **Power:** 3.3V from ESP32 to MPU6050.

## 3. Software Components

### A. Firmware (ESP32-C3 / PlatformIO)
- **Framework:** Arduino.
- **Libraries:**
    - `adafruit/Adafruit MPU6050`: High-level sensor driver.
    - `adafruit/Adafruit Unified Sensor`: Dependency for Adafruit drivers.
    - `me-no-dev/ESPAsyncWebServer`: High-performance async web server.
    - `me-no-dev/AsyncTCP`: Dependency for the web server.
    - `bblanchon/ArduinoJson`: For efficient JSON serialization of telemetry.
- **Key Modules:**
    - `WiFiManager`: Handle connection to local network.
    - `IMUHandler`: Manage MPU6050 initialization and data polling.
    - `TelemetryStreamer`: Broadcast IMU data via WebSockets at ~20-50Hz.

### B. Web Dashboard (Embedded/Single-Page)
- **Frontend Framework:** Vanilla HTML/JS (to keep it lightweight and self-contained).
- **Visualization:**
    - **Three.js:** To render a 3D object (e.g., a simple cube or robot model) that mirrors the IMU's physical orientation.
    - **Chart.js:** Real-time line charts for Accelerometer (m/s²) and Gyroscope (deg/s) readings.
- **Interactions:**
    - "Calibrate" button to zero-out gyro drift and set current orientation as "level".
    - Toggle for raw data vs. filtered data (if fusion is implemented).

## 4. Implementation Phases

### Phase 1: Basic Telemetry (Firmware) - [COMPLETED]
1. Initialize PlatformIO project in `test_IMU/firmware`.
2. Configure `platformio.ini` with `Adafruit MPU6050`, `ESPAsyncWebServer`, and `ArduinoJson` libraries.
3. Implement I2C communication and verify MPU6050 connection.
4. Print raw accel/gyro data to Serial for initial validation.

### Phase 2: Network & WebSocket - [IN PROGRESS]
1. Configure WiFi Station mode.
2. Setup `ESPAsyncWebServer` with a WebSocket endpoint (`/ws`).
3. Stream JSON-formatted IMU data: `{ "accel": [x,y,z], "gyro": [x,y,z], "temp": t }`.

### Phase 3: The Dashboard (HTML/JS)
1. Design the UI layout with placeholders for graphs and 3D view.
2. Implement WebSocket client in JavaScript to receive and parse data.
3. Integrate Chart.js for live data plotting.

### Phase 4: 3D Visualization & Fusion
1. Integrate Three.js for 3D orientation display.
2. (Optional) Implement a simple Complementary Filter on the ESP32 to provide stable Pitch/Roll/Yaw values.
3. Finalize the UI with calibration features and status indicators.

## 5. Experimentation Ideas
- **Vibration Analysis:** Use the graphs to see how different motors or surfaces affect accelerometer noise.
- **Tilt Sensing:** Map the 3D model to understand gravity-based orientation.
- **Filter Tuning:** Compare raw data vs. filtered data to see the effect of gyro integration and drift compensation.
