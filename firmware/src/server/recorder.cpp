#include "recorder.h"
#include "SD_MMC.h"
#include <time.h>

// AVI Header Structs
struct MainAVIHeader {
    uint32_t dwMicroSecPerFrame;
    uint32_t dwMaxBytesPerSec;
    uint32_t dwPaddingGranularity;
    uint32_t dwFlags;
    uint32_t dwTotalFrames;
    uint32_t dwInitialFrames;
    uint32_t dwStreams;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwWidth;
    uint32_t dwHeight;
    uint32_t dwReserved[4];
} __attribute__((packed));

struct AVIStreamHeader {
    char     fccType[4];
    char     fccHandler[4];
    uint32_t dwFlags;
    uint16_t wPriority;
    uint16_t wLanguage;
    uint32_t dwInitialFrames;
    uint32_t dwScale;
    uint32_t dwRate;
    uint32_t dwStart;
    uint32_t dwLength;
    uint32_t dwSuggestedBufferSize;
    uint32_t dwQuality;
    uint32_t dwSampleSize;
    struct { int16_t left, top, right, bottom; } rcFrame;
} __attribute__((packed));

struct BITMAPINFOHEADER {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} __attribute__((packed));

static bool is_recording = false;
static File video_file;
static String current_filename;
static uint32_t frame_count = 0;
static uint32_t start_time = 0;
static uint32_t max_frame_size = 0;
static StoragePolicy current_policy = StoragePolicy::STOP_WHEN_FULL;

static QueueHandle_t frame_queue = NULL;
static TaskHandle_t recorder_task_handle = NULL;

#define FRAME_QUEUE_LEN 5
#define SD_MIN_SPACE_MB 50

static void deleteOldestVideo() {
    File root = SD_MMC.open("/");
    String oldest_file = "";
    time_t oldest_time = 0xFFFFFFFF;

    File file = root.openNextFile();
    while (file) {
        String name = String(file.name());
        if (name.startsWith("/vid_") && name.endsWith(".avi")) {
            // In a real system we'd check file time, but here we can just delete the first one found 
            // or compare alphabetical names since they are timestamped.
            if (oldest_file == "" || name < oldest_file) {
                oldest_file = name;
            }
        }
        file = root.openNextFile();
    }
    
    if (oldest_file != "") {
        Serial.printf("[REC] Deleting oldest: %s\n", oldest_file.c_str());
        SD_MMC.remove(oldest_file);
    }
}

static bool checkSpace() {
    uint64_t free_bytes = SD_MMC.totalBytes() - SD_MMC.usedBytes();
    if (free_bytes < (uint64_t)SD_MIN_SPACE_MB * 1024 * 1024) {
        if (current_policy == StoragePolicy::OVERWRITE_OLDEST) {
            deleteOldestVideo();
            return true;
        }
        return false;
    }
    return true;
}

static void recorderTask(void* pvParameters) {
    camera_fb_t* fb = NULL;
    while (true) {
        if (xQueueReceive(frame_queue, &fb, portMAX_DELAY) == pdTRUE) {
            if (is_recording && video_file) {
                if (!checkSpace()) {
                    Serial.println("[REC] Storage Full - Stopping");
                    recorderStop();
                } else {
                    // Write MJPEG Chunk
                    video_file.write((const uint8_t*)"00dc", 4);
                    uint32_t size = fb->len;
                    video_file.write((const uint8_t*)&size, 4);
                    video_file.write(fb->buf, fb->len);
                    
                    // Padding
                    if (size % 4 != 0) {
                        uint8_t pad[3] = {0,0,0};
                        video_file.write(pad, 4 - (size % 4));
                    }

                    frame_count++;
                    if (fb->len > max_frame_size) max_frame_size = fb->len;
                }
            }
            esp_camera_fb_return(fb);
        }
    }
}

void recorderInit() {
    frame_queue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(camera_fb_t*));
    xTaskCreate(recorderTask, "recorder_task", 4096, NULL, 4, &recorder_task_handle);
}

bool recorderStart(StoragePolicy policy) {
    if (is_recording) return false;
    if (SD_MMC.cardType() == CARD_NONE) return false;

    current_policy = policy;
    
    // Generate filename with timestamp
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    
    char buf[32];
    if (timeinfo.tm_year > 120) { // check if time is synced
        strftime(buf, sizeof(buf), "/vid_%Y%m%d_%H%M%S.avi", &timeinfo);
    } else {
        snprintf(buf, sizeof(buf), "/vid_%lu.avi", millis());
    }
    current_filename = String(buf);

    video_file = SD_MMC.open(current_filename, FILE_WRITE);
    if (!video_file) return false;

    // Write Placeholder RIFF Header
    uint8_t dummy[2048] = {0};
    video_file.write(dummy, 2048); // Reserves space for headers

    is_recording = true;
    frame_count = 0;
    max_frame_size = 0;
    start_time = millis();
    
    Serial.printf("[REC] Started: %s\n", current_filename.c_str());
    return true;
}

