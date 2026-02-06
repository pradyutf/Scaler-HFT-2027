#!/bin/bash

echo "=================================="
echo "Dependency Check Script"
echo "=================================="

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

all_ok=true

# Check C++ compiler
echo -n "Checking for C++ compiler... "
if command -v g++ &> /dev/null; then
    version=$(g++ --version | head -n1)
    echo -e "${GREEN}✓${NC} Found: $version"
elif command -v clang++ &> /dev/null; then
    version=$(clang++ --version | head -n1)
    echo -e "${GREEN}✓${NC} Found: $version"
else
    echo -e "${RED}✗${NC} Not found"
    echo "  Install: xcode-select --install (macOS) or apt-get install build-essential (Linux)"
    all_ok=false
fi

# Check CMake
echo -n "Checking for CMake... "
if command -v cmake &> /dev/null; then
    version=$(cmake --version | head -n1)
    echo -e "${GREEN}✓${NC} Found: $version"
else
    echo -e "${YELLOW}⚠${NC} Not found (optional, can use Make instead)"
    echo "  Install: brew install cmake (macOS) or apt-get install cmake (Linux)"
fi

# Check Make
echo -n "Checking for Make... "
if command -v make &> /dev/null; then
    version=$(make --version | head -n1)
    echo -e "${GREEN}✓${NC} Found: $version"
else
    echo -e "${RED}✗${NC} Not found"
    all_ok=false
fi

# Check Boost
echo -n "Checking for Boost... "
boost_found=false
for dir in /usr/include /usr/local/include /opt/homebrew/include; do
    if [ -d "$dir/boost" ]; then
        echo -e "${GREEN}✓${NC} Found in $dir"
        boost_found=true
        break
    fi
done
if [ "$boost_found" = false ]; then
    echo -e "${RED}✗${NC} Not found"
    echo "  Install: brew install boost (macOS) or apt-get install libboost-all-dev (Linux)"
    all_ok=false
fi

# Check fmt
echo -n "Checking for fmt library... "
fmt_found=false

# Try pkg-config first
if pkg-config --exists fmt 2>/dev/null; then
    version=$(pkg-config --modversion fmt 2>/dev/null)
    echo -e "${GREEN}✓${NC} Found: version $version"
    fmt_found=true
else
    # Check common library locations
    for dir in /usr/lib /usr/local/lib /opt/homebrew/lib; do
        if [ -f "$dir/libfmt.a" ] || [ -f "$dir/libfmt.dylib" ] || [ -f "$dir/libfmt.so" ]; then
            echo -e "${GREEN}✓${NC} Found in $dir"
            fmt_found=true
            break
        fi
    done
fi

if [ "$fmt_found" = false ]; then
    echo -e "${RED}✗${NC} Not found"
    echo "  Install: brew install fmt (macOS) or apt-get install libfmt-dev (Linux)"
    all_ok=false
fi

# Check pthread
echo -n "Checking for pthread... "
if [ -f "/usr/lib/libpthread.so" ] || [ -f "/usr/lib/libpthread.a" ] || [ -f "/usr/lib/x86_64-linux-gnu/libpthread.a" ] || [ -f "/usr/lib/aarch64-linux-gnu/libpthread.a" ]; then
    echo -e "${GREEN}✓${NC} Found"
elif [ "$(uname)" = "Darwin" ]; then
    echo -e "${GREEN}✓${NC} Built-in (macOS)"
else
    echo -e "${YELLOW}⚠${NC} Possibly missing"
fi

echo ""
echo "=================================="
if [ "$all_ok" = true ]; then
    echo -e "${GREEN}All required dependencies found!${NC}"
    echo "You can now run: ./build.sh"
else
    echo -e "${RED}Some dependencies are missing.${NC}"
    echo "Please install missing dependencies and try again."
    exit 1
fi
