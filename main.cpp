#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include "ThreadPool.hpp"
#include "json.hpp" 
#include "Logger.hpp" 

using json = nlohmann::json;

std::string get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

struct PoolConfig {
    size_t worker_count = 4;
    size_t max_queue_size = 5;
};

PoolConfig read_pool_configuration() {
    PoolConfig cfg;
    std::ifstream file("config.json");
    if (file.is_open()) {
        try {
            json data;
            file >> data;
            cfg.worker_count = data["thread_pool"]["worker_count"].get<size_t>();
            cfg.max_queue_size = data["thread_pool"]["max_queue_size"].get<size_t>();
        } catch (...) {}
    }
    return cfg;
}

int main() {
    // 💡 REMOVE or make sure those temporary 'clear_log' lines are completely gone!
    PoolConfig current_cfg = read_pool_configuration();
    std::cout << "Initial setup. Threads: " << current_cfg.worker_count 
              << " | Max Queue Capacity: " << current_cfg.max_queue_size << std::endl;

    ThreadPool pool(current_cfg.worker_count, current_cfg.max_queue_size);
    // ... [keep everything else in main.cpp exactly as it was] ...

    for (int loop = 1; loop <= 3; ++loop) {
        std::cout << "\n--- [Cycle " << loop << "/3] Processing Work ---" << std::endl;
        
        for (int i = 0; i < 10; ++i) {
            try {
                // Simplified enqueue passing directly to standard task loop block
                pool.enqueue([i] {
                    std::string tid = get_thread_id();

                    if (i == 5) {
                        throw std::runtime_error("Critical computational mismatch on element ID " + std::to_string(i));
                    }

                    printf("Task %d processed by Thread %s\n", i, tid.c_str());
                    fflush(stdout); 
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                });
            } catch (const std::exception& e) {
                std::cerr << "❌ Submission Rejected on Task " << i << " -> " << e.what() << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 💡 NEW FEATURE: Read and print live runtime stats snapshot
        PoolStats live_stats = pool.get_stats();
        std::cout << "📊 [Live Monitor] Queue Depth: " << live_stats.queue_depth 
                  << " tasks | Throughput: " << live_stats.throughput << " success/sec" << std::endl;


        PoolConfig new_cfg = read_pool_configuration();
        if (new_cfg.worker_count != current_cfg.worker_count) {
            pool.reconfigure(new_cfg.worker_count);
            current_cfg = new_cfg;
        } else {
            std::cout << "[Main] Checking config.json... No thread change. (Threads: " << current_cfg.worker_count << ")" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    std::cout << "\n All execution cycles complete. Closing down." << std::endl;

    // 💡 NEW TELEMETRY READOUT VALUES
    LatencyPercentiles pct = pool.get_latency_percentiles();

    std::cout << "\n=============================================" << std::endl;
    std::cout << "        ENGINE PERFORMANCE METRICS           " << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << " Total Tasks Attempted:    " << pool.get_total_submitted() << std::endl;
    std::cout << " Total Tasks Succeeded:    " << pool.get_succeeded_count() << std::endl;
    std::cout << " Total Tasks Failed:       " << pool.get_failed_count() << std::endl;
    std::cout << " Total Tasks Rejected:     " << pool.get_rejected_count() << std::endl;
    std::cout << " Average Task Latency:     " << pool.get_avg_latency_ms() << " ms" << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << " P50 Latency (Median):     " << pct.p50 << " ms" << std::endl;
    std::cout << " P95 Latency (Tail):       " << pct.p95 << " ms" << std::endl;
    std::cout << " P99 Latency (Max Outlier):" << pct.p99 << " ms" << std::endl;
    std::cout << "=============================================" << std::endl;

    return 0;
}

