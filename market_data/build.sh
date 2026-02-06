#!/bin/bash
set -e

echo "==================================="
echo "Market Data System - Build Script"
echo "==================================="

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check for required dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}ERROR: cmake not found. Please install cmake.${NC}"
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}ERROR: C++ compiler not found. Please install g++ or clang++.${NC}"
    exit 1
fi

# Check for Boost
if [ ! -d "/usr/local/include/boost" ] && [ ! -d "/usr/include/boost" ] && [ ! -d "/opt/homebrew/include/boost" ]; then
    echo -e "${RED}ERROR: Boost not found. Please install boost.${NC}"
    echo "  macOS: brew install boost"
    echo "  Linux: sudo apt-get install libboost-all-dev"
    exit 1
fi

# Check for fmt
if [ ! -f "/usr/local/lib/libfmt.a" ] && [ ! -f "/usr/lib/libfmt.a" ] && [ ! -f "/opt/homebrew/lib/libfmt.a" ] && ! pkg-config --exists fmt; then
    echo -e "${YELLOW}WARNING: fmt library not found, will try to find it during build...${NC}"
fi

echo -e "${GREEN}✓ Dependencies check passed${NC}"

# Navigate to project directory
cd "$(dirname "$0")"

# Clean previous build
if [ -d "build" ]; then
    echo -e "${YELLOW}Cleaning previous build...${NC}"
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Determine build type
BUILD_TYPE="${1:-Release}"
echo -e "${YELLOW}Building in ${BUILD_TYPE} mode...${NC}"

# Configure
echo -e "${YELLOW}Running CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..

# Build
echo -e "${YELLOW}Compiling...${NC}"
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
make -j${NPROC}

# Check build success
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}==================================="
    echo "✓ Build completed successfully!"
    echo "===================================${NC}"
    echo ""
    echo "Executables:"
    echo "  - $(pwd)/publisher"
    echo "  - $(pwd)/consumer_shm"
    echo "  - $(pwd)/consumer_tcp"
    echo ""
    echo "To run:"
    echo "  Terminal 1: ./build/publisher"
    echo "  Terminal 2: ./build/consumer_shm"
    echo "  Terminal 3: ./build/consumer_tcp"
    echo ""
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
