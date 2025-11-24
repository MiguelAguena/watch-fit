#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "BleUc.hpp"

extern "C" void print(void*) {
    printf("a");
}

extern "C" void app_main(void) {
    BleUc bleUc;
    xTaskCreate(print, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
}