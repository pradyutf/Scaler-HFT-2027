#pragma once

#include "ring_buffer.hpp"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <string>

namespace market {

constexpr size_t RING_BUFFER_CAPACITY = 4096; // Must be power of 2 for optimal performance
constexpr const char* SHM_NAME = "/market_data_shm";

// Shared Memory Manager for Ring Buffer
class ShmManager {
public:
    using RingBufferType = RingBuffer<RING_BUFFER_CAPACITY>;
    
    // Create shared memory (for Publisher)
    static RingBufferType* create() {
        // Remove any existing shared memory
        shm_unlink(SHM_NAME);
        
        int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        if (shm_fd == -1) {
            throw std::runtime_error(std::string("shm_open failed: ") + strerror(errno));
        }
        
        // Set size
        if (ftruncate(shm_fd, sizeof(RingBufferType)) == -1) {
            close(shm_fd);
            throw std::runtime_error(std::string("ftruncate failed: ") + strerror(errno));
        }
        
        // Map memory
        void* ptr = mmap(nullptr, sizeof(RingBufferType), 
                        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        
        if (ptr == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap failed: ") + strerror(errno));
        }
        
        // Placement new to initialize the ring buffer
        RingBufferType* ring_buffer = new (ptr) RingBufferType();
        return ring_buffer;
    }
    
    // Open existing shared memory (for Consumer)
    static RingBufferType* open() {
        int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) {
            throw std::runtime_error(std::string("shm_open failed: ") + strerror(errno));
        }
        
        void* ptr = mmap(nullptr, sizeof(RingBufferType),
                        PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        close(shm_fd);
        
        if (ptr == MAP_FAILED) {
            throw std::runtime_error(std::string("mmap failed: ") + strerror(errno));
        }
        
        return reinterpret_cast<RingBufferType*>(ptr);
    }
    
    // Cleanup shared memory
    static void cleanup(RingBufferType* ptr) {
        if (ptr) {
            munmap(ptr, sizeof(RingBufferType));
        }
    }
    
    // Unlink shared memory
    static void unlink() {
        shm_unlink(SHM_NAME);
    }
};

} // namespace market
