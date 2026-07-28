
#ifndef THREADING_THREADING_HPP_
#define THREADING_THREADING_HPP_

#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"

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
#include <queue>


struct JobEntry {
    std::mutex mutex;
    bool destroyed = false;
    bool destructionPended = false;
    std::optional<std::chrono::steady_clock::time_point> nextLaunchTimepoint;

    const std::vector<std::weak_ptr<JobEntry>> dependencies;
    size_t countdown;
    std::vector<std::shared_ptr<JobEntry>> dependents;
    size_t usage = 0;

    const TprJob mainHandle;
    std::vector<uint32_t> capabilities;

    void(*const function)(void* context, TprJob job);
    void* const context;
    const TprJobDuration duration;

    template <std::input_iterator It>
    JobEntry(const TprJobCreateInfo& info, TprJob mainHandle, It depsBegin, It depsEnd)
        : function(info.function), context(info.context), duration(info.duration),
        mainHandle(mainHandle), dependencies(depsBegin, depsEnd), countdown(dependencies.size()) {}

    JobEntry(const TprJobCreateInfo& info, TprJob mainHandle)
        : function(info.function), context(info.context), duration(info.duration), mainHandle(mainHandle) {}
};


struct JobHandle {
    TprJobCapabilityFlags capability = std::numeric_limits<TprJobCapabilityFlags>::max();
    std::shared_ptr<JobEntry> entry;
};


struct Thread {
    const uint32_t id;
    std::jthread thread;
    Thread(uint32_t id) : id(id) {}
};


struct JobLaunch {
    std::shared_ptr<JobEntry> entry;
    std::chrono::steady_clock::time_point timepoint;
    std::strong_ordering operator<=>(const JobLaunch& other) const {
        return timepoint <=> other.timepoint;
    }
};

struct JobQueue {
    public:
        void push(JobLaunch&& entry) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                mJobs.push(std::forward<JobLaunch>(entry));
            }
            mCv.notify_all();
        }

        std::optional<JobLaunch> pull(std::stop_token stop) {
            std::stop_callback callback(stop, [&]() { mCv.notify_all(); });
            std::unique_lock<std::mutex> lock(mMutex);
            while (!stop.stop_requested()) {
                if (mJobs.empty()) {
                    mCv.wait(lock);
                } else {
                    mCv.wait_until(lock, mJobs.top().timepoint);
                }
                if (!mJobs.empty()) {
                    auto now = std::chrono::steady_clock::now();
                    if (mJobs.top().timepoint <= now) {
                        auto launch = mJobs.top();
                        mJobs.pop();
                        return launch;
                    }
                }
            }
            return std::nullopt;
        }

    private:
        std::mutex mMutex;
        std::priority_queue<JobLaunch, std::vector<JobLaunch>, std::greater<JobLaunch>> mJobs;
        std::condition_variable mCv;
};


// from "settings.hpp"
class Settings;


class Scheduler {
    public:
        Scheduler(Logger logger, Settings& rSetting);
        TprResult init();
        void shutdown();
        ~Scheduler();

        expected<TprJob, TprResult> createJob(const TprJobCreateInfo* pInfo) noexcept;
        expected<TprJob, TprResult> createJobCapability(TprJob job, TprJobCapabilityFlags mask) noexcept;
        TprResult scheduleJob(TprJob job, uint64_t timepoint) noexcept;
        void pendJobDestruction(TprJob job) noexcept;
        uint64_t now() noexcept;

    private:

        void shortThread(std::stop_token stop, std::shared_ptr<Thread> thread) noexcept;
        void processLaunch(JobLaunch launch);

        Logger mLogger;
        Logger mSpamLogger;
        Settings& mrSett;

        std::mutex mMutex;
        bool mInitialized = false;

        uint32_t mShortPoolSize;
        std::chrono::steady_clock::duration mShortThreadMigrationTimeout;
        const std::chrono::steady_clock::time_point mTimeBegin;

        std::unordered_map<uint32_t, std::shared_ptr<Thread>> mThreads;
        uint32_t mThreadCounter = 0;

        std::unordered_map<uint32_t, JobHandle> mJobs;
        uint32_t mJobCounter = 0;
        
        JobQueue mQueue;

};

REGISTER_TYPE_NAME_S(Scheduler, "Schd");


#endif  // THREADING_THREADING_HPP_
