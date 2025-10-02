#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

extern "C" void app_main(void);  // IDF expects C symbol

static const char* TAG = "app";

extern "C" 
void app_main(void) {
    xTaskCreate(
        [](void*) {
            while (true) {
                ESP_LOGI(TAG, "Hello from C++ FreeRTOS task");
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        },
        "blinker", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr
    );
}
