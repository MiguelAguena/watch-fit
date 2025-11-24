#include "BleUc.hpp"

void BleUc::handle(std::string value) {
    ESP_LOGI(TAG, "Received heart rate equal to %s", value);
}

BleUc::BleUc() : 
Uc<BleUc>("BleUc", 4096, 5) {}

BleUc::~BleUc() = default;

void BleUc::taskLoop() {
    Message msg;

    while(true) {
        if(xQueueReceive(queue, &msg, portMAX_DELAY)) {
            if(msg.type == MessageType::String) {
                auto str = *static_cast<std::string*>(msg.data);
                handle(str);
            }
        }
    }
}