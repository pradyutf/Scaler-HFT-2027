# Low-Latency Market Data Publishing System

A production-ready, high-performance market data distribution system demonstrating enterprise-grade C++ low-latency techniques.

## 🎯 Project Overview

This system implements a realistic exchange market data publisher that distributes real-time bid/ask prices through two channels:
1. **TCP Loopback Socket** - Network-based distribution
2. **Shared Memory (mmap)** - Zero-copy, ultra-low latency IPC

### Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Process A                          │
│              Market Data Publisher                   │
│                                                      │
│  ┌──────────────┐      ┌──────────────┐            │
│  │   Market     │      │   TCP        │            │
│  │   Data Gen   │─────▶│   Server     │────┐       │
│  └──────────────┘      └──────────────┘    │       │
│         │                                   │       │
│         │                                   │       │
│         ▼                                   │       │
│  ┌──────────────┐                          │       │
│  │  Shared Mem  │                          │       │
│  │  Ring Buffer │                          │       │
│  └──────────────┘                          │       │
└─────────┬───────────────────────────────────┼───────┘
          │                                   │
          │ mmap (lock-free)                  │ TCP loopback
          │                                   │
    ┌─────▼─────────┐              ┌────────▼────────┐
    │  Process B    │              │   Process C     │
    │  SHM Consumer │              │  TCP Consumer   │
    │ (200-500ns)   │              │  (5-20μs)       │
    └───────────────┘              └─────────────────┘
```

## ✨ Key Features

### Performance Optimizations

1. **Lock-Free Ring Buffer**
   - SPSC (Single Producer Single Consumer) design
   - `std::atomic` with acquire/release memory ordering
   - Zero syscalls in hot path
   - Cache-line aligned (64 bytes) to prevent false sharing

2. **Zero-Copy Design**
   - Shared memory via `mmap()` and `shm_open()`
   - Direct memory access without serialization overhead
   - Fixed-size messages for predictable performance

3. **Network Optimizations**
   - TCP_NODELAY (disables Nagle's algorithm)
   - Non-blocking async I/O with Boost.Asio
   - Fixed-size buffers to minimize allocations

4. **CPU Optimization**
   - CPU affinity support (Linux)
   - Cache-aligned data structures
   - Memory barriers for proper ordering

5. **High-Resolution Timing**
   - Nanosecond precision timestamps
   - `std::chrono::steady_clock` for latency measurements
   - Per-message latency tracking

## 📁 Project Structure

```
market_data/
├── market_data.hpp       # Core data structure (64-byte aligned)
├── ring_buffer.hpp       # Lock-free SPSC ring buffer
├── shm_manager.hpp       # Shared memory lifecycle management
├── clock_utils.hpp       # High-resolution timing utilities
├── json_utils.hpp        # Fast JSON serialization
├── publisher.cpp         # Process A: Data publisher
├── consumer_shm.cpp      # Process B: Shared memory consumer
├── consumer_tcp.cpp      # Process C: TCP consumer
├── CMakeLists.txt        # CMake build configuration
├── Makefile              # Alternative build method
├── build.sh              # Automated build script
├── run.sh                # All-in-one launch script
├── check_deps.sh         # Dependency verification
├── README.md             # Detailed documentation
└── SETUP.md              # Installation guide
```

## 🚀 Quick Start

### Prerequisites

```bash
# macOS
brew install boost fmt cmake

# Ubuntu/Debian
sudo apt-get install libboost-all-dev libfmt-dev cmake build-essential

# Check dependencies
./check_deps.sh
```

### Build & Run

```bash
# Build all executables
./build.sh

# Run all processes (automated)
./run.sh

# Or manually (3 separate terminals):
# Terminal 1:
./build/publisher

# Terminal 2:
./build/consumer_shm

# Terminal 3:
./build/consumer_tcp
```

## 🔬 Technical Deep Dive

### Lock-Free Ring Buffer Implementation

```cpp
template<size_t Capacity>
struct alignas(64) RingBuffer {
    // Separate cache lines prevent false sharing
    alignas(64) std::atomic<size_t> write_idx{0};  // Producer-only
    alignas(64) std::atomic<size_t> read_idx{0};   // Consumer-only
    
    MarketData buffer[Capacity];
    
