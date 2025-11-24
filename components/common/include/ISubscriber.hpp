#pragma once
#include <cstdint>
#include <concepts>

template<typename TSubscriber, typename TMsg>
concept ISubscriberConcept = requires(TSubscriber& subscriber, TMsg& msg) {
    {subscriber.onMessage(msg)} -> std::same_as<void>;
};

template<typename TSubscriber>
concept ISubscriberDestructor = std::destructible<TSubscriber>;