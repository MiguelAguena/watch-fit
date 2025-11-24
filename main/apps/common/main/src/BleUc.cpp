#include "IUc.hpp"


class BleUc {
private:
    QueueHandle_t queue;
    TaskHandle_t taskHandle;

    static void taskEntry(void* arg) {
        reinterpret_cast<BleUc*>(arg)->taskLoop();
    }


public:
    BleUc(const char* name, uint32_t stack, int priority) {
        queue = xQueueCreate(10, sizeof(Message));
        xTaskCreate(&BleUc::taskEntry, name, stack, this, priority, &taskHandle);
    }

    ~BleUc() = default;

    void sendMessage(const Message& msg) {
        xQueueSend(queue, &msg, portMAX_DELAY);
    }


    static_assert(IUcConcept<void* params>);
    static_assert(IUcConstructor);
    static_assert(IUcDestructor);
}