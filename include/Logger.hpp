#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iostream>

class ErrorLogger {
public:
    static ErrorLogger& getInstance() {
        static ErrorLogger instance; 
        return instance;
    }

    void log(const std::string& thread_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        
        // 💡 ABSOLUTE PATH FIX: Points explicitly to your folder. It will recreate the file automatically!
        std::string path = "error.log"; 
        std::ofstream log_file(path, std::ios::app); 
        
        if (log_file.is_open()) {
            log_file << "[" << get_current_timestamp() << "] "
                     << "[Thread: " << thread_id << "] "
                     << "ERROR: " << message << std::endl;
            log_file.close();
            std::cout << "🔍 [Logger Active] SUCCESS: Captured error and updated error.log for Thread " << thread_id << std::endl;
        } else {
            std::cerr << "❌ [Logger Active] CRITICAL ERROR: System cannot write to " << path << std::endl;
        }
    }

private:
    // Wipes old files clean exactly once when the application boots up
    ErrorLogger() {
        std::string path = "error.log";
        std::ofstream clear_file(path, std::ios::trunc);
        clear_file.close();
    }
    
    ~ErrorLogger() {}

    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        std::stringstream ss;
        ss << "Unix Epoch: " << seconds << "s";
        return ss.str();
    }

    std::mutex log_mutex;
    
    ErrorLogger(const ErrorLogger&) = delete;
    ErrorLogger& operator=(const ErrorLogger&) = delete;
};

#endif
