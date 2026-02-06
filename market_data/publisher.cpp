#include "market_data.hpp"
#include "ring_buffer.hpp"
#include "shm_manager.hpp"
#include "clock_utils.hpp"
#include "json_utils.hpp"
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <boost/asio.hpp>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <memory>

using boost::asio::ip::tcp;

namespace {
    std::atomic<bool> g_running{true};
    
    void signal_handler(int) {
        g_running.store(false, std::memory_order_release);
    }
}

// TCP Session for handling client connections
class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    explicit TcpSession(tcp::socket socket) 
        : socket_(std::move(socket)) {
        // Disable Nagle's algorithm for lower latency
        socket_.set_option(tcp::no_delay(true));
    }
    
    void start() {
        active_ = true;
    }
    
    bool send_data(const char* data, size_t length) {
        if (!active_) return false;
        
        try {
            boost::asio::write(socket_, boost::asio::buffer(data, length));
            return true;
        } catch (const std::exception&) {
            active_ = false;
            return false;
        }
    }
    
    bool is_active() const { return active_; }
    
private:
    tcp::socket socket_;
    std::atomic<bool> active_{false};
};

// Market Data Publisher
class Publisher {
public:
    Publisher(uint16_t port)
        : io_context_(),
          acceptor_(io_context_, tcp::endpoint(tcp::v4(), port)),
          ring_buffer_(market::ShmManager::create()) {
        
        fmt::print("[{}] Publisher initialized on port {}\n", 
                   std::chrono::system_clock::now(), port);
        fmt::print("[{}] Shared memory ring buffer created\n", 
                   std::chrono::system_clock::now());
        
        start_accept();
    }
    
    ~Publisher() {
        market::ShmManager::cleanup(ring_buffer_);
        market::ShmManager::unlink();
    }
    
    void run() {
        // Start accepting connections in a separate thread
        std::thread accept_thread([this]() {
            io_context_.run();
        });
        
        // Market data generation thread
        std::thread generator_thread([this]() {
            generate_market_data();
        });
        
        accept_thread.join();
        generator_thread.join();
        
        fmt::print("[{}] Publisher shutting down\n", 
                   std::chrono::system_clock::now());
    }
    
private:
    void start_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    auto session = std::make_shared<TcpSession>(std::move(socket));
                    session->start();
                    
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.push_back(session);
                    
                    fmt::print("[{}] New TCP client connected (total: {})\n",
                               std::chrono::system_clock::now(), sessions_.size());
                }
                
                if (g_running.load(std::memory_order_acquire)) {
                    start_accept();
                }
            }
        );
    }
    
    void generate_market_data() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(2800.0, 2900.0);
        std::uniform_real_distribution<> spread_dist(0.25, 1.0);
        
        const std::vector<std::string> instruments = {
            "RELIANCE", "TCS", "INFY", "HDFC", "ICICI"
        };
        
        char json_buffer[256];
        size_t msg_count = 0;
        uint64_t shm_drops = 0;
        uint64_t tcp_errors = 0;
        
        auto last_stats = std::chrono::steady_clock::now();
        
        while (g_running.load(std::memory_order_acquire)) {
            // Generate market data
            market::MarketData data;
            data.set_instrument(instruments[msg_count % instruments.size()]);
            data.bid = price_dist(gen);
            data.ask = data.bid + spread_dist(gen);
            data.timestamp_ns = market::Clock::now_ns();
            
            // Push to shared memory ring buffer
            if (!ring_buffer_->try_push(data)) {
                ++shm_drops;
            }
            
            // Serialize to JSON for TCP
            size_t json_len = market::JsonUtils::serialize_to_buffer(
                data, json_buffer, sizeof(json_buffer)
            );
            json_buffer[json_len++] = '\n'; // Add newline delimiter
            
            // Send to all TCP clients
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.erase(
                    std::remove_if(sessions_.begin(), sessions_.end(),
                        [&](auto& session) {
                            if (!session->is_active() || 
                                !session->send_data(json_buffer, json_len)) {
                                ++tcp_errors;
                                return true;
                            }
                            return false;
                        }
                    ),
                    sessions_.end()
                );
            }
            
            ++msg_count;
            
            // Print stats every second
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 1) {
                fmt::print("[{}] Sent: {} msgs | SHM drops: {} | Active clients: {}\n",
                           std::chrono::system_clock::now(),
                           msg_count, shm_drops, sessions_.size());
                last_stats = now;
            }
            
            // Throttle to simulate realistic market data rate (~10k msgs/sec)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        io_context_.stop();
    }
    
    boost::asio::io_context io_context_;
    tcp::acceptor acceptor_;
    market::RingBuffer<market::RING_BUFFER_CAPACITY>* ring_buffer_;
    std::vector<std::shared_ptr<TcpSession>> sessions_;
    std::mutex sessions_mutex_;
};

int main(int argc, char* argv[]) {
    uint16_t port = 9090;
    
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        Publisher publisher(port);
        fmt::print("Market Data Publisher started. Press Ctrl+C to stop.\n");
        publisher.run();
    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    
    return 0;
}
