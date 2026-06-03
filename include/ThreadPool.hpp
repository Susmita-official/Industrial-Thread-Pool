#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <chrono>
#include <memory>
#include <type_traits>
#include <stdexcept>

enum class TaskPriority { HIGH = 0, NORMAL = 1, LOW = 2 };

// 💡 NEW: Wrapper struct to hold the task, its timestamp, and its priority
struct PrioritizedTask {
    std::function<void()> fn;
    std::chrono::steady_clock::time_point enqueue_time;
    TaskPriority priority;
    
    // 💡 THE FIX: If priorities are identical, the older task gets executed first.
    // std::greater puts the "smallest" at the top. So returning true here pushes the task DOWN the queue.
    bool operator>(const PrioritizedTask& other) const {
        if (priority == other.priority) {
            return enqueue_time > other.enqueue_time;
        }
        return static_cast<int>(priority) > static_cast<int>(other.priority);
    }
};

struct PoolStats {
    size_t queue_depth;     
    double throughput;      
};

struct LatencyPercentiles {
    double p50; 
    double p95; 
    double p99; 
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
    LatencyPercentiles get_latency_percentiles() const;

    // 💡 UPDATED: Now takes an optional priority (Defaults to NORMAL)
    void enqueue(std::function<void()> task, TaskPriority priority = TaskPriority::NORMAL);

    // 💡 UPDATED: submit now expects a priority parameter before the arguments
    // Example: pool.submit(TaskPriority::HIGH, myFunction, arg1);
    template<typename F, typename... Args>
    auto submit(TaskPriority priority, F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = task->get_future();

        // Pass the priority down to enqueue
        enqueue([task]() {
            (*task)();
        }, priority);

        return res;
    }
    
    // Convenience overload: If they don't provide a priority, default to NORMAL
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        return submit(TaskPriority::NORMAL, std::forward<F>(f), std::forward<Args>(args)...);
    }

private:
    void worker_loop();
    void stop_and_clear_current_workers();

    std::vector<std::thread> workers;
    
    // 💡 UPDATED: Replaced std::queue with std::priority_queue
    std::priority_queue<PrioritizedTask, std::vector<PrioritizedTask>, std::greater<PrioritizedTask>> tasks;
    
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

    mutable std::mutex metrics_mutex;
    std::deque<double> latency_window;

    std::chrono::steady_clock::time_point start_time;
};

#endif