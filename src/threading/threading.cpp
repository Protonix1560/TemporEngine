
#include "threading.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "settings.hpp"

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
                    job.entry->func(job.entry->ctx);
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
            job.entry->func(job.entry->ctx);
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


Threading::Threading(Logger& rLogger, Settings& rSett) : mrLogger(rLogger), mrSett(rSett) {
    auto shortPoolSize = mrSett.createSettingIntegerOr("shortThreadPoolSize", 0);
    if (shortPoolSize < 0) shortPoolSize = 0;
    if (shortPoolSize > UINT32_MAX) shortPoolSize = 0;

    auto threadCountFallback = mrSett.createSettingIntegerOr("threadCountFallback", 4);
    if (threadCountFallback < 0) threadCountFallback = 4;
    if (threadCountFallback > UINT32_MAX) threadCountFallback = 4;

    if (shortPoolSize == 0) {
        shortPoolSize = std::thread::hardware_concurrency();
        if (shortPoolSize == 0) {
            mrLogger.warn(TPR_LOG_STYLE_WARN1) << logPrxThrd() << "Failed to get thread count\n";
            shortPoolSize = threadCountFallback;
        }
    }

    auto threadCountFactor = mrSett.createSettingDoubleOr("threadCountFactor", 1.0);
    if (threadCountFactor <= 0.0) threadCountFactor = 1.0;
    shortPoolSize = std::ceil(shortPoolSize * threadCountFactor);

    auto threadCountBias = mrSett.createSettingIntegerOr("threadCountBias", -1);
    shortPoolSize = std::max(int64_t{1}, shortPoolSize + threadCountBias);

    mShortPoolSize = shortPoolSize;
    mrLogger << logPrxThrd() << "Using short pool size = " << mShortPoolSize << "\n";

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
        mrLogger.trace() << logPrxThrd() << "Created short thread " << mThreadCounter << "\n";
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
                mrLogger.debug() << logPrxThrd() << "Short thread " << thread->id << " is working overtime; moving it to long threads\n";
                thread->thread.request_stop();
                mLongThreads.emplace_back(std::move(thread));
                // creating a new short thread in place of the moved thread
                thread = std::make_unique<Thread>();
                thread->id = mThreadCounter;
                thread->thread = std::jthread(shortThread, std::ref(*mQueue.get()), std::ref(thread->state));
                mrLogger.trace() << logPrxThrd() << "Created short thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
    }

    for (auto it = mLongThreads.begin(); it != mLongThreads.end();) {
        auto& thread = *it->get();
        if (thread.state.load() == ThreadState::Finished) {
            mrLogger.trace() << logPrxThrd() << "Long thread " << thread.id << " finished\n";
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
    mrLogger.debug() << logPrxThrd() << "Requesting all threads to stop\n";
    for (auto& thread : mShortPool) {
        thread->thread.request_stop();
    }
    for (auto& thread : mLongThreads) {
        thread->thread.request_stop();
    }
    mQueue->notifyAll();
    mrLogger.debug() << logPrxThrd() << "Waiting total thread timeout\n";
    std::this_thread::sleep_for(mThreadTotalTimeout);
    mrLogger << logPrxThrd() << "Joining threads\n";
    mrLogger.debug() << logPrxThrd() << "Joining short threads\n";
    for (auto& thread : mShortPool) {
        if (thread->state.load() != ThreadState::Finished) {
            mrLogger.warn(TPR_LOG_STYLE_WARN1) << logPrxThrd() << "Short thread " << thread->id << " didn't stop on time!\n";
        }
        mrLogger.trace() << logPrxThrd() << "Calling join on short thread " << thread->id << "\n";
        if (thread->thread.joinable()) thread->thread.join();
    }
    if (!mLongThreads.empty()) mrLogger.debug() << logPrxThrd() << "Joining long threads\n";
    for (auto& thread : mLongThreads) {
        if (thread->state.load() != ThreadState::Finished) {
            mrLogger.warn(TPR_LOG_STYLE_WARN1) << logPrxThrd() << "Long thread " << thread->id << " didn't stop on time!\n";
        }
        mrLogger.trace() << logPrxThrd() << "Calling join on long thread " << thread->id << "\n";
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
    if (!pInfo) return unexpected(TPR_INVALID_VALUE);
    if (!pInfo->func) return unexpected(TPR_INVALID_VALUE);
    if (pInfo->dependencyJobCount > 0 && !pInfo->pDependencyJobs) return unexpected(TPR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return unexpected(TPR_CONTRACT_VIOLATION);
    TprJob handle;
    try {
        auto& entry = mJobs.try_emplace(mMapJobCounter).first->second;
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            Job job{&entry};
            
            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mrLogger.warn(TPR_LOG_STYLE_WARN1)
                << logPrxThrd() << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";
            
            entry.id = mJobCounter++;
            entry.type = pInfo->type;
            entry.ctx = pInfo->ctx;
            entry.func = pInfo->func;
            entry.priority = priority;
            entry.dependencies.reserve(pInfo->dependencyJobCount);

            for (uint32_t i = 0; i < pInfo->dependencyJobCount; i++) {
                TprJob dependency = pInfo->pDependencyJobs[i];
                if (get_basic_handle_type(dependency) != handle_type::job) return unexpected(TPR_INVALID_VALUE);
                if (get_basic_handle_index(dependency) > mMapJobCounter) return unexpected(TPR_INVALID_VALUE);
                auto it = mJobs.find(get_basic_handle_index(dependency));
                if (it == mJobs.end()) return unexpected(TPR_INVALID_VALUE);
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
                mrLogger.trace() << logPrxThrd() << "Created long thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
        handle = construct_basic_handle<TprJob>(mMapJobCounter, 0, handle_type::job);
        mMapJobCounter++;
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
    return handle;
}


TprResult Threading::createDetachedJob(const TprJobCreateInfo* pInfo) noexcept {
    if (!pInfo) return TPR_INVALID_VALUE;
    if (!pInfo->func) return TPR_INVALID_VALUE;
    if (pInfo->dependencyJobCount > 0 && !pInfo->pDependencyJobs) return TPR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return TPR_CONTRACT_VIOLATION;
    try {
        auto& entry = *mDetachedJobs.emplace_back(std::make_unique<JobEntry>()).get();
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            Job job{&entry};

            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mrLogger.warn(TPR_LOG_STYLE_WARN1)
                << logPrxThrd() << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";

            entry.id = mJobCounter++;
            entry.type = pInfo->type;
            entry.ctx = pInfo->ctx;
            entry.func = pInfo->func;
            entry.priority = priority;
            entry.dependencies.reserve(pInfo->dependencyJobCount);

            for (uint32_t i = 0; i < pInfo->dependencyJobCount; i++) {
                TprJob dependency = pInfo->pDependencyJobs[i];
                if (get_basic_handle_type(dependency) != handle_type::job) return TPR_INVALID_VALUE;
                if (get_basic_handle_index(dependency) > mMapJobCounter) return TPR_INVALID_VALUE;
                auto it = mJobs.find(get_basic_handle_index(dependency));
                if (it == mJobs.end()) return TPR_INVALID_VALUE;
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
                mrLogger.trace() << logPrxThrd() << "Created long thread " << mThreadCounter << "\n";
                mThreadCounter++;
            }
        }
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


expected<TprBool8, TprResult> Threading::jobFinished(TprJob job) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return unexpected(TPR_CONTRACT_VIOLATION);
    try {
        if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_INVALID_VALUE);
        if (get_basic_handle_index(job) > mMapJobCounter) return unexpected(TPR_INVALID_VALUE);
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return unexpected(TPR_INVALID_VALUE);
        auto& job = it->second;
        return TprBool8(job.state.load());
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}


void Threading::joinJob(TprJob job) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mUsable) return;
    try {
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
