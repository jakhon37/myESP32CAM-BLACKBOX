# ESP32-CAM Smart Stream & Black Box

A modular, high-performance ESP32-CAM firmware featuring MJPEG streaming, integrated web dashboard, and an automated "Black Box" video recording system.

---

## 🚀 Key Features

- **Embedded Dashboard:** No external server needed. Access the full UI directly from the ESP32 IP.
- **MJPEG Streaming:** Optimized live video with adjustable FPS and resolution.
- **Black Box Recording:** Background recording of MJPEG frames into `.avi` files on the SD card.
- **Storage Management:** Choose between "Overwrite Oldest" (circular buffer) or "Stop When Full".
- **NTP Time Sync:** Automatic date/time syncing for human-readable video filenames (e.g., `vid_20260610_130000.avi`).
- **Dual LED Control:** Independent control of the bright **Flash LED** (GPIO 4) and the red **Status LED** (GPIO 33).
- **Web File Browser:** Download or delete recorded videos and snapshots directly from your browser.
- **Smart Connectivity:** Auto-fallback to AP mode (hotspot) if Wi-Fi is unavailable.

---

## 🛠 Hardware Setup

| Component | Specification |
|---|---|
| **Board** | AI-Thinker ESP32-CAM |
| **Programmer** | ESP32-CAM-MB (USB-Serial) |
| **Flash LED** | GPIO 4 (Active HIGH) - Shared with SD Card |
| **Status LED** | GPIO 33 (Active LOW) - Located on the back |
| **Power** | 5V via Micro-USB (recommended 1A supply) |

---

## 💻 First-Time Configuration

1. **Clone the Repo**
2. **Setup Credentials:**
   Copy `firmware/include/config.example.h` to `firmware/include/config.h` and edit:
   ```cpp
   #define STA_SSID "YourWiFiName"
   #define STA_PASS "YourPassword"
   ```
3. **Flash the Board:**
   Using PlatformIO in VSCode:
   - Click the **PlatformIO** icon -> **Upload**.
   - (If using MB board, hold `IO0` while clicking upload).

---

## 🌐 How to Access

### 1. AP Mode (Direct Hotspot)
If the board cannot find your Wi-Fi, it creates its own network:
- **SSID:** `ESP32-CAM`
- **Password:** `12345678`
- **Dashboard URL:** `http://192.168.4.1/`

### 2. STA Mode (Home Wi-Fi)
Once connected to your router, check the **Serial Monitor** for the IP address.
- **Dashboard URL:** `http://192.168.1.xx/`

---

## 🎥 The Dashboard

### Camera Controls
- **Start/Stop Stream:** Manually toggle the video feed to save CPU/Power.
- **Snapshot:** Take a high-quality JPEG and save it to the SD card.
- **Display Size:** Resize the video window in your browser (Small to Full Width).

### Settings (Performance Tuning)
| Setting | Recommended Value | Why? |
|---|---|---|
| **Resolution** | QVGA (320x240) | Best for multi-device stability. |
| **Resolution** | VGA (640x480) | Best for recording quality. |
| **Max FPS** | 5 or 10 FPS | Dramatically reduces CPU lag and fixes LED unresponsiveness. |

### Black Box (Recording)
- **Start Recording:** Begins saving frames to an `.avi` file in the background.
- **Storage Policy:** 
    - `Overwrite`: Deletes the oldest video file when the SD card is nearly full (50MB threshold).
    - `Stop`: Disables recording when space is low.
- **Blinking Red Dot:** Indicates recording is currently active.

### File Browser
- **Refresh SD:** Scans the card for new files.
- **DL Button:** Downloads the file directly to your phone/computer.
- **X Button:** Deletes the file from the SD card.

---

## ⚠️ Important Notes & Troubleshooting

- **SD Card Conflict:** The Flash LED (GPIO 4) is shared with the SD Card. It may flicker during recording. This is a hardware limitation of the ESP32-CAM.
- **Brownouts:** If the board resets when you turn on the Flash LED or start a stream, your USB power supply is too weak. Use a dedicated 5V/1A power adapter.
- **Multi-Device:** The ESP32-CAM can handle 2-3 concurrent connections if the FPS is set low (5-10 FPS). If the stream is too laggy, close tabs on other devices.
- **Local Dashboard:** For developers, you can still run `dashboard/lunchweb.sh` to host the dashboard on your Mac (port 8080) and use the **Change IP** button to point it to any ESP32-CAM on your network.

---

## 📂 Project Structure

```
my-esp32-cam/
├── dashboard/           # Standalone web files (for local Mac hosting)
└── firmware/            # PlatformIO project
    ├── include/         # Configuration headers
    └── src/
        ├── main.cpp     # App entry point
        ├── camera/      # OV2640 driver wrapper
        ├── network/     # Wi-Fi & NTP logic
        └── server/      # Web server, AVI recorder, & UI HTML
```