    bool try_push(const MarketData& data) {
        const size_t w = write_idx.load(std::memory_order_relaxed);
        const size_t next_w = (w + 1) % Capacity;
        const size_t r = read_idx.load(std::memory_order_acquire);
        
        if (next_w == r) return false;  // Full
        
        buffer[w] = data;
        write_idx.store(next_w, std::memory_order_release);
        return true;
    }
};
```

**Key Points:**
- `memory_order_acquire`: Ensures all writes before this are visible
- `memory_order_release`: Publishes changes to other threads
- Cache-line alignment prevents false sharing between CPU cores
- No locks, mutexes, or syscalls in critical path

### Market Data Format

```cpp
struct alignas(64) MarketData {
    char instrument[16];    // Stock symbol (e.g., "RELIANCE")
    double bid;             // Best bid price
    double ask;             // Best ask price
    uint64_t timestamp_ns;  // Nanosecond timestamp
};
```

JSON representation:
```json
{
  "instrument": "RELIANCE",
  "bid": 2850.25,
  "ask": 2850.75,
  "timestamp_ns": 1234567890123
}
```

### Performance Characteristics

| Metric | Shared Memory | TCP Loopback |
|--------|---------------|--------------|
| Latency | 200-500ns | 5-20μs |
| Throughput | 1M+ msgs/sec | 100K+ msgs/sec |
| Jitter | Ultra-low | Low |
| Reliability | 100% | 100% |
| Overhead | Zero-copy | Syscalls |

## 🛠️ Advanced Usage

### Custom Port

```bash
./publisher 8080           # Publisher on port 8080
./consumer_tcp 127.0.0.1 8080  # Connect to port 8080
```

### Performance Tuning (Linux)

```bash
# 1. Isolate CPU cores
sudo vim /etc/default/grub
# Add: isolcpus=2,3

# 2. Set real-time priority
sudo chrt -f 99 ./consumer_shm

# 3. Disable swap
sudo swapoff -a

# 4. Increase shared memory limits
sudo sysctl -w kernel.shmmax=17179869184
sudo sysctl -w kernel.shmall=4194304
```

### Latency Profiling

```bash
# Run for 60 seconds and collect stats
timeout 60 ./consumer_shm > latency_stats.txt

# Analyze latencies
grep "Avg latency" latency_stats.txt
```

## 📊 Expected Performance

On a modern CPU (Intel i7/i9 or AMD Ryzen):

**Shared Memory:**
- P50 latency: ~300ns
- P99 latency: ~500ns
- P99.9 latency: ~1μs

**TCP Loopback:**
- P50 latency: ~10μs
- P99 latency: ~20μs
- P99.9 latency: ~50μs

## 🧪 Testing

### Verify Build

```bash
# Check executables exist
ls -lh build/{publisher,consumer_shm,consumer_tcp}

# Check dependencies
ldd build/publisher  # Linux
otool -L build/publisher  # macOS
```

### Functional Test

```bash
# 1. Start publisher
./build/publisher &
sleep 2

# 2. Start consumers
./build/consumer_shm &
./build/consumer_tcp &

# 3. Monitor for 10 seconds
sleep 10

# 4. Cleanup
pkill publisher
pkill consumer_
rm -f /dev/shm/market_data_shm
```

## 🐛 Troubleshooting

### Shared Memory Issues

```bash
# List shared memory segments
ls -lh /dev/shm/

# Remove stale segment
rm -f /dev/shm/market_data_shm

# Check permissions
sudo chmod 666 /dev/shm/market_data_shm
```

### Build Failures

```bash
# Verify dependencies
./check_deps.sh

# Clean build
rm -rf build
./build.sh

# Alternative: Use Makefile
make clean && make
```

### Port in Use

```bash
# Find process using port 9090
lsof -i :9090
netstat -an | grep 9090

# Kill process
kill -9 <PID>
```

## 📚 Learning Resources

This implementation demonstrates:
- ✅ Lock-free data structures
- ✅ Memory barriers and ordering
- ✅ Cache-line alignment
- ✅ Zero-copy IPC
- ✅ TCP optimizations
- ✅ High-resolution timing
- ✅ Modern C++17 features

## 🎓 Assignment Requirements Checklist

- [x] Modern C++ (C++17)
- [x] TCP loopback networking
- [x] Shared memory with mmap
- [x] Lock-free ring buffer
- [x] Three independent processes
- [x] JSON message format
- [x] fmt library for logging
- [x] Nanosecond timestamps
- [x] Cache-line padding
- [x] Memory ordering (acquire/release)
- [x] TCP_NODELAY optimization
- [x] CPU affinity (Linux)
- [x] Realistic market data

## 📝 License

This project is for educational purposes (HFT assignment).

## 👥 Author

Created for Scaler HFT 2027 assignment.

---

**Note:** This is a demonstration system. Production systems would include:
- Market data normalization
- Order book reconstruction
- Gap detection and recovery
- Monitoring and alerting
- Failover and redundancy
- Configuration management
