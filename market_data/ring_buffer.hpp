#pragma once

#include "market_data.hpp"
#include <atomic>
#include <cstddef>
#include <new>

namespace market {

// Single Producer Single Consumer (SPSC) Lock-Free Ring Buffer
// Optimized with cache-line padding to avoid false sharing
template<size_t Capacity>
struct alignas(64) RingBuffer {
    static constexpr size_t BUFFER_SIZE = Capacity;
    
    // Cache-line aligned atomics to prevent false sharing
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};
    
    // Message buffer
    MarketData buffer[BUFFER_SIZE];
    
    RingBuffer() = default;
    
    // Try to push a message (non-blocking)
    bool try_push(const MarketData& data) {
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) % BUFFER_SIZE;
        const size_t current_read = read_idx.load(std::memory_order_acquire);
        
        // Check if buffer is full
        if (next_write == current_read) {
            return false;
        }
        
        // Write data
        buffer[current_write] = data;
        
        // Update write index with release semantics
        write_idx.store(next_write, std::memory_order_release);
        return true;
    }
    
    // Try to pop a message (non-blocking)
    bool try_pop(MarketData& data) {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);
        const size_t current_write = write_idx.load(std::memory_order_acquire);
        
        // Check if buffer is empty
        if (current_read == current_write) {
            return false;
        }
        
        // Read data
        data = buffer[current_read];
        
        // Update read index with release semantics
        const size_t next_read = (current_read + 1) % BUFFER_SIZE;
        read_idx.store(next_read, std::memory_order_release);
        return true;
    }
    
    // Check if buffer is empty
    bool empty() const {
        return read_idx.load(std::memory_order_acquire) == 
               write_idx.load(std::memory_order_acquire);
    }
    
    // Get current size (approximate, for monitoring)
    size_t size() const {
        const size_t write = write_idx.load(std::memory_order_relaxed);
        const size_t read = read_idx.load(std::memory_order_relaxed);
        return (write >= read) ? (write - read) : (BUFFER_SIZE - read + write);
    }
};

} // namespace market
