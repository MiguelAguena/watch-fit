#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "BleUc.hpp"
#include "HeartRateUc.hpp"

extern "C" void app_main(void) {
    BleUc bleUc;
    HeartRateUc<BleUc> heartRate(bleUc);
}