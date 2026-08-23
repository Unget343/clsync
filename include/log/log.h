#pragma once
#include <cmath>
#include <iostream>
#include <chrono>
#include <string>
#include <iomanip>
#include <mutex>

inline void output(const std::string& caller, int error_code, const std::string& message) {
    static const auto start_time = std::chrono::steady_clock::now();
    static std::mutex output_mutex;
    std::lock_guard lock(output_mutex);

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - start_time;

    std::cout << "[ " << std::fixed << std::setprecision(6) << elapsed.count() 
              << " " << caller << " " << error_code << " " << message << "\n" << std::flush;
}
