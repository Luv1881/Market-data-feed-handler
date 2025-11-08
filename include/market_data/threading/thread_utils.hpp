#pragma once

#include "../core/common.hpp"
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <climits>
#include <cstdlib>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

namespace market_data {

/**
 * Thread utilities for CPU affinity and real-time scheduling
 */
class ThreadUtils {
public:
    /**
     * Pin thread to specific CPU core
     * @param thread Thread to pin (use std::this_thread for current thread)
     * @param cpu_id CPU core ID
     * @return true if successful
     */
    static bool pin_to_cpu(std::thread& thread, int cpu_id) {
#ifdef __linux__
        // Validate cpu_id
        if (cpu_id < 0 || cpu_id >= get_num_cpus()) {
            return false;
        }

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);

        int rc = pthread_setaffinity_np(thread.native_handle(),
                                       sizeof(cpu_set_t), &cpuset);
        return rc == 0;
#else
        (void)cpu_id; // Suppress unused parameter warning
        return false; // Not supported on non-Linux platforms
#endif
    }

    /**
     * Pin current thread to specific CPU core
     */
    static bool pin_current_thread_to_cpu(int cpu_id) {
#ifdef __linux__
        // Validate cpu_id
        if (cpu_id < 0 || cpu_id >= get_num_cpus()) {
            return false;
        }

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);

        int rc = pthread_setaffinity_np(pthread_self(),
                                       sizeof(cpu_set_t), &cpuset);
        return rc == 0;
#else
        (void)cpu_id; // Suppress unused parameter warning
        return false;
#endif
    }

    /**
     * Set thread to real-time FIFO scheduling
     * @param thread Thread to configure
     * @param priority Priority (1-99, higher is more urgent)
     * @return true if successful
     */
    static bool set_realtime_priority(std::thread& thread, int priority = 99) {
#ifdef __linux__
        // Validate priority range
        if (priority < 1 || priority > 99) {
            return false;
        }

        sched_param param;
        param.sched_priority = priority;

        int rc = pthread_setschedparam(thread.native_handle(),
                                      SCHED_FIFO, &param);
        return rc == 0;
#else
        (void)priority; // Suppress unused parameter warning
        return false;
#endif
    }

    /**
     * Set current thread to real-time FIFO scheduling
     */
    static bool set_current_thread_realtime(int priority = 99) {
#ifdef __linux__
        // Validate priority range
        if (priority < 1 || priority > 99) {
            return false;
        }

        sched_param param;
        param.sched_priority = priority;

        int rc = pthread_setschedparam(pthread_self(),
                                      SCHED_FIFO, &param);
        return rc == 0;
#else
        (void)priority; // Suppress unused parameter warning
        return false;
#endif
    }

    /**
     * Set thread name (for debugging/monitoring)
     */
    static bool set_thread_name(std::thread& thread, const std::string& name) {
#ifdef __linux__
        // Linux thread names limited to 16 chars including null terminator
        std::string truncated = name.substr(0, 15);
        int rc = pthread_setname_np(thread.native_handle(), truncated.c_str());
        return rc == 0;
#else
        return false;
#endif
    }

    /**
     * Set current thread name
     */
    static bool set_current_thread_name(const std::string& name) {
#ifdef __linux__
        std::string truncated = name.substr(0, 15);
        int rc = pthread_setname_np(pthread_self(), truncated.c_str());
        return rc == 0;
#else
        return false;
#endif
    }

    /**
     * Get isolated CPU cores from /sys/devices/system/cpu/isolated
     * @return Vector of isolated CPU IDs
     */
    static std::vector<int> get_isolated_cpus() {
        std::vector<int> isolated_cpus;

#ifdef __linux__
        std::ifstream file("/sys/devices/system/cpu/isolated");
        if (!file.is_open()) {
            return isolated_cpus;
        }

        std::string line;
        if (std::getline(file, line)) {
            isolated_cpus = parse_cpu_list(line);
        }
#endif

        return isolated_cpus;
    }

    /**
     * Get number of online CPUs
     */
    static int get_num_cpus() {
#ifdef __linux__
        return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
        return static_cast<int>(std::thread::hardware_concurrency());
#endif
    }

    /**
     * Check if running with real-time capabilities
     */
    static bool has_realtime_capabilities() {
#ifdef __linux__
        sched_param param;
        int policy;
        int rc = pthread_getschedparam(pthread_self(), &policy, &param);
        return rc == 0 && policy == SCHED_FIFO;
#else
        return false;
#endif
    }

    /**
     * Yield CPU to other threads
     */
    static void yield() {
        std::this_thread::yield();
    }

    /**
     * Busy-wait spin with exponential backoff
     * @param iterations Number of spin iterations before yielding
     */
    static void spin_wait(int iterations) {
        for (int i = 0; i < iterations; ++i) {
            cpu_pause();
        }
    }

private:
    /**
     * Safe string to int conversion (no exceptions)
     */
    static bool safe_stoi(const std::string& str, int& result) {
        if (str.empty()) {
            return false;
        }

        char* end = nullptr;
        long val = std::strtol(str.c_str(), &end, 10);

        // Check for conversion errors
        if (end == str.c_str() || *end != '\0') {
            return false;
        }

        // Check for overflow
        if (val < INT_MIN || val > INT_MAX) {
            return false;
        }

        result = static_cast<int>(val);
        return true;
    }

    /**
     * Parse CPU list format (e.g., "2-7,10-15")
     * Safe version without exceptions
     */
    static std::vector<int> parse_cpu_list(const std::string& str) {
        std::vector<int> cpus;
        std::istringstream iss(str);
        std::string token;

        while (std::getline(iss, token, ',')) {
            size_t dash_pos = token.find('-');
            if (dash_pos != std::string::npos) {
                // Range (e.g., "2-7")
                int start, end;
                if (safe_stoi(token.substr(0, dash_pos), start) &&
                    safe_stoi(token.substr(dash_pos + 1), end) &&
                    start <= end) {
                    for (int i = start; i <= end; ++i) {
                        cpus.push_back(i);
                    }
                }
            } else {
                // Single CPU
                int cpu;
                if (safe_stoi(token, cpu)) {
                    cpus.push_back(cpu);
                }
            }
        }

        return cpus;
    }
};

/**
 * RAII wrapper for thread configuration
 */
class ConfiguredThread {
public:
    template <typename Func>
    ConfiguredThread(Func&& func, int cpu_id, const std::string& name, int priority = 99)
        : thread_(std::forward<Func>(func))
    {
        ThreadUtils::pin_to_cpu(thread_, cpu_id);
        ThreadUtils::set_thread_name(thread_, name);
        ThreadUtils::set_realtime_priority(thread_, priority);
    }

    ~ConfiguredThread() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    // Non-copyable
    ConfiguredThread(const ConfiguredThread&) = delete;
    ConfiguredThread& operator=(const ConfiguredThread&) = delete;

    // Movable
    ConfiguredThread(ConfiguredThread&&) noexcept = default;
    ConfiguredThread& operator=(ConfiguredThread&&) noexcept = default;

    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] std::thread::id get_id() const {
        return thread_.get_id();
    }

private:
    std::thread thread_;
};

} // namespace market_data
