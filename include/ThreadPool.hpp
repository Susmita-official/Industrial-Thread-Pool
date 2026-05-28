#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <chrono>

struct PoolStats {
    size_t queue_depth;     
    double throughput;      
};

// 💡 NEW STRUCTURE: Holds percentile calculation outputs
struct LatencyPercentiles {
    double p50; // Median
    double p95; // 95th percentile
    double p99; // 99th percentile
};

class ThreadPool {
public:
    explicit ThreadPool(size_t threads, size_t max_queue_size = 5);
    ~ThreadPool();

    void reconfigure(size_t new_threads);

    size_t get_total_submitted() const { return total_submitted.load(); }
    size_t get_succeeded_count() const { return succeeded_tasks.load(); }
    size_t get_failed_count() const { return failed_tasks.load(); }
    size_t get_rejected_count() const { return rejected_tasks.load(); }
    
    double get_avg_latency_ms() const {
        size_t total_processed = succeeded_tasks.load() + failed_tasks.load();
        if (total_processed == 0) return 0.0;
        return static_cast<double>(total_latency_us.load()) / total_processed / 1000.0;
    }

    PoolStats get_stats() const;

    // 💡 NEW FEATURE: Public getter to compute and return percentiles
    LatencyPercentiles get_latency_percentiles() const;

    void enqueue(std::function<void()> task);

private:
    void stop_and_clear_current_workers();

    std::vector<std::thread> workers;
    std::queue<std::pair<std::function<void()>, std::chrono::steady_clock::time_point>> tasks;
    
    mutable std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop;
    std::atomic<bool> reject_tasks; 

    size_t max_queue_size;
    
    std::atomic<size_t> total_submitted;
    std::atomic<size_t> succeeded_tasks;
    std::atomic<size_t> failed_tasks;
    std::atomic<size_t> rejected_tasks;
    std::atomic<uint64_t> total_latency_us;

    // 💡 NEW METRICS TRACKING: Container and mutex to store individual latency metrics safely
    mutable std::mutex metrics_mutex;
    std::vector<double> all_latencies_ms;

    std::chrono::steady_clock::time_point start_time;
};

#endif
