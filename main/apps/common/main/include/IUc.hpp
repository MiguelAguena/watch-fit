#pragma once
#include <cstdint>
#include <concepts>
#include <freertos/FreeRTOS.h>
#include "freertos/queue.h"
#include "Message.hpp"

template<typename TUc, typename T>
concept IUcConcept = requires(TUc& uc, Message message, T params) {
    {uc.queue} -> std::same_as<QueueHandle_t>;
    {uc.taskHandle} -> std::same_as<TaskHandle_t>;
    {uc.sendMessage(message)} -> std::same_as<void>;
    {TUc::taskEntry(params)} -> std::same_as<void>;
    {uc.taskLoop()} -> std::same_as<void>;
};

template<typename TUc>
concept IUcConstructor = std::constructible_from<TUc, const char*, uint32_t, int>;

template<typename TUc>
concept IUcDestructor = std::destructible<TUc>;