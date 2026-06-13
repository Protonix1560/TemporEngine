
#ifndef THREADING_THREADING_HPP_
#define THREADING_THREADING_HPP_


#include "core.hpp"
#include "plugin_core.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <functional>
#include <deque>
#include <mutex>


// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;


struct JobEntry;

struct Job {
    JobEntry* entry;
};

enum class JobState {
    WaitingDependencies = 0,
    Running = 1,
    Finished = 2
};

struct JobEntry {
    uint32_t id;
    TprJobType type;
    std::function<void(void* ctx)> func;
    void* ctx;
    float priority;
    std::atomic<JobState> state = JobState::WaitingDependencies;
    std::mutex mutex;
    std::vector<Job> dependentJobs;
    std::unordered_set<uint32_t> dependencies;
};


struct Queue {

    public:
        void push(Job job) {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mJobs.end();
                while (it != mJobs.begin() && std::prev(it)->entry->priority < job.entry->priority) it--;
                mJobs.insert(it, job);
            }
            mCv.notify_all();
        }

        std::optional<Job> pull() {
            std::unique_lock<std::mutex> lock(mMutex);
            mCv.wait(lock, [this]() { return !mJobs.empty(); });
            Job job = mJobs.front();
            mJobs.pop_front();
            return job;
        }

        std::optional<Job> tryPull(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::zero()) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mCv.wait_for(lock, timeout, [this]() { return !mJobs.empty(); })) {
                return std::nullopt;
            }
            Job job = mJobs.front();
            mJobs.pop_front();
            return job;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mMutex);
            return mJobs.empty();
        }

        bool size() const {
            std::lock_guard<std::mutex> lock(mMutex);
            return mJobs.size();
        }

        void notifyAll() {
            mCv.notify_all();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(mMutex);
            mJobs.clear();
        }

    private:
        std::deque<Job> mJobs;
        mutable std::mutex mMutex;
        std::condition_variable mCv;
};


enum class ThreadState {
    Idle = 0,
    Working = 1,
    Finished = 2
};


struct Thread {
    std::jthread thread;
    std::atomic<ThreadState> state;
    std::atomic<std::chrono::time_point<std::chrono::steady_clock>> jobBegin;
    uint32_t id;
    Thread() : thread(), state(ThreadState::Idle) {}
    Thread(Thread&& other) : thread(std::move(other.thread)), state(other.state.load()) {}
};


class Threading {

    public:
        Threading(Logger& rLogger, Settings& rSetting);
        void update();
        ~Threading();

        expected<TprJob, TprResult> createJob(const TprJobCreateInfo* pInfo) noexcept;
        TprResult createDetachedJob(const TprJobCreateInfo* pInfo) noexcept;
        expected<TprBool8, TprResult> jobFinished(TprJob job) noexcept;
        void joinJob(TprJob job) noexcept;

        void joinAll() noexcept;
        expected<uint32_t, TprResult> getJobID(TprJob job) noexcept;

    private:
        Logger& mrLogger;
        Settings& mrSett;

        bool mUsable = true;
        std::mutex mMutex;

        uint32_t mShortPoolSize;
        std::chrono::nanoseconds mThreadTotalTimeout;
        std::chrono::nanoseconds mShortThreadMigrationTimeout;

        std::vector<std::unique_ptr<Thread>> mShortPool;
        std::vector<std::unique_ptr<Thread>> mLongThreads;
        std::unique_ptr<Queue> mQueue;
        uint32_t mThreadCounter = 0;

        std::unordered_map<uint32_t, JobEntry> mJobs;
        uint32_t mMapJobCounter = 0;
        std::vector<std::unique_ptr<JobEntry>> mDetachedJobs;
        uint32_t mJobCounter = 0;

};

REGISTER_TYPE_NAME_S(Threading, "Thrd");


#endif  // THREADING_THREADING_HPP_
