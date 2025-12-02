#pragma once

#include "domain/tracking/state.hpp"

namespace driven_ports::state {

    class IStatePort {
    public:
        virtual ~IStatePort() = default;

        virtual domain::tracking::TrackingState get_tracking_state() const = 0;

    };
};