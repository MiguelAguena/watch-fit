#include "BleUc.hpp"

void BleUc::handle() {
    bleRoutines.ble_main_task();
}

BleUc::BleUc() : 
Uc("BleUc", 4096, 5),
bleRoutines(BleRoutines()) {}

BleUc::~BleUc() = default;

void BleUc::taskLoop() {
    Message msg;

    while(true) {
        if(xQueueReceive(queue, &msg, portMAX_DELAY)) {
            if(msg.type == MessageType::ValueMessage) {
                handle();
            }
        }
    }
}