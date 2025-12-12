#pragma once
#include "esp_random.h"
#include <string>
#include "esp_log.h"

#define HEART_RATE_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

class HeartRateTest {
private:
    uint8_t heart_rate;
    void update_heart_rate(void) { heart_rate = 60 + (uint8_t)(esp_random() % 21); }

public:
    HeartRateTest() {}

    ~HeartRateTest() = default;
    
    void taskLoop() {
        while(true) {
            update_heart_rate();
            printf("heart rate updated to %d\n", heart_rate);
            vTaskDelay(HEART_RATE_TASK_PERIOD);
        }
    }

    static void taskEntry(void* arg) {
        static_cast<HeartRateTest*>(arg)->taskLoop();
    }

    uint8_t getHeartRate() {
        return heart_rate;
    }
};