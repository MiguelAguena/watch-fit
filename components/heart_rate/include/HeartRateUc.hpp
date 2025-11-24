#pragma once
#include "Uc.hpp"
#include "esp_random.h"
#include <string>
#include "esp_log.h"

#define HEART_RATE_TASK_PERIOD (1000 / portTICK_PERIOD_MS)

template<typename TSubscriberUc>
class HeartRateUc : public Uc<HeartRateUc<TSubscriberUc>> {
private:
    uint8_t heart_rate;
    Uc<TSubscriberUc> subscriber;
    void update_heart_rate(void) { heart_rate = 60 + (uint8_t)(esp_random() % 21); }

public:
    HeartRateUc(Uc<TSubscriberUc>& subscriber) :
    Uc<HeartRateUc<TSubscriberUc>>("HeartRateUc", 4096, 5),
    heart_rate(60 + (uint8_t)(esp_random() % 21)),
    subscriber(subscriber) {}

    ~HeartRateUc() = default;
    void taskLoop() {
        while(true) {
            update_heart_rate();
            ESP_LOGI(TAG, "heart rate updated to %d", heart_rate);
            Message m;
            m.type = MessageType::String;
            std::string data = std::to_string(int(heart_rate));
            m.data = &data;
            this->sendMessage(subscriber.queue, m);
            vTaskDelay(HEART_RATE_TASK_PERIOD);
        }
    }

    static_assert(IUcConcept<HeartRateUc, void*>);
};