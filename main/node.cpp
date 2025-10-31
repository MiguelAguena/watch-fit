#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "common.h"
#include "BleUc.hpp"
#include "HeartRate.hpp"

extern "C" void app_main(void) {
    BleUc bleUc;
    HeartRate heartRate;
    xTaskCreate(bleUc.nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(heartRate.heart_rate_task, "Heart Rate", 4 * 1024, NULL, 5, NULL);
}