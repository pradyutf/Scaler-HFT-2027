# Market Data Publishing System - Setup Guide

## Prerequisites

This project requires:
- C++17 compatible compiler (g++ 7+ or clang++ 5+)
- Boost libraries (1.65+)
- fmt library (6.0+)
- CMake 3.15+ (recommended) or Make
- POSIX-compliant OS (Linux/macOS)

## Installation

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake boost fmt

# Verify installation
cmake --version
g++ --version
```

### Ubuntu/Debian Linux

```bash
# Update package list
sudo apt-get update

# Install build tools
sudo apt-get install -y build-essential cmake

# Install dependencies
sudo apt-get install -y libboost-all-dev libfmt-dev

# Verify installation
cmake --version
g++ --version
```

### CentOS/RHEL Linux

```bash
# Install EPEL repository
sudo yum install -y epel-release

# Install build tools
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake3

# Install dependencies
sudo yum install -y boost-devel fmt-devel

# Use cmake3 instead of cmake
alias cmake=cmake3
```

## Building

### Method 1: CMake (Recommended)

```bash
cd market_data
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Method 2: Build Script

```bash
cd market_data
./build.sh
```

### Method 3: Direct Makefile

```bash
cd market_data
make -j$(nproc)
```

## Running

### Option 1: Manual Start (3 terminals)

Terminal 1 - Publisher:
```bash
cd market_data/build
./publisher
```

Terminal 2 - Shared Memory Consumer:
```bash
cd market_data/build
./consumer_shm
```

Terminal 3 - TCP Consumer:
```bash
cd market_data/build
./consumer_tcp
```

### Option 2: Quick Start Script

```bash
cd market_data
./run.sh
```

This script starts all three processes and monitors their logs.

## Troubleshooting

### "cmake: command not found"

Install CMake:
```bash
# macOS
brew install cmake

# Linux
sudo apt-get install cmake
```

### "fatal error: boost/asio.hpp: No such file or directory"

Install Boost:
```bash
# macOS
brew install boost

# Linux
sudo apt-get install libboost-all-dev
```

### "fatal error: fmt/core.h: No such file or directory"

Install fmt library:
```bash
# macOS
brew install fmt

# Linux
sudo apt-get install libfmt-dev
```

### Shared Memory Permission Issues (Linux)

```bash
# Check shared memory mount
df -h | grep shm

# Clean stale shared memory
rm -f /dev/shm/market_data_shm

# Check limits
cat /proc/sys/kernel/shmmax
cat /proc/sys/kernel/shmall
```

### Port Already in Use

```bash
# Find process using port 9090
lsof -i :9090

# Kill the process
kill -9 <PID>
```

## System Requirements

### Minimum
- 2 CPU cores
- 2 GB RAM
- 100 MB disk space

### Recommended
- 4+ CPU cores for optimal performance
- 4+ GB RAM
- SSD for faster compilation

## Performance Tips

1. **Build with optimizations**: Always use `Release` build type
2. **CPU affinity**: On Linux, processes automatically pin to specific cores
3. **Disable swap**: For consistent latency, disable swap or use `swapoff -a`
4. **Isolate CPUs**: Use kernel parameter `isolcpus` for dedicated cores
5. **Real-time priority**: Run with `chrt -f 99` for real-time scheduling (requires root)

## Verification

After building, verify executables exist:
```bash
ls -lh build/publisher build/consumer_shm build/consumer_tcp
```

Check runtime dependencies:
```bash
ldd build/publisher
ldd build/consumer_shm
ldd build/consumer_tcp
```

## Next Steps

1. Review `README.md` for architecture details
2. Examine source code for implementation specifics
3. Run benchmarks to measure latency
4. Experiment with different configurations

## Support

For issues:
1. Check compiler version: `g++ --version` or `clang++ --version`
2. Check Boost version: `dpkg -l | grep libboost` (Linux) or `brew info boost` (macOS)
3. Review build logs for specific errors
4. Ensure all dependencies are installed
