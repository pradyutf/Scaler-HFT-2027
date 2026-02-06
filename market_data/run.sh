#!/bin/bash

echo "=================================="
echo "Market Data System - Quick Start"
echo "=================================="

cd "$(dirname "$0")"

# Check if build exists
if [ ! -d "build" ] || [ ! -f "build/publisher" ]; then
    echo "Building project..."
    ./build.sh
    if [ $? -ne 0 ]; then
        echo "Build failed. Please check errors above."
        exit 1
    fi
fi

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -P $$ 2>/dev/null
    sleep 1
    # Clean shared memory
    rm -f /dev/shm/market_data_shm 2>/dev/null || true
    exit 0
}

trap cleanup SIGINT SIGTERM

cd build

echo ""
echo "Starting Market Data System..."
echo "Press Ctrl+C to stop all processes"
echo ""

# Clean any existing shared memory
rm -f /dev/shm/market_data_shm 2>/dev/null || true

# Start publisher in background
echo "[1/3] Starting Publisher..."
./publisher > publisher.log 2>&1 &
PUBLISHER_PID=$!
sleep 2

# Start SHM consumer in background
echo "[2/3] Starting SHM Consumer..."
./consumer_shm > consumer_shm.log 2>&1 &
CONSUMER_SHM_PID=$!
sleep 1

# Start TCP consumer in background
echo "[3/3] Starting TCP Consumer..."
./consumer_tcp > consumer_tcp.log 2>&1 &
CONSUMER_TCP_PID=$!

echo ""
echo "All processes started!"
echo "  Publisher PID: $PUBLISHER_PID"
echo "  SHM Consumer PID: $CONSUMER_SHM_PID"
echo "  TCP Consumer PID: $CONSUMER_TCP_PID"
echo ""
echo "Monitoring logs (Ctrl+C to stop)..."
echo "=========================================="

# Monitor logs
tail -f publisher.log consumer_shm.log consumer_tcp.log &

# Wait for user interrupt
wait
