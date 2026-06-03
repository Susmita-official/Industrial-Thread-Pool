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

# ==========================================
# Unit Testing Setup (GoogleTest Auto-Download)
# ==========================================

# 1. Ask CMake to use its auto-download tool
include(FetchContent)

# 2. Tell it exactly where to get GoogleTest from GitHub
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)

# 3. Download and make it available to our project
FetchContent_MakeAvailable(googletest)

# 4. Create our test application
add_executable(ThreadPoolTests tests/test_threadpool.cpp src/ThreadPool.cpp)

# 5. Link it to the GoogleTest we just downloaded
target_link_libraries(ThreadPoolTests gtest_main Threads::Threads)
target_include_directories(ThreadPoolTests PRIVATE include)