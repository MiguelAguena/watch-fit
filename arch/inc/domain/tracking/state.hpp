#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace domain::tracking {

    struct TrackingState {
        // Placeholder for tracking state variables
        float position[3]{ 0.0f, 0.0f, 0.0f };
        float orientation[4]{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

}