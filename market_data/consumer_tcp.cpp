#include "market_data.hpp"
#include "clock_utils.hpp"
#include "json_utils.hpp"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <boost/asio.hpp>
#include <atomic>
#include <csignal>
#include <string>
#include <chrono>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#endif

using boost::asio::ip::tcp;

namespace {
    std::atomic<bool> g_running{true};
    
    void signal_handler(int) {
        g_running.store(false, std::memory_order_release);
    }
}

// TCP Consumer
class TcpConsumer {
public:
    TcpConsumer(const std::string& host, uint16_t port)
        : io_context_(),
          socket_(io_context_) {
        
        tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        
        fmt::print("Connecting to {}:{}...\n", host, port);
        boost::asio::connect(socket_, endpoints);
        
        // Disable Nagle's algorithm for lower latency
        socket_.set_option(tcp::no_delay(true));
        
        fmt::print("[{}] Connected to TCP server\n", 
                   std::chrono::system_clock::now());
    }
    
    void run() {
        set_cpu_affinity(3); // Pin to CPU core 3 for consistent performance
        
        uint64_t msg_count = 0;
        uint64_t total_latency_ns = 0;
        uint64_t min_latency_ns = UINT64_MAX;
        uint64_t max_latency_ns = 0;
        
        auto last_stats = std::chrono::steady_clock::now();
        
        fmt::print("Starting consumption from TCP...\n");
        
        std::string buffer;
        buffer.reserve(4096);
        
        boost::asio::streambuf streambuf;
        
        while (g_running.load(std::memory_order_acquire)) {
            try {
                // Read until newline delimiter
                boost::asio::read_until(socket_, streambuf, '\n');
                
                // Extract the line
                std::istream is(&streambuf);
                std::string line;
                std::getline(is, line);
                
                if (line.empty()) continue;
                
                // Parse JSON
                market::MarketData data;
                if (market::JsonUtils::deserialize(line, data)) {
                    uint64_t now_ns = market::Clock::now_ns();
                    uint64_t latency_ns = now_ns - data.timestamp_ns;
                    
                    // Update statistics
                    ++msg_count;
                    total_latency_ns += latency_ns;
                    min_latency_ns = std::min(min_latency_ns, latency_ns);
                    max_latency_ns = std::max(max_latency_ns, latency_ns);
                    
                    // Log every 10000th message
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
                }
            } catch (const boost::system::system_error& e) {
                if (e.code() == boost::asio::error::eof) {
                    fmt::print("Server closed connection\n");
                    break;
                } else if (e.code() != boost::asio::error::operation_aborted) {
                    fmt::print(stderr, "Error reading from socket: {}\n", e.what());
                    break;
                }
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
    
    boost::asio::io_context io_context_;
    tcp::socket socket_;
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    uint16_t port = 9090;
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = static_cast<uint16_t>(std::atoi(argv[2]));
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        TcpConsumer consumer(host, port);
        fmt::print("TCP Consumer started. Press Ctrl+C to stop.\n");
        consumer.run();
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    
    return 0;
}
