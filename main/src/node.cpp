#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "BleUc.hpp"
#include "HeartRateUc.hpp"
#include "BleNetworkPort.hpp"

extern "C" void app_main(void) {
    BleUc bleUc;
    //BleNetworkPort bleNetWorkPort;
    HeartRateUc<BleUc> heartRate(bleUc);
}