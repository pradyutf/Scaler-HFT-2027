#pragma once

#include <cstdint>
#include <array>
#include <string_view>

namespace market {

// Fixed-size market data message for efficient serialization
struct alignas(64) MarketData {
    static constexpr size_t INSTRUMENT_SIZE = 16;
    
    std::array<char, INSTRUMENT_SIZE> instrument;
    double bid;
    double ask;
    uint64_t timestamp_ns;
    
    MarketData() : bid(0.0), ask(0.0), timestamp_ns(0) {
        instrument.fill('\0');
    }
    
    void set_instrument(std::string_view inst) {
        size_t len = std::min(inst.size(), INSTRUMENT_SIZE - 1);
        std::copy_n(inst.begin(), len, instrument.begin());
        instrument[len] = '\0';
    }
    
    std::string_view get_instrument() const {
        return std::string_view(instrument.data());
    }
};

static_assert(sizeof(MarketData) % 64 == 0, "MarketData must be cache-aligned");

} // namespace market
