#pragma once

#include "market_data.hpp"
#include <array>
#include <cstdio>
#include <string>

namespace market {

// Fast, minimal JSON serialization/deserialization
// Avoids heavy dependencies while maintaining performance
class JsonUtils {
public:
    // Serialize MarketData to JSON string
    static std::string serialize(const MarketData& data) {
        char buffer[256];
        int len = std::snprintf(
            buffer, sizeof(buffer),
            R"({"instrument":"%s","bid":%.2f,"ask":%.2f,"timestamp_ns":%llu})",
            data.get_instrument().data(),
            data.bid,
            data.ask,
            (unsigned long long)data.timestamp_ns
        );
        return std::string(buffer, len);
    }
    
    // Serialize to fixed-size buffer (for TCP - more efficient)
    static size_t serialize_to_buffer(const MarketData& data, char* buffer, size_t size) {
        int len = std::snprintf(
            buffer, size,
            R"({"instrument":"%s","bid":%.2f,"ask":%.2f,"timestamp_ns":%llu})",
            data.get_instrument().data(),
            data.bid,
            data.ask,
            (unsigned long long)data.timestamp_ns
        );
        return static_cast<size_t>(len);
    }
    
    // Parse JSON string to MarketData (basic parser for client)
    static bool deserialize(const std::string& json, MarketData& data) {
        char instrument[32];
        double bid, ask;
        unsigned long long timestamp_ns;
        
        int matched = std::sscanf(
            json.c_str(),
            R"({"instrument":"%31[^"]","bid":%lf,"ask":%lf,"timestamp_ns":%llu})",
            instrument,
            &bid,
            &ask,
            &timestamp_ns
        );
        
        if (matched == 4) {
            data.set_instrument(instrument);
            data.bid = bid;
            data.ask = ask;
            data.timestamp_ns = static_cast<uint64_t>(timestamp_ns);
            return true;
        }
        return false;
    }
};

} // namespace market
