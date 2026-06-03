#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include <vector>
#include <future>
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
    size_t max_queue_size = 15; // Increased slightly to hold our test tasks
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
        } catch (...) {
            // fallback to defaults
        }
    }
    return cfg;
}

int main() {
    PoolConfig current_cfg = read_pool_configuration();
    std::cout << "Starting ThreadPool with " << current_cfg.worker_count << " workers...\n";

    ThreadPool pool(current_cfg.worker_count, current_cfg.max_queue_size);

    for (int loop = 1; loop <= 3; ++loop) {
        std::cout << "\n--- [Cycle " << loop << "/3] Creating Traffic Jam ---" << std::endl;
        
        std::vector<std::future<int>> cycle_futures;

        for (int i = 0; i < 10; ++i) {
            try {
                TaskPriority priority_level;
                std::string priority_name;

                // Tasks 0-3 are LOW priority (These grab the threads first)
                if (i < 4) {
                    priority_level = TaskPriority::LOW;
                    priority_name = "LOW";
                } 
                // Task 9 is HIGH priority (Submitted last)
                else if (i == 9) {
                    priority_level = TaskPriority::HIGH;
                    priority_name = "HIGH🚨";
                } 
                // Tasks 4-8 are NORMAL priority (Submitted in the middle)
                else {
                    priority_level = TaskPriority::NORMAL;
                    priority_name = "NORMAL";
                }

                // Submit with priority!
                cycle_futures.push_back(pool.submit(priority_level, [i, priority_name] {
                    std::string tid = get_thread_id();

                    if (i == 5) {
                        throw std::runtime_error("Critical failure on Task 5");
                    }

                    printf("Task %d [%s] processed by Thread %s\n", i, priority_name.c_str(), tid.c_str());
                    fflush(stdout); 
                    
                    // Sleep to keep the thread busy and force a queue to form
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    return i * 10;
                }));
            } catch (const std::exception& e) {
                std::cerr << "❌ Queue Full: " << e.what() << std::endl;
            }
        }

        std::cout << "All 10 tasks submitted! Waiting for completion..." << std::endl;

        for (size_t i = 0; i < cycle_futures.size(); ++i) {
            try {
                int result = cycle_futures[i].get();
            } catch (const std::exception& e) {
                std::cerr << "⚠️ Main thread caught worker failure: " << e.what() << std::endl;
            }
        }
    }

    // ... end of your loops ...

    std::cout << "\n=============================================" << std::endl;
    std::cout << "        ENGINE PERFORMANCE METRICS           " << std::endl;
    std::cout << "=============================================" << std::endl;
    
    // 💡 Added the missing print statements back!
    std::cout << " Total Tasks Attempted:    " << pool.get_total_submitted() << std::endl;
    std::cout << " Total Tasks Succeeded:    " << pool.get_succeeded_count() << std::endl;
    std::cout << " Total Tasks Failed:       " << pool.get_failed_count() << std::endl;
    std::cout << " Total Tasks Rejected:     " << pool.get_rejected_count() << std::endl;
    
    LatencyPercentiles pct = pool.get_latency_percentiles();
    std::cout << " Average Task Latency:     " << pool.get_avg_latency_ms() << " ms" << std::endl;
    std::cout << " P99 Latency (Max Outlier):" << pct.p99 << " ms" << std::endl;
    std::cout << "=============================================" << std::endl;

    return 0;
}