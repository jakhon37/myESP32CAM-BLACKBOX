#pragma once
#include <Arduino.h>
#include "esp_camera.h"

enum class StoragePolicy {
    STOP_WHEN_FULL,
    OVERWRITE_OLDEST
};

void recorderInit();
bool recorderStart(StoragePolicy policy);
void recorderStop();
bool recorderIsRecording();
void recorderPushFrame(camera_fb_t* fb);
uint32_t recorderGetDuration(); // in seconds
String recorderGetFilename();
