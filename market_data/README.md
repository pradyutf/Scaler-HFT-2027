# Low-Latency Market Data Publishing System

A high-performance market data distribution system in C++17 demonstrating:
- TCP loopback networking with Boost.Asio
- Lock-free shared memory (mmap) with SPSC ring buffer
- Sub-microsecond latency measurements
- Cache-aligned data structures to prevent false sharing

## Architecture

The system consists of three independent processes:

1. **Publisher (Process A)**: Generates market data and publishes via TCP and shared memory
2. **SHM Consumer (Process B)**: Reads from shared memory with lock-free ring buffer
3. **TCP Consumer (Process C)**: Reads from TCP loopback socket

## Features

### Performance Optimizations
- **Lock-free SPSC ring buffer** with proper memory ordering (`std::memory_order_acquire/release`)
- **Cache-line alignment** (64 bytes) to avoid false sharing
- **TCP_NODELAY** enabled to disable Nagle's algorithm
- **CPU affinity** support (Linux only)
- **Nanosecond timestamps** using `std::chrono::steady_clock`
- **Zero-copy** shared memory via `mmap`

### Technical Highlights
- C++17 standard
- Boost.Asio for async TCP networking
- fmt library for fast, type-safe logging
- POSIX shared memory (`shm_open`, `mmap`)
- Atomic operations with memory barriers
- Thread-safe design

## Build Instructions

### Prerequisites
```bash
# macOS
brew install boost fmt cmake

# Ubuntu/Debian
sudo apt-get install libboost-all-dev libfmt-dev cmake build-essential

# CentOS/RHEL
sudo yum install boost-devel fmt-devel cmake gcc-c++
```

### Compilation
```bash
cd market_data
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Debug Build (with AddressSanitizer)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..
make -j$(nproc)
```

## Running the System

### Start Publisher (Process A)
```bash
./publisher [port]
# Default port: 9090
```

### Start Shared Memory Consumer (Process B)
```bash
./consumer_shm
```

### Start TCP Consumer (Process C)
```bash
./consumer_tcp [host] [port]
# Default: 127.0.0.1 9090
```

### Example Session
```bash
# Terminal 1
./publisher

# Terminal 2
./consumer_shm

# Terminal 3
./consumer_tcp
```

## Message Format

```json
{
  "instrument": "RELIANCE",
  "bid": 2850.25,
  "ask": 2850.75,
  "timestamp_ns": 1234567890123
}
```

## Performance Characteristics

### Shared Memory (Expected)
- **Latency**: 200-500ns (sub-microsecond)
- **Throughput**: 1M+ msgs/sec
- **Zero drops** under normal load

### TCP Loopback (Expected)
- **Latency**: 5-20μs
- **Throughput**: 100K+ msgs/sec
- **Reliable** delivery

## Code Structure

```
market_data/
├── market_data.hpp       # Core data structure (cache-aligned)
├── ring_buffer.hpp       # Lock-free SPSC ring buffer
├── shm_manager.hpp       # Shared memory management
├── clock_utils.hpp       # High-resolution timing
├── json_utils.hpp        # Fast JSON serialization
├── publisher.cpp         # Process A (publisher)
├── consumer_shm.cpp      # Process B (SHM consumer)
├── consumer_tcp.cpp      # Process C (TCP consumer)
├── CMakeLists.txt        # Build configuration
└── README.md             # This file
```

## Design Decisions

### Lock-Free Ring Buffer
- Uses `std::atomic` with acquire/release memory ordering
- Cache-line padding (64 bytes) on read/write indices
- Prevents false sharing across CPU cores
- Single producer, single consumer (SPSC) pattern

### Memory Layout
```cpp
struct RingBuffer {
    alignas(64) atomic<size_t> write_idx;  // Producer cache line
    alignas(64) atomic<size_t> read_idx;   // Consumer cache line
    MarketData buffer[N];                  // Data buffer
};
```

### TCP Optimizations
- `TCP_NODELAY` disables Nagle's algorithm
- Non-blocking async I/O with Boost.Asio
- Fixed-size buffers to reduce allocations
- Newline-delimited JSON for simple framing

### CPU Affinity (Linux)
- Publisher: no pinning (handles multiple tasks)
- SHM Consumer: pinned to core 2
- TCP Consumer: pinned to core 3

## Testing

### Latency Test
```bash
# Run for 60 seconds and measure latencies
timeout 60 ./consumer_shm
```

### Stress Test
```bash
# Monitor for drops under high load
watch -n 1 'cat /dev/shm/market_data_shm | wc -c'
```

## Platform Compatibility

- **Linux**: Full support (CPU affinity, real-time priority)
- **macOS**: Supported (no CPU affinity)
- **Windows**: Not tested (requires WSL)

## Troubleshooting

### Shared Memory Issues
```bash
# List shared memory
ls -lh /dev/shm/

# Remove stale shared memory
rm /dev/shm/market_data_shm

# Check shared memory limits (Linux)
cat /proc/sys/kernel/shmmax
```

### Permission Denied
```bash
# Ensure proper permissions
chmod 666 /dev/shm/market_data_shm
```

## Benchmarking

Performance will vary based on:
- CPU architecture (Intel vs AMD)
- CPU frequency and turbo boost
- NUMA configuration
- System load and other processes
- Compiler optimization level

## Future Enhancements

- Multiple instrument support with separate buffers
- Binary protocol (instead of JSON) for TCP
- Market depth (Level 2 order book data)
- Multicast UDP for fan-out
- Performance monitoring dashboard
- Kernel bypass (DPDK) for ultra-low latency
