
#include "threading.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "settings.hpp"

#include "thread_job_info.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <cmath>


using namespace std::chrono_literals;


void shortThread(std::stop_token stop, std::reference_wrapper<Queue> queueWrapper, std::reference_wrapper<std::atomic<ThreadState>> stateWrapper) noexcept {
    Queue& queue = queueWrapper.get();
    std::atomic<ThreadState>& state = stateWrapper.get();
    try {
        while (!stop.stop_requested()) {
            auto value = queue.tryPull(std::chrono::duration_cast<std::chrono::nanoseconds>(100ms));
            if (value.has_value()) {
                state.store(ThreadState::Working);
                state.notify_all();
                Job job = value.value();
                {
                    std::lock_guard<std::mutex> lock(job.entry->mutex);
                    threadLocalJobInfo.job = job.entry->id;
                    job.entry->func(job.entry->ctx);
                    // Great Job!!!
                    threadLocalJobInfo.job.reset();
                    if (!job.entry->dependentJobs.empty()) {
                        for (auto& dependent : job.entry->dependentJobs) {
                            std::lock_guard<std::mutex> dependentLock(dependent.entry->mutex);
                            auto it = dependent.entry->dependencies.find(job.entry->id);
                            if (it != dependent.entry->dependencies.end()) {
                                dependent.entry->dependencies.erase(it);
                            }
                            if (dependent.entry->dependencies.empty()) {
                                switch (dependent.entry->type) {
                                    case TPR_JOB_TYPE_SHORT_TERM: {
                                        dependent.entry->state.store(JobState::Running);
                                        dependent.entry->state.notify_all();
                                        queue.push(dependent);
                                        break;
                                    }
                                    case TPR_JOB_TYPE_LONG_TERM:
                                    default: {
                                        dependent.entry->state.store(JobState::Running);
                                        dependent.entry->state.notify_all();
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                job.entry->state.store(JobState::Finished);
                job.entry->state.notify_all();

                state.store(ThreadState::Idle);
                state.notify_all();
            }
        }

    } catch (...) {}

    state.store(ThreadState::Finished);
    state.notify_all();
}


void longThread(std::stop_token stop, Job job, std::reference_wrapper<Queue> queueWrapper, std::reference_wrapper<std::atomic<ThreadState>> stateWrapper) noexcept {
    Queue& queue = queueWrapper.get();
    std::atomic<ThreadState>& state = stateWrapper.get();
    try {
        job.entry->state.wait(JobState::WaitingDependencies);
        state.store(ThreadState::Working);
        state.notify_all();
        {
            std::lock_guard<std::mutex> lock(job.entry->mutex);
            threadLocalJobInfo.job = job.entry->id;
            job.entry->func(job.entry->ctx);
            // Great Job!!!
            threadLocalJobInfo.job.reset();
            if (!job.entry->dependentJobs.empty()) {
                for (auto& dependent : job.entry->dependentJobs) {
                    std::lock_guard<std::mutex> dependentLock(dependent.entry->mutex);
                    auto it = dependent.entry->dependencies.find(job.entry->id);
                    if (it != dependent.entry->dependencies.end()) {
                        dependent.entry->dependencies.erase(it);
                    }
                    if (dependent.entry->dependencies.empty()) {
                        switch (dependent.entry->type) {
                            case TPR_JOB_TYPE_SHORT_TERM: {
                                dependent.entry->state.store(JobState::Running);
                                dependent.entry->state.notify_all();
                                queue.push(dependent);
                                break;
                            }
                            case TPR_JOB_TYPE_LONG_TERM:
                            default: {
                                dependent.entry->state.store(JobState::Running);
                                dependent.entry->state.notify_all();
                                break;
                            }
                        }
                    }
                }
            }
        }
        job.entry->state.store(JobState::Finished);
        job.entry->state.notify_all();

        state.store(ThreadState::Idle);
        state.notify_all();

    } catch (...) {}

    state.store(ThreadState::Finished);
    state.notify_all();
}


expected<std::chrono::nanoseconds, const char*> parse_duration(std::string_view input) {
    using namespace std::chrono;
    double value;
    std::from_chars_result r = std::from_chars(
        input.data(),
        input.data() + input.size(),
        value
    );
    if (r.ec != std::errc{}) {
        return unexpected("Invalid number");
    }
    size_t pos = r.ptr - input.data();
    std::string suffix(input.size() - pos, '\0');
    std::transform(input.begin() + pos, input.end(), suffix.begin(), [](char c) { return std::tolower(c); });
    if (suffix == "ns") {
        return duration_cast<nanoseconds>(duration<double, std::nano>(value));
    } else if (suffix == "us") {
        return duration_cast<nanoseconds>(duration<double, std::micro>(value));
    } else if (suffix == "ms") {
        return duration_cast<nanoseconds>(duration<double, std::milli>(value));
    } else if (suffix == "s") {
        return duration_cast<nanoseconds>(duration<double>(value));
    } else if (suffix == "min" || suffix == "m") {
        return duration_cast<nanoseconds>(duration<double, std::ratio<60>>(value));
    } else if (suffix == "h") {
        return duration_cast<nanoseconds>(duration<double, std::ratio<3600>>(value));
    }
    return unexpected("Unknown duration suffix");
}


Threading::Threading(Logger logger, Settings& rSett) : mLogger(logger), mrSett(rSett) {
    auto shortPoolSize = mrSett.createSettingIntegerOr("shortPoolSize", 0);
    if (shortPoolSize < 0) shortPoolSize = 0;
    if (shortPoolSize > UINT32_MAX) shortPoolSize = 0;

    auto threadCountFallback = mrSett.createSettingIntegerOr("threadCountFallback", 4);
    if (threadCountFallback < 0) threadCountFallback = 4;
    if (threadCountFallback > UINT32_MAX) threadCountFallback = 4;

    if (shortPoolSize == 0) {
        shortPoolSize = std::thread::hardware_concurrency();
        if (shortPoolSize == 0) {
            mLogger.warn(TPR_LOG_STYLE_WARN1) << "Failed to get thread count\n";
            shortPoolSize = threadCountFallback;
        }
    }

    auto shortPoolFactor = mrSett.createSettingDoubleOr("shortPoolFactor", 1.0);
    if (shortPoolFactor <= 0.0) shortPoolFactor = 1.0;
    shortPoolSize = std::ceil(shortPoolSize * shortPoolFactor);

    auto shortPoolBias = mrSett.createSettingIntegerOr("shortPoolBias", -1);
    shortPoolSize = std::max(int64_t{1}, shortPoolSize + shortPoolBias);

    mShortPoolSize = shortPoolSize;
    mLogger.info() << "Using short pool size = " << mShortPoolSize << "\n";

    mThreadTotalTimeout = parse_duration(
        mrSett.createSettingStringOr("threadTotalTimeout", "200ms")
    ).value_or(std::chrono::duration_cast<std::chrono::nanoseconds>(200ms));

    mShortThreadMigrationTimeout = parse_duration(
        mrSett.createSettingStringOr("shortThreadMigrationTimeout", "50ms")
    ).value_or(std::chrono::duration_cast<std::chrono::nanoseconds>(50ms));

    mQueue = std::make_unique<Queue>();

    for (uint32_t i = 0; i < mShortPoolSize; i++) {
        auto* thread = mShortPool.emplace_back(std::make_unique<Thread>()).get();
        thread->id = mThreadCounter;
        thread->thread = std::jthread(shortThread, std::ref(*mQueue.get()), std::ref(thread->state));
        mLogger.trace() << "Created short thread " << mThreadCounter << "\n";
        mThreadCounter++;
    }
}


void Threading::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return;

    for (auto it = mDetachedJobs.begin(); it != mDetachedJobs.end();) {
        auto& job = *it->get();
        if (job.state.load() == JobState::Finished) {
            it = mDetachedJobs.erase(it);
        } else {
            it++;
        }
    }

    for (auto& thread : mShortPool) {
        auto now = std::chrono::steady_clock::now();
        if (now - thread->jobBegin.load() > mShortThreadMigrationTimeout) {
            if (thread->state.load() == ThreadState::Working) {
                mLogger.debug() << "Short thread " << thread->id << " is working overtime; moving it to long threads\n";
                thread->thread.request_stop();
                mLongThreads.emplace_back(std::move(thread));
                // creating a new short thread in place of the moved thread
                thread = std::make_unique<Thread>();
                thread->id = mThreadCounter;
                thread->thread = std::jthread(shortThread, std::ref(*mQueue.get()), std::ref(thread->state));
                mLogger.trace() << "Created short thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
    }

    for (auto it = mLongThreads.begin(); it != mLongThreads.end();) {
        auto& thread = *it->get();
        if (thread.state.load() == ThreadState::Finished) {
            mLogger.trace() << "Long thread " << thread.id << " finished\n";
            it = mLongThreads.erase(it);
        } else {
            it++;
        }
    }

}


void Threading::joinAll() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return;
    mUsable = false;
    mLogger.debug() << "Requesting all threads to stop\n";
    for (auto& thread : mShortPool) {
        thread->thread.request_stop();
    }
    for (auto& thread : mLongThreads) {
        thread->thread.request_stop();
    }
    mQueue->notifyAll();
    mLogger.debug() << "Waiting total thread timeout\n";
    std::this_thread::sleep_for(mThreadTotalTimeout);
    mLogger.info() << "Joining threads\n";
    mLogger.debug() << "Joining short threads\n";
    for (auto& thread : mShortPool) {
        if (thread->state.load() != ThreadState::Finished) {
            mLogger.warn(TPR_LOG_STYLE_WARN1) << "Short thread " << thread->id << " didn't stop on time!\n";
        }
        mLogger.trace() << "Calling join on short thread " << thread->id << "\n";
        if (thread->thread.joinable()) thread->thread.join();
    }
    if (!mLongThreads.empty()) mLogger.debug() << "Joining long threads\n";
    for (auto& thread : mLongThreads) {
        if (thread->state.load() != ThreadState::Finished) {
            mLogger.warn(TPR_LOG_STYLE_WARN1) << "Long thread " << thread->id << " didn't stop on time!\n";
        }
        mLogger.trace() << "Calling join on long thread " << thread->id << "\n";
        if (thread->thread.joinable()) thread->thread.join();
    }
    mShortPool.clear();
    mLongThreads.clear();
    mQueue->clear();
    mJobs.clear();
    mDetachedJobs.clear();
}


Threading::~Threading() {
    joinAll();
}


expected<TprJob, TprResult> Threading::createJob(const TprJobCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (!pInfo->func) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (pInfo->dependencyJobCount > 0 && !pInfo->pDependencyJobs) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mUsable) return unexpected(TPR_ERROR_INVALID_OPERATION);

        TprJob handle;

        auto& entry = mJobs.try_emplace(mMapJobCounter).first->second;
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            Job job{&entry};
            
            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mLogger.warn(TPR_LOG_STYLE_WARN1)
                << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";
            
            entry.id = mJobCounter++;
            entry.type = pInfo->type;
            entry.ctx = pInfo->ctx;
            entry.func = pInfo->func;
            entry.priority = priority;
            entry.dependencies.reserve(pInfo->dependencyJobCount);

            for (uint32_t i = 0; i < pInfo->dependencyJobCount; i++) {
                TprJob dependency = pInfo->pDependencyJobs[i];
                if (get_basic_handle_type(dependency) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
                if (get_basic_handle_index(dependency) > mMapJobCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
                auto it = mJobs.find(get_basic_handle_index(dependency));
                if (it == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
                std::lock_guard<std::mutex> dependencyLock(it->second.mutex);
                entry.dependencies.insert(it->second.id);
                it->second.dependentJobs.push_back(job);
            }
            if (pInfo->type == TPR_JOB_TYPE_SHORT_TERM) {
                entry.state.store(JobState::Running);
                if (pInfo->dependencyJobCount == 0) mQueue->push(job);
            } else {
                if (pInfo->dependencyJobCount == 0) {
                    entry.state.store(JobState::Running);
                }
                auto* thread = mLongThreads.emplace_back(std::make_unique<Thread>()).get();
                thread->id = mThreadCounter;
                thread->thread = std::jthread(longThread, job, std::ref(*mQueue.get()), std::ref(thread->state));
                mLogger.trace() << "Created long thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
        handle = construct_basic_handle<TprJob>(mMapJobCounter, 0, handle_type::job);
        mMapJobCounter++;
        return handle;
    } catch (...) {
        return unexpected(TPR_PANIC);
    }
}


TprResult Threading::createDetachedJob(const TprJobCreateInfo* pInfo) noexcept {
    if (!pInfo) return TPR_ERROR_INVALID_VALUE;
    if (!pInfo->func) return TPR_ERROR_INVALID_VALUE;
    if (pInfo->dependencyJobCount > 0 && !pInfo->pDependencyJobs) return TPR_ERROR_INVALID_VALUE;
    try {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mUsable) return TPR_ERROR_INVALID_OPERATION;

        auto& entry = *mDetachedJobs.emplace_back(std::make_unique<JobEntry>()).get();
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            Job job{&entry};

            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mLogger.warn(TPR_LOG_STYLE_WARN1)
                << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";

            entry.id = mJobCounter++;
            entry.type = pInfo->type;
            entry.ctx = pInfo->ctx;
            entry.func = pInfo->func;
            entry.priority = priority;
            entry.dependencies.reserve(pInfo->dependencyJobCount);

            for (uint32_t i = 0; i < pInfo->dependencyJobCount; i++) {
                TprJob dependency = pInfo->pDependencyJobs[i];
                if (get_basic_handle_type(dependency) != handle_type::job) return TPR_ERROR_INVALID_VALUE;
                if (get_basic_handle_index(dependency) > mMapJobCounter) return TPR_ERROR_INVALID_VALUE;
                auto it = mJobs.find(get_basic_handle_index(dependency));
                if (it == mJobs.end()) return TPR_ERROR_INVALID_VALUE;
                std::lock_guard<std::mutex> dependencyLock(it->second.mutex);
                entry.dependencies.insert(it->second.id);
                it->second.dependentJobs.push_back(job);
            }

            if (pInfo->type == TPR_JOB_TYPE_SHORT_TERM) {
                if (pInfo->dependencyJobCount == 0) {
                    entry.state.store(JobState::Running);
                    mQueue->push(job);
                }
            } else {
                if (pInfo->dependencyJobCount == 0) {
                    entry.state.store(JobState::Running);
                }
                auto* thread = mLongThreads.emplace_back(std::make_unique<Thread>()).get();
                thread->id = mThreadCounter;
                thread->thread = std::jthread(longThread, job, std::ref(*mQueue.get()), std::ref(thread->state));
                mLogger.trace() << "Created long thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
    } catch (...) {
        return TPR_PANIC;
    }
    return TPR_SUCCESS;
}


expected<TprBool8, TprResult> Threading::jobFinished(TprJob job) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mUsable) return unexpected(TPR_ERROR_INVALID_OPERATION);

        if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
        if (get_basic_handle_index(job) > mMapJobCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& job = it->second;
        return TprBool8(job.state.load());
    } catch (...) {
        return unexpected(TPR_PANIC);
    }
}


expected<uint32_t, TprResult> Threading::getJobID(TprJob job) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mUsable) return unexpected(TPR_ERROR_INVALID_OPERATION);

        if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
        if (get_basic_handle_index(job) > mMapJobCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& job = it->second;
        return job.id;
    } catch (...) {
        return unexpected(TPR_PANIC);
    }
}


void Threading::joinJob(TprJob job) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mUsable) return;

        if (get_basic_handle_type(job) != handle_type::job) return;
        if (get_basic_handle_index(job) > mMapJobCounter) return;
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return;
        auto& job = it->second;
        job.state.wait(JobState::WaitingDependencies);
        job.state.wait(JobState::Running);
        mJobs.erase(it);
    } catch (...) {
        return;
    }
}
