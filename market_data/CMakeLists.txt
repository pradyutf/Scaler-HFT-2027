cmake_minimum_required(VERSION 3.15)
project(MarketDataSystem CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# Find required packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

# Publisher executable
add_executable(publisher
    publisher.cpp
)

target_link_libraries(publisher
    ${Boost_LIBRARIES}
    fmt::fmt
    Threads::Threads
    rt  # Required for shm_open on Linux
)

# Shared Memory Consumer executable
add_executable(consumer_shm
    consumer_shm.cpp
)

target_link_libraries(consumer_shm
    fmt::fmt
    Threads::Threads
    rt
)

# TCP Consumer executable
add_executable(consumer_tcp
    consumer_tcp.cpp
)

target_link_libraries(consumer_tcp
    ${Boost_LIBRARIES}
    fmt::fmt
    Threads::Threads
)

# Compiler warnings
target_compile_options(publisher PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(consumer_shm PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(consumer_tcp PRIVATE -Wall -Wextra -Wpedantic)

# Optional: Enable AddressSanitizer for debugging
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    target_compile_options(publisher PRIVATE -fsanitize=address)
    target_link_options(publisher PRIVATE -fsanitize=address)
    target_compile_options(consumer_shm PRIVATE -fsanitize=address)
    target_link_options(consumer_shm PRIVATE -fsanitize=address)
    target_compile_options(consumer_tcp PRIVATE -fsanitize=address)
    target_link_options(consumer_tcp PRIVATE -fsanitize=address)
endif()

# Installation
install(TARGETS publisher consumer_shm consumer_tcp
    RUNTIME DESTINATION bin
)
