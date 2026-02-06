#include "market_data.hpp"
#include "ring_buffer.hpp"
#include "shm_manager.hpp"
#include "clock_utils.hpp"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#endif

namespace {
    std::atomic<bool> g_running{true};
    
    void signal_handler(int) {
        g_running.store(false, std::memory_order_release);
    }
}

// Shared Memory Consumer
class ShmConsumer {
public:
    ShmConsumer() {
        // Wait for publisher to create shared memory
        fmt::print("Waiting for shared memory...\n");
        
        for (int i = 0; i < 10; ++i) {
            try {
                ring_buffer_ = market::ShmManager::open();
                fmt::print("[{}] Connected to shared memory\n", 
                           std::chrono::system_clock::now());
                return;
            } catch (const std::exception& e) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        throw std::runtime_error("Failed to connect to shared memory");
    }
    
    ~ShmConsumer() {
        market::ShmManager::cleanup(ring_buffer_);
    }
    
    void run() {
        set_cpu_affinity(2); // Pin to CPU core 2 for consistent performance
        
        uint64_t msg_count = 0;
        uint64_t total_latency_ns = 0;
        uint64_t min_latency_ns = UINT64_MAX;
        uint64_t max_latency_ns = 0;
        
        auto last_stats = std::chrono::steady_clock::now();
        
        fmt::print("Starting consumption from shared memory...\n");
        
        while (g_running.load(std::memory_order_acquire)) {
            market::MarketData data;
            
            if (ring_buffer_->try_pop(data)) {
                uint64_t now_ns = market::Clock::now_ns();
                uint64_t latency_ns = now_ns - data.timestamp_ns;
                
                // Update statistics
                ++msg_count;
                total_latency_ns += latency_ns;
                min_latency_ns = std::min(min_latency_ns, latency_ns);
                max_latency_ns = std::max(max_latency_ns, latency_ns);
                
                // Log every 10000th message to avoid overwhelming output
                if (msg_count % 10000 == 0) {
                    fmt::print("[{}] {} BID={:.2f} ASK={:.2f} LATENCY={}ns\n",
                               std::chrono::system_clock::now(),
                               data.get_instrument(),
                               data.bid,
                               data.ask,
                               latency_ns);
                }
                
                // Print stats every second
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 1) {
                    uint64_t avg_latency = msg_count > 0 ? (total_latency_ns / msg_count) : 0;
                    fmt::print("[{}] Received: {} msgs | Avg latency: {}ns | Min: {}ns | Max: {}ns\n",
                               std::chrono::system_clock::now(),
                               msg_count, avg_latency, min_latency_ns, max_latency_ns);
                    last_stats = now;
                }
            } else {
                // Small yield to avoid busy-waiting when buffer is empty
                std::this_thread::yield();
            }
        }
        
        // Final statistics
        if (msg_count > 0) {
            uint64_t avg_latency = total_latency_ns / msg_count;
            fmt::print("\n=== Final Statistics ===\n");
            fmt::print("Total messages: {}\n", msg_count);
            fmt::print("Average latency: {}ns ({:.3f}μs)\n", avg_latency, avg_latency / 1000.0);
            fmt::print("Min latency: {}ns ({:.3f}μs)\n", min_latency_ns, min_latency_ns / 1000.0);
            fmt::print("Max latency: {}ns ({:.3f}μs)\n", max_latency_ns, max_latency_ns / 1000.0);
        }
    }
    
private:
    void set_cpu_affinity(int cpu_id) {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        
        pthread_t thread = pthread_self();
        int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        
        if (result == 0) {
            fmt::print("CPU affinity set to core {}\n", cpu_id);
        } else {
            fmt::print("Warning: Failed to set CPU affinity\n");
        }
#else
        // CPU affinity not supported on macOS
        (void)cpu_id;
#endif
    }
    
    market::RingBuffer<market::RING_BUFFER_CAPACITY>* ring_buffer_;
};

int main() {
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        ShmConsumer consumer;
        fmt::print("Shared Memory Consumer started. Press Ctrl+C to stop.\n");
        consumer.run();
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    
    return 0;
}
