
#ifndef THREADING_HPP_
#define THREADING_HPP_


#include "core.hpp"
#include "plugin_core.h"

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
    std::function<void(void* ctx)> func;
    void* ctx;
    JobEntry* entry;
    float priority;
};

struct JobEntry {
    uint32_t id;
    std::atomic<bool> finished = false;
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
                while (it != mJobs.begin() && std::prev(it)->priority < job.priority) it--;
                mJobs.insert(it, job);
            }
            mCv.notify_all();
        }

        std::optional<Job> pull() {
            std::unique_lock<std::mutex> lock(mMutex);
            mCv.wait(lock, [this]() { return !mJobs.empty(); });
            Job job = std::move(mJobs.front());
            mJobs.pop_front();
            return job;
        }

        std::optional<Job> tryPull(std::chrono::nanoseconds timeout = std::chrono::nanoseconds::zero()) {
            std::unique_lock<std::mutex> lock(mMutex);
            if (!mCv.wait_for(lock, timeout, [this]() { return !mJobs.empty(); })) {
                return std::nullopt;
            }
            Job job = std::move(mJobs.front());
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

    private:
        std::deque<Job> mJobs;
        mutable std::mutex mMutex;
        std::condition_variable mCv;
};


struct Thread {
    std::jthread thread;
    std::atomic<bool> running;
    Thread() : thread(), running(true) {}
    Thread(Thread&& other) : thread(std::move(other.thread)), running(other.running.load()) {}
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

    private:
        Logger& mrLogger;
        Settings& mrSett;

        uint32_t mShortPoolSize;
        std::chrono::nanoseconds mThreadTotalTimeout;

        std::vector<std::unique_ptr<Thread>> mShortPool;
        std::unique_ptr<Queue> mQueue;

        std::unordered_map<uint32_t, JobEntry> mJobs;
        uint32_t mMapJobCounter = 0;
        std::vector<std::unique_ptr<JobEntry>> mDetachedJobs;
        uint32_t mJobCounter = 0;

};

REGISTER_TYPE_NAME_S(Threading, "Thrd");


#endif  // THREADING_HPP_