void recorderStop() {
    if (!is_recording) return;
    is_recording = false;

    if (video_file) {
        // Finalize AVI Header
        video_file.seek(0);
        
        uint32_t file_size = video_file.size();
        uint32_t avg_fps = (frame_count * 1000) / (millis() - start_time + 1);
        if (avg_fps == 0) avg_fps = 1;

        video_file.write((const uint8_t*)"RIFF", 4);
        uint32_t riff_size = file_size - 8;
        video_file.write((const uint8_t*)&riff_size, 4);
        video_file.write((const uint8_t*)"AVI ", 4);
        
        // hdrl LIST
        video_file.write((const uint8_t*)"LIST", 4);
        uint32_t hdrl_size = 4 + 8 + sizeof(MainAVIHeader) + 8 + 4 + 8 + sizeof(AVIStreamHeader) + 8 + sizeof(BITMAPINFOHEADER);
        video_file.write((const uint8_t*)&hdrl_size, 4);
        video_file.write((const uint8_t*)"hdrl", 4);

        // avih
        video_file.write((const uint8_t*)"avih", 4);
        uint32_t avih_size = sizeof(MainAVIHeader);
        video_file.write((const uint8_t*)&avih_size, 4);
        
        sensor_t* s = esp_camera_sensor_get();
        MainAVIHeader avih = {
            .dwMicroSecPerFrame = (uint32_t)(1000000 / avg_fps),
            .dwMaxBytesPerSec = 1000000,
            .dwPaddingGranularity = 0,
            .dwFlags = 0,
            .dwTotalFrames = frame_count,
            .dwInitialFrames = 0,
            .dwStreams = 1,
            .dwSuggestedBufferSize = max_frame_size,
            .dwWidth = (uint32_t)(s ? (s->status.framesize == FRAMESIZE_VGA ? 640 : 320) : 640),
            .dwHeight = (uint32_t)(s ? (s->status.framesize == FRAMESIZE_VGA ? 480 : 240) : 480),
            .dwReserved = {0,0,0,0}
        };
        video_file.write((const uint8_t*)&avih, sizeof(avih));

        // strl LIST
        video_file.write((const uint8_t*)"LIST", 4);
        uint32_t strl_size = 4 + 8 + sizeof(AVIStreamHeader) + 8 + sizeof(BITMAPINFOHEADER);
        video_file.write((const uint8_t*)&strl_size, 4);
        video_file.write((const uint8_t*)"strl", 4);

        // strh
        video_file.write((const uint8_t*)"strh", 4);
        uint32_t strh_size = sizeof(AVIStreamHeader);
        video_file.write((const uint8_t*)&strh_size, 4);
        
        AVIStreamHeader strh = {
            .fccType = {'v','i','d','s'},
            .fccHandler = {'M','J','P','G'},
            .dwFlags = 0,
            .wPriority = 0,
            .wLanguage = 0,
            .dwInitialFrames = 0,
            .dwScale = 1,
            .dwRate = avg_fps,
            .dwStart = 0,
            .dwLength = frame_count,
            .dwSuggestedBufferSize = max_frame_size,
            .dwQuality = (uint32_t)-1,
            .dwSampleSize = 0,
            .rcFrame = {0, 0, (int16_t)avih.dwWidth, (int16_t)avih.dwHeight}
        };
        video_file.write((const uint8_t*)&strh, sizeof(strh));

        // strf
        video_file.write((const uint8_t*)"strf", 4);
        uint32_t strf_size = sizeof(BITMAPINFOHEADER);
        video_file.write((const uint8_t*)&strf_size, 4);
        
        BITMAPINFOHEADER strf = {
            .biSize = sizeof(BITMAPINFOHEADER),
            .biWidth = (int32_t)avih.dwWidth,
            .biHeight = (int32_t)avih.dwHeight,
            .biPlanes = 1,
            .biBitCount = 24,
            .biCompression = 0x47504A4D, // "MJPG"
            .biSizeImage = avih.dwWidth * avih.dwHeight * 3,
            .biXPelsPerMeter = 0,
            .biYPelsPerMeter = 0,
            .biClrUsed = 0,
            .biClrImportant = 0
        };
        video_file.write((const uint8_t*)&strf, sizeof(strf));

        // movi LIST
        video_file.seek(2048 - 12); // movi header starts just before frame data
        video_file.write((const uint8_t*)"LIST", 4);
        uint32_t movi_size = file_size - 2048 + 4;
        video_file.write((const uint8_t*)&movi_size, 4);
        video_file.write((const uint8_t*)"movi", 4);

        video_file.close();
        Serial.printf("[REC] Stopped. Frames: %u, Size: %.2f MB\n", 
                      frame_count, (float)file_size / (1024*1024));
    }
}

bool recorderIsRecording() { return is_recording; }

void recorderPushFrame(camera_fb_t* fb) {
    if (!is_recording || !frame_queue) return;
    
    // Create a copy of the FB header but we use the same buffer 
    // Wait, we need to ensure the FB is not returned until the recorder is done.
    // In streamHandler, we'll increment the ref count if possible, or just hold it.
    // Actually, we'll just send the pointer and the recorderTask will call esp_camera_fb_return.
    // This means the caller MUST NOT call esp_camera_fb_return if push succeeds.
    if (xQueueSend(frame_queue, &fb, 0) != pdTRUE) {
        // Queue full, drop frame
        esp_camera_fb_return(fb);
    }
}

uint32_t recorderGetDuration() {
    if (!is_recording) return 0;
    return (millis() - start_time) / 1000;
}

String recorderGetFilename() { return current_filename; }
