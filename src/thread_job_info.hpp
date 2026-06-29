
#ifndef THREAD_JOB_INFO_HPP_
#define THREAD_JOB_INFO_HPP_

#include <cstdint>
#include <optional>


struct ThreadLocalJobInfo {
    bool mainThread = false;
    std::optional<uint32_t> job;
};

inline thread_local ThreadLocalJobInfo threadLocalJobInfo{};


#endif  // THREAD_JOB_INFO_HPP_
