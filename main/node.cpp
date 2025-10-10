#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "BleUc.h"

extern "C" void app_main(void) {
    BleUc bleUc;
    xTaskCreate(bleUc.nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
}