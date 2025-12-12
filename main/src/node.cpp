#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "HeartRateTest.hpp"
#include "BlePeripheralRoutines.hpp"

extern "C" void app_main(void) {
    BlePeripheralRoutines ble;
    
    auto *hr = new HeartRateTest();

    xTaskCreate(HeartRateTest::taskEntry, "heartRateTest", 4*1024, hr, 5, NULL);
    xTaskCreate(BlePeripheralRoutines::ble_main_task, "ble", 4*1024, NULL, 5, NULL);
    
}