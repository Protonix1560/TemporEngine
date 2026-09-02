
#ifndef THREADING_THREADING_HPP_
#define THREADING_THREADING_HPP_

#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>
#include <mutex>


template <typename T>
class atomic_counter {
    public:
        atomic_counter() : value() {}
        atomic_counter(T v) : value(v) {}

        T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
            return value.load(order);
        }
        T exchange_increment(std::memory_order order = std::memory_order_seq_cst) noexcept {
            T v = value.load(order);
            while (true) {
                if (v == std::numeric_limits<T>::max()) return v;
                bool ok = value.compare_exchange_weak(v, v + 1, order);
                if (ok) return v;
            }
        }
        void wait(T v, std::memory_order order = std::memory_order_seq_cst) const noexcept {
            value.wait(v, order);
        }
        void notify_all() noexcept {
            value.notify_all();
        }
        void notify_one() noexcept {
            value.notify_one();
        }
    private:
        std::atomic<T> value;
};

template <>
class atomic_counter<bool> {
    public:
        atomic_counter() : value() {}
        atomic_counter(bool value) : value(value) {}

        bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
            return value.load(order);
        }
        void store_true(std::memory_order order = std::memory_order_seq_cst) noexcept {
            value.store(true, order);
        }
        bool exchange_true(std::memory_order order = std::memory_order_seq_cst) noexcept {
            return value.exchange(true, order);
        }
        void wait(bool v, std::memory_order order = std::memory_order_seq_cst) const noexcept {
            value.wait(v, order);
        }
        void notify_all() noexcept {
            value.notify_all();
        }
        void notify_one() noexcept {
            value.notify_one();
        }
    private:
        std::atomic<bool> value;
};


struct JobEntry;

struct SharedJobMeta {
    std::shared_ptr<JobEntry> entry;
    TprJob handle;
};

struct WeakJobMeta {
    std::weak_ptr<JobEntry> entry;
    TprJob handle;
    WeakJobMeta(const SharedJobMeta& meta) : entry(meta.entry), handle(meta.handle) {}
};

struct JobEntry {
    atomic_counter<bool> destroyed;
    atomic_counter<bool> destructionPended;
    atomic_counter<bool> invalidated;

    std::mutex mutex;

    const std::vector<WeakJobMeta> dependencies;
    size_t countdown;
    std::vector<SharedJobMeta> dependents;
    size_t usage = 0;

    void(*const function)(void* context);
    void* const context;
    const TprJobDuration duration;

    template <std::input_iterator It>
    JobEntry(const TprJobCreateInfo& info, It depsBegin, It depsEnd)
        : function(info.function), context(info.context), duration(info.duration),
        dependencies(depsBegin, depsEnd), countdown(dependencies.size()) {}

    JobEntry(const TprJobCreateInfo& info)
        : function(info.function), context(info.context), duration(info.duration) {}
};


struct JobHandle {
    TprJobCapabilityFlags capability = std::numeric_limits<TprJobCapabilityFlags>::max();
    std::shared_ptr<JobEntry> entry;
};


struct Thread {
    const uint32_t id;
    atomic_counter<bool> ready{false};
    std::jthread thread;
    Thread(uint32_t id) : id(id) {}
};


struct JobLaunch {
    SharedJobMeta meta;
    std::chrono::steady_clock::time_point timepoint;
    std::strong_ordering operator<=>(const JobLaunch& other) const {
        return timepoint <=> other.timepoint;
    }
};

struct JobQueue {
    public:
        void push(const JobLaunch& launch) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = std::ranges::lower_bound(mLaunches, launch, std::less<JobLaunch>{});
                mLaunches.emplace(it, launch);
            }
            mCv.notify_all();
        }

        std::optional<JobLaunch> pull(std::stop_token stop, std::chrono::steady_clock::duration waitTimeout) {
            std::stop_callback callback(stop, [&]() { mCv.notify_all(); });
            std::unique_lock<std::mutex> lock(mMutex);
            while (!stop.stop_requested()) {
                if (mLaunches.empty()) {
                    mCv.wait_for(lock, waitTimeout);
                } else {
                    // `deadline` must be in a local variable because std::min returns a reference to the min element
                    // and `atime` in wait_until for some reason is a reference too
                    auto deadline = std::min(std::chrono::steady_clock::now() + waitTimeout, mLaunches.front().timepoint);
                    mCv.wait_until(lock, deadline);
                }
                auto now = std::chrono::steady_clock::now();
                for (auto it = mLaunches.begin(); it < mLaunches.end(); it++) {
                    auto launch = *it;
                    if (launch.timepoint > now) break;
                    std::lock_guard<std::mutex> entryLock(launch.meta.entry->mutex);
                    if (launch.meta.entry->usage == 0) {
                        // incrementing it here so another thread wouldn't pull a launch
                        // of this job before working thread locks it
                        launch.meta.entry->usage++;
                        mLaunches.erase(it);
                        lock.unlock();
                        mCv.notify_all();
                        return launch;
                    }
                }
            }
            return std::nullopt;
        }

        void notify() {
            mCv.notify_all();
        }

    private:
        std::mutex mMutex;
        std::vector<JobLaunch> mLaunches;
        std::condition_variable mCv;
};


// from "settings.hpp"
class Settings;


class Scheduler {
    public:
        Scheduler(Logger logger, Settings& rSetting, std::atomic<TprResult>& rRunResult);
        TprResult init();
        void shutdown();
        ~Scheduler();

        expected<TprJob, TprResult> createJob(const TprJobCreateInfo& info) noexcept;
        expected<TprJob, TprResult> createJobCapability(TprJob job, TprJobCapabilityFlags mask) noexcept;
        TprResult scheduleJob(TprJob job, uint64_t timepoint) noexcept;
        void invalidateJob(TprJob job) noexcept;
        void destroyJob(TprJob job) noexcept;
        uint64_t now() noexcept;

        std::chrono::steady_clock::time_point timeBegin();

    private:

        void shortThread(std::stop_token stop, std::shared_ptr<Thread> thread) noexcept;
        void processLaunch(JobLaunch launch);

        Logger mLogger;
        Logger mSpamLogger;
        Settings& mrSett;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;
        bool mInitialised = false;

        uint32_t mShortPoolSize;
        std::chrono::steady_clock::duration mShortThreadMigrationTimeout;
        std::chrono::steady_clock::duration mThreadPullWaitTimeout;
        const std::chrono::steady_clock::time_point mTimeBegin;

        std::unordered_map<uint32_t, std::shared_ptr<Thread>> mThreads;
        uint32_t mThreadCounter = 0;

        std::unordered_map<uint32_t, JobHandle> mJobs;
        uint32_t mJobCounter = 0;
        
        JobQueue mQueue;

};

REGISTER_TYPE_NAME_S(Scheduler, "Schd");


#endif  // THREADING_THREADING_HPP_
