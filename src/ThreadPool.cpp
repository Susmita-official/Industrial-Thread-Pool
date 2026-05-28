#include "ThreadPool.hpp"
#include "Logger.hpp"
#include <iostream>
#include <sstream>
#include <algorithm> // 💡 Required for std::sort

//this is a dummy comment

static std::string get_current_thread_string_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

ThreadPool::ThreadPool(size_t threads, size_t max_queue) 
    : stop(false), reject_tasks(false), max_queue_size(max_queue), 
      total_submitted(0), succeeded_tasks(0), failed_tasks(0), rejected_tasks(0), total_latency_us(0),
      start_time(std::chrono::steady_clock::now()) {
    
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            std::string tid = get_current_thread_string_id();
            while (true) {
                std::function<void()> task;
                std::chrono::steady_clock::time_point start_time_task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->cv.wait(lock, [this] { 
                        return this->stop.load() || !this->tasks.empty(); 
                    });
                    if (this->stop.load() && this->tasks.empty()) {
                        return;
                    }
                    task = std::move(this->tasks.front().first);
                    start_time_task = this->tasks.front().second;
                    this->tasks.pop();
                }
                
                bool success = false;
                try {
                    task();
                    succeeded_tasks.fetch_add(1); 
                    success = true;
                } 
                catch (const std::exception& e) {
                    failed_tasks.fetch_add(1);    
                    ErrorLogger::getInstance().log(tid, e.what());
                } 
                catch (...) {
                    failed_tasks.fetch_add(1);
                    ErrorLogger::getInstance().log(tid, "Unknown error.");
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_task).count();
                total_latency_us.fetch_add(duration_us);

                // 💡 NEW METRIC: Convert to milliseconds and store safely in our history list
                double duration_ms = static_cast<double>(duration_us) / 1000.0;
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex);
                    all_latencies_ms.push_back(duration_ms);
                }
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(queue_mutex);
    total_submitted.fetch_add(1);

    if (tasks.size() >= max_queue_size) {
        rejected_tasks.fetch_add(1);
        throw std::runtime_error("Backpressure Alert: Task queue is full!");
    }

    if (stop || reject_tasks) {
        throw std::runtime_error("Enqueue rejected: Pool is updating or stopped");
    }
    
    tasks.emplace(std::make_pair(task, std::chrono::steady_clock::now()));
    cv.notify_one();
}

PoolStats ThreadPool::get_stats() const {
    PoolStats stats;
    {
        std::unique_lock<std::mutex> lock(this->queue_mutex);
        stats.queue_depth = tasks.size(); 
    }
    auto now = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    if (elapsed_seconds == 0) {
        stats.throughput = static_cast<double>(succeeded_tasks.load());
    } else {
        stats.throughput = static_cast<double>(succeeded_tasks.load()) / elapsed_seconds;
    }
    return stats;
}

// 💡 NEW FEATURE: Percentiles Math Processing Engine
LatencyPercentiles ThreadPool::get_latency_percentiles() const {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    
    if (all_latencies_ms.empty()) {
        return {0.0, 0.0, 0.0};
    }

    // 1. Create a local copy to avoid modifying the active dataset runtime, then sort it
    std::vector<double> sorted_latencies = all_latencies_ms;
    std::sort(sorted_latencies.begin(), sorted_latencies.end());

    size_t size = sorted_latencies.size();

    // 2. Sample data at specific boundary landmarks
    size_t idx_p50 = static_cast<size_t>(size * 0.50);
    size_t idx_p95 = static_cast<size_t>(size * 0.95);
    size_t idx_p99 = static_cast<size_t>(size * 0.99);

    // Bound check guardrails to prevent buffer overflow constraints
    if (idx_p95 >= size) idx_p95 = size - 1;
    if (idx_p99 >= size) idx_p99 = size - 1;

    return {
        sorted_latencies[idx_p50],
        sorted_latencies[idx_p95],
        sorted_latencies[idx_p99]
    };
}

void ThreadPool::stop_and_clear_current_workers() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true; 
        std::queue<std::pair<std::function<void()>, std::chrono::steady_clock::time_point>> empty_queue;
        std::swap(tasks, empty_queue);
    }
    cv.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear(); 
}

void ThreadPool::reconfigure(size_t new_threads) {
    std::cout << "\n[ThreadPool] Reconfiguration triggered! Changing to " << new_threads << " threads." << std::endl;
    reject_tasks = true;
    stop_and_clear_current_workers();
    
    std::cout << "[ThreadPool] Old threads safely shut down. Spawning new pool..." << std::endl;
    stop = false;
    reject_tasks = false;
    
    for (size_t i = 0; i < new_threads; ++i) {
        workers.emplace_back([this] {
            std::string tid = get_current_thread_string_id();
            while (true) {
                std::function<void()> task;
                std::chrono::steady_clock::time_point start_time_task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->cv.wait(lock, [this] { 
                        return this->stop.load() || !this->tasks.empty(); 
                    });
                    if (this->stop.load() && this->tasks.empty()) {
                        return;
                    }
                    task = std::move(this->tasks.front().first);
                    start_time_task = this->tasks.front().second;
                    this->tasks.pop();
                }
                
                try {
                    task();
                    succeeded_tasks.fetch_add(1);
                } 
                catch (const std::exception& e) {
                    failed_tasks.fetch_add(1);
                    ErrorLogger::getInstance().log(tid, e.what());
                } 
                catch (...) {
                    failed_tasks.fetch_add(1);
                    ErrorLogger::getInstance().log(tid, "Unknown error.");
                }

                auto end_time = std::chrono::steady_clock::now();
                auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time_task).count();
                total_latency_us.fetch_add(duration_us);

                double duration_ms = static_cast<double>(duration_us) / 1000.0;
                {
                    std::lock_guard<std::mutex> lock(metrics_mutex);
                    all_latencies_ms.push_back(duration_ms);
                }
            }
        });
    }
    std::cout << "✅ [ThreadPool] Reconfiguration successfully complete!\n" << std::endl;
}

ThreadPool::~ThreadPool() {
    stop_and_clear_current_workers();
}
