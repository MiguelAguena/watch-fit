#pragma once
#include <cstddef>
#include <cstdint>

#include "domain/tracking/state.hpp"

namespace driven_ports::buffer {

    class IBufferPort {
    public:
        virtual ~IBufferPort() = default;

        virtual bool append(domain::tracking::TrackingState const& state) = 0;
        virtual bool has_data() const = 0;
        virtual bool send_to_parent() = 0;
    };

}