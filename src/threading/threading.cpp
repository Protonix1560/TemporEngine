
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


void shortThread(std::stop_token stop, std::reference_wrapper<Queue> queueWrapper, std::reference_wrapper<std::atomic<bool>> runningWrapper) {
    Queue& queue = queueWrapper.get();
    std::atomic<bool>& running = runningWrapper.get();
    while (!stop.stop_requested()) {
        auto value = queue.tryPull(std::chrono::duration_cast<std::chrono::nanoseconds>(100ms));
        if (value.has_value()) {
            Job job = std::move(value.value());
            std::lock_guard<std::mutex> lock(job.entry->mutex);
            job.func(job.ctx);
            if (!job.entry->dependentJobs.empty()) {
                for (auto& dependent : job.entry->dependentJobs) {
                    std::lock_guard<std::mutex> dependentLock(dependent.entry->mutex);
                    auto it = dependent.entry->dependencies.find(job.entry->id);
                    if (it != dependent.entry->dependencies.end()) {
                        dependent.entry->dependencies.erase(it);
                    }
                    if (dependent.entry->dependencies.empty()) {
                        queue.push(dependent);
                    }
                }
            }
            job.entry->finished.store(true);
            job.entry->finished.notify_all();
        }
    }
    running = false;
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

    mQueue = std::make_unique<Queue>();

    for (uint32_t i = 0; i < mShortPoolSize; i++) {
        auto* thread = mShortPool.emplace_back(std::make_unique<Thread>()).get();
        thread->thread = std::jthread(shortThread, std::ref(*mQueue.get()), std::ref(thread->running));
    }
}


void Threading::update() {



}


Threading::~Threading() {
    mrLogger << logPrxThrd() << "Requesting all threads to stop\n";
    for (auto& thread : mShortPool) {
        thread->thread.request_stop();
    }
    mQueue->notifyAll();
    mrLogger << logPrxThrd() << "Waiting total thread timeout\n";
    std::this_thread::sleep_for(mThreadTotalTimeout);
    mrLogger << logPrxThrd() << "Joining threads\n";
    for (uint32_t i = 0; i < mShortPool.size(); i++) {
        auto& thread = mShortPool[i];
        if (thread->running.load()) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxThrd() << "Thread " << i << " didn't stop on time!\n";
        }
        mrLogger.trace() << logPrxThrd() << "Calling join on thread " << i << "\n";
        if (thread->thread.joinable()) thread->thread.join();
    }
}


expected<TprJob, TprResult> Threading::createJob(const TprJobCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_INVALID_VALUE);
    if (!pInfo->func) return unexpected(TPR_INVALID_VALUE);
    if (pInfo->dependencyJobCount > 0 && !pInfo->pDependencyJobs) return unexpected(TPR_INVALID_VALUE);
    TprJob handle;
    try {
        auto& entry = mJobs.try_emplace(mMapJobCounter).first->second;
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mrLogger.warn(TPR_LOG_STYLE_WARN1)
                << logPrxThrd() << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";
            Job job{pInfo->func, pInfo->ctx, &entry, priority};
            entry.id = mJobCounter++;
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
            if (pInfo->dependencyJobCount == 0) mQueue->push(job);
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
    try {
        auto& entry = *mDetachedJobs.emplace_back(std::make_unique<JobEntry>()).get();
        {
            std::lock_guard<std::mutex> lock(entry.mutex);
            float priority = std::clamp(pInfo->priority, 0.0f, 1.0f);
            if (priority != pInfo->priority) mrLogger.warn(TPR_LOG_STYLE_WARN1)
                << logPrxThrd() << "Clamping out-of-bounds priority " << pInfo->priority << " to " << priority << "\n";
            Job job{pInfo->func, pInfo->ctx, &entry, priority};
            entry.id = mJobCounter++;
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
            if (pInfo->dependencyJobCount == 0) mQueue->push(job);
        }
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


expected<TprBool8, TprResult> Threading::jobFinished(TprJob job) noexcept {
    try {
        if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_INVALID_VALUE);
        if (get_basic_handle_index(job) > mMapJobCounter) return unexpected(TPR_INVALID_VALUE);
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return unexpected(TPR_INVALID_VALUE);
        auto& job = it->second;
        return TprBool8(job.finished.load());
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}


void Threading::joinJob(TprJob job) noexcept {
    try {
        if (get_basic_handle_type(job) != handle_type::job) return;
        if (get_basic_handle_index(job) > mMapJobCounter) return;
        auto it = mJobs.find(get_basic_handle_index(job));
        if (it == mJobs.end()) return;
        auto& job = it->second;
        job.finished.wait(false);
        mJobs.erase(it);
    } catch (...) {
        return;
    }
}
