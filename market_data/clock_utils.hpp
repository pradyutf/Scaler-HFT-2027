#pragma once

#include <chrono>
#include <cstdint>

namespace market {

// High-resolution clock utilities for low-latency measurements
class Clock {
public:
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;
    
    // Get current timestamp in nanoseconds since epoch
    static inline uint64_t now_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock_type::now().time_since_epoch()
        ).count();
    }
    
    // Get current time point
    static inline time_point now() {
        return clock_type::now();
    }
    
    // Calculate duration in nanoseconds between two time points
    static inline uint64_t duration_ns(time_point start, time_point end) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - start
        ).count();
    }
};

} // namespace market
