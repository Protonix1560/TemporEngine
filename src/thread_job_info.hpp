
#ifndef THREAD_JOB_INFO_HPP_
#define THREAD_JOB_INFO_HPP_

#include "plugin_core.h"

#include <optional>


struct ThreadInfo {
    bool mainThread = false;
    std::optional<TprJob> currentJob;
};

inline thread_local ThreadInfo threadInfo{};


#endif  // THREAD_JOB_INFO_HPP_
