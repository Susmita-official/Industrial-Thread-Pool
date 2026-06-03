#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <stdexcept>
#include "ThreadPool.hpp"

// ---------------------------------------------------------
// TEST 1: Basic Submission
// Proves: The system can take a task, execute it, and return the correct value.
// ---------------------------------------------------------
TEST(ThreadPoolTest, BasicSubmitAndGet) {
    ThreadPool pool(2, 10);
    
    // Submit a basic math problem
    auto future = pool.submit([]() { return 42; });
    
    // The result should exactly equal 42
    EXPECT_EQ(future.get(), 42);
}

// ---------------------------------------------------------
// TEST 2: Backpressure (Queue Full)
// Proves: If the inbox is overflowing, it rejects new work instead of crashing.
// ---------------------------------------------------------
TEST(ThreadPoolTest, QueueFullThrows) {
    // 1 Worker TA, exactly 1 slot in the inbox tray
    ThreadPool pool(1, 1);  

    // 💡 NEW: A communication channel between the worker and the main thread
    std::promise<void> worker_started;
    std::future<void> future_started = worker_started.get_future();

    // 1. Give the worker a task, and have it signal us the exact moment it starts
    pool.submit([&worker_started]() { 
        worker_started.set_value(); // Signal the main thread: "I'm busy now!"
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    });

    // 💡 NEW: The main thread pauses right here until the worker sends the signal
    future_started.wait();

    // 2. Now we know for a fact the worker is busy and the queue is empty.
    // We put one task in the inbox (Inbox is now exactly full)
    pool.submit([]() { return 0; });

    // 3. Try to add another task. This MUST throw a runtime_error!
    EXPECT_THROW(
        pool.submit([]() { return 0; }), 
        std::runtime_error
    );
}

// ---------------------------------------------------------
// TEST 3: Graceful Shutdown
// Proves: Destroying the pool while tasks are pending doesn't crash the system.
// ---------------------------------------------------------
TEST(ThreadPoolTest, GracefulShutdown) {
    // We create an artificial scope using brackets
    {
        ThreadPool pool(4, 150);
        // Spam the pool with 100 tasks
        for (int i = 0; i < 100; ++i) {
            pool.enqueue([]() { 
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
            });
        }
    } // The pool goes out of scope here and its destructor is called!
    
    // If we reach this line without a segmentation fault, the test passes.
    SUCCEED();
}

// ---------------------------------------------------------
// TEST 4: Telemetry Accuracy
// Proves: The sliding window correctly calculates real-world time.
// ---------------------------------------------------------
TEST(ThreadPoolTest, LatencyPercentilesAccurate) {
    ThreadPool pool(2, 10);
    
    // Submit two tasks that we mathematically KNOW take 50ms
    auto f1 = pool.submit([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    auto f2 = pool.submit([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    
    // Wait for them to finish
    f1.get(); 
    f2.get();

    LatencyPercentiles pct = pool.get_latency_percentiles();
    
    // The Median (P50) should be at least 50ms, but allow a little overhead for CPU processing (up to 80ms)
    EXPECT_GE(pct.p50, 45.0); 
    EXPECT_LE(pct.p50, 80.0);
}

// ---------------------------------------------------------
// TEST 5: Thread Safety (Concurrent Enqueue)
// Proves: 10 different people yelling orders at the same time don't corrupt the data.
// ---------------------------------------------------------
TEST(ThreadPoolTest, ConcurrentEnqueue) {
    // A pool with enough queue space to hold all tasks
    ThreadPool pool(4, 2000); 
    std::vector<std::thread> submitters;

    // Create 10 outside threads...
    for (int i = 0; i < 10; ++i) {
        submitters.emplace_back([&pool]() {
            // ...each rapidly submitting 100 tasks
            for (int j = 0; j < 100; ++j) {
                pool.enqueue([]() {});
            }
        });
    }

    // Wait for all the outside threads to finish submitting
    for (auto& t : submitters) {
        t.join();
    }

    // 10 threads * 100 tasks = EXACTLY 1000 tasks submitted. 
    // If atomic locks failed, this number would be wrong!
    EXPECT_EQ(pool.get_total_submitted(), 1000);
}