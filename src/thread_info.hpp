
#ifndef THREAD_JOB_INFO_HPP_
#define THREAD_JOB_INFO_HPP_

#include "atomic_counter.hpp"

#include <atomic>
#include <memory>


struct PluginContext {
    const uint32_t id = 0;
    const std::string name;
    const bool engine = false;
    std::atomic<uint32_t> executions = 0;
    atomic_counter<bool> unloaded;
    PluginContext(uint32_t id, std::string_view name) : id(id), name(name) {}
    PluginContext(bool engine) : engine(engine), name("engine") {}
};

struct ThreadInfo {
    std::shared_ptr<PluginContext> currentPlugin;
};

inline thread_local ThreadInfo threadInfo{};


#endif  // THREAD_JOB_INFO_HPP_
