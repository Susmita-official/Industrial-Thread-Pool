cmake_minimum_required(VERSION 3.15)
project(ThreadPoolProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate the system threads library
find_package(Threads REQUIRED)

# Include header paths
include_directories(include)

# Define the final runnable application name and its exact dependencies
add_executable(ThreadPoolApp 
    main.cpp 
    src/ThreadPool.cpp
)

# Link multi-threading processing components
target_link_libraries(ThreadPoolApp PRIVATE Threads::Threads)

# 💡 FIX: Force-copy config.json directly into the execution output folder
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
    "${CMAKE_CURRENT_BINARY_DIR}/config.json"
    COPYONLY
)
