
#ifndef THREAD_JOB_INFO_HPP_
#define THREAD_JOB_INFO_HPP_

#include "plugin_core.h"

#include <optional>


struct ThreadLocalJobInfo {
    bool mainThread = false;
    std::optional<TprJob> job;
};

inline thread_local ThreadLocalJobInfo threadLocalJobInfo{};


#endif  // THREAD_JOB_INFO_HPP_
