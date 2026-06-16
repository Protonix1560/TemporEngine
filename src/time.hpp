
#ifndef TIME_HPP_
#define TIME_HPP_

#include <chrono>
#include <format>


inline std::string current_time() {
    auto now = std::chrono::system_clock::now();
    return std::format("{:%H:%M:%S}", now);
}


#endif  // TIME_HPP_
