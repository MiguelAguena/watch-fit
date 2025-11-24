#pragma once
#include <cstdint>
#include <concepts>
#include <freertos/FreeRTOS.h>
#include "freertos/queue.h"
#include "freertos/task.h"
#include "Message.hpp"

template<typename TUc, typename T>
concept IUcConcept =
    requires {
        typename TUc::queue_type;
        typename TUc::task_handle_type;
    } &&

    std::same_as<typename TUc::queue_type, QueueHandle_t> &&
    std::same_as<typename TUc::task_handle_type, TaskHandle_t> &&

    requires(TUc& uc, Message message, T params) {
    {TUc::taskEntry(params)} -> std::same_as<void>;
    {uc.sendMessage(message)} -> std::same_as<void>;
    {uc.taskLoop()} -> std::same_as<void>;
    {uc.handle()} -> std::same_as<void>;
};

template<typename DerivedUc>
class Uc {

public:
    using queue_type = QueueHandle_t;
    using task_handle_type = TaskHandle_t;

    QueueHandle_t queue;
    TaskHandle_t taskHandle;

    Uc(const char* name, uint32_t stack, int priority) : 
    queue(xQueueCreate(10, sizeof(Message))) {
        xTaskCreate(&Uc::taskEntry, name, stack, this, priority, &taskHandle);
    }

    static void taskEntry(void* arg) {
        static_cast<DerivedUc*>(arg)->taskLoop();
    }

    void sendMessage(const Message& msg) {
        xQueueSend(queue, &msg, portMAX_DELAY);
    }
};