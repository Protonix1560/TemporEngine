
#include "scheduler.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "settings.hpp"
#include "log_entry.hpp"

#include "thread_job_info.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string_view>
#include <thread>
#include <cmath>

#ifdef POSIX
    #include "pthread.h"
#endif

#ifdef WINDOWS
    #include "windows.h"
#endif

using namespace std::chrono_literals;

template<typename T>
concept boolean_testable = requires(T&& t) {
    requires std::convertible_to<T, bool>;
    { !std::forward<T>(t) } -> std::convertible_to<bool>;
};

expected<std::chrono::steady_clock::duration, int> parse_duration(std::string_view input) {
    using namespace std::chrono;
    double value;
    std::from_chars_result r = std::from_chars(
        input.data(),
        input.data() + input.size(),
        value
    );
    if (r.ec != std::errc{}) {
        return unexpected(-1);  // Invalid number
    }
    size_t pos = r.ptr - input.data();
    std::string suffix(input.size() - pos, '\0');
    std::transform(input.begin() + pos, input.end(), suffix.begin(), [](char c) { return std::tolower(c); });
    if (suffix == "ns") {
        return duration_cast<steady_clock::duration>(duration<double, std::nano>(value));
    } else if (suffix == "us") {
        return duration_cast<steady_clock::duration>(duration<double, std::micro>(value));
    } else if (suffix == "ms") {
        return duration_cast<steady_clock::duration>(duration<double, std::milli>(value));
    } else if (suffix == "s") {
        return duration_cast<steady_clock::duration>(duration<double>(value));
    } else if (suffix == "min" || suffix == "m") {
        return duration_cast<steady_clock::duration>(duration<double, std::ratio<60>>(value));
    } else if (suffix == "h") {
        return duration_cast<steady_clock::duration>(duration<double, std::ratio<3600>>(value));
    }
    return unexpected(-2);  // Unknown duration suffix
}

expected<std::chrono::steady_clock::duration, TprResult> parseDuration(std::string_view input, std::atomic<TprResult>& rRunResult) {
    auto exp = parse_duration(input);
    if (exp) return exp.value();
    switch (exp.error()) {
        case -1:
        case -2:
            return unexpected(TPR_ERROR_INVALID_VALUE);
        default:
            rRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
    }
}


Scheduler::Scheduler(Logger logger, Settings& rSett, std::atomic<TprResult>& rRunResult)
    : mLogger(logger), mrSett(rSett), mTimeBegin(std::chrono::steady_clock::now()), mrRunResult(rRunResult) {
    // mSpamLogger = mLogger;
    // mSpamLogger.prefix() << marker_background4_dark << "spam:" << marker_no_background << " ";
}


TprResult Scheduler::init() {
    auto shortPoolSizeExp = mrSett.createSetting(mrSett.getRoot(), "shortPoolSize")
        .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
    if (!shortPoolSizeExp && shortPoolSizeExp.error() == TPR_PANIC) return TPR_PANIC;
    uint64_t shortPoolSize = bounded_or(shortPoolSizeExp.value_or(0), 0, UINT32_MAX, 0);

    auto threadCountFallbackExp = mrSett.createSetting(mrSett.getRoot(), "threadCountFallback")
        .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
    if (!threadCountFallbackExp && threadCountFallbackExp.error() == TPR_PANIC) return TPR_PANIC;
    uint64_t threadCountFallback = bounded_or(threadCountFallbackExp.value_or(4), 0, UINT32_MAX, 4);

    if (shortPoolSize == 0) {
        shortPoolSize = std::thread::hardware_concurrency();
        if (shortPoolSize == 0) {
            mLogger.warn() << "Failed to get thread count; using threadCountFallback";
            shortPoolSize = threadCountFallback;
        }
    }

    auto shortPoolFactorExp = mrSett.createSetting(mrSett.getRoot(), "shortPoolFactor")
        .and_then([&](auto s) { return mrSett.getSettingDouble(s); });
    if (!shortPoolFactorExp && shortPoolFactorExp.error() == TPR_PANIC) return TPR_PANIC;
    double shortPoolFactor = shortPoolFactorExp.value_or(1.0);
    if (shortPoolFactor <= 0.0) shortPoolFactor = 1.0;

    auto shortPoolBiasExp = mrSett.createSetting(mrSett.getRoot(), "shortPoolBias")
        .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
    if (!shortPoolBiasExp && shortPoolBiasExp.error() == TPR_PANIC) return TPR_PANIC;
    int64_t shortPoolBias = shortPoolBiasExp.value_or(-1);

    shortPoolSize = std::max(int64_t{1}, static_cast<int64_t>(std::ceil(shortPoolSize * shortPoolFactor)) + shortPoolBias);
    if (shortPoolSize > UINT32_MAX) mLogger.warn() << "Short pool size exceeded UINT32_MAX; casting it down will result in different number";
    mShortPoolSize = shortPoolSize;
    mLogger.debug() << "Using short pool size = " << mShortPoolSize;

    auto shortThreadMigrationTimeoutExp = mrSett.createSetting(mrSett.getRoot(), "shortThreadMigrationTimeout")
        .and_then([&](auto s) { return mrSett.getSettingString(s); })
        .and_then([&](const auto& s) { return parseDuration(s, mrRunResult); });
    if (!shortThreadMigrationTimeoutExp && shortThreadMigrationTimeoutExp.error() == TPR_PANIC) return TPR_PANIC;
    mShortThreadMigrationTimeout = shortThreadMigrationTimeoutExp.value_or(50ms);

    auto threadPullWaitTimeoutExp = mrSett.createSetting(mrSett.getRoot(), "threadPullWaitTimeout")
        .and_then([&](auto s) { return mrSett.getSettingString(s); })
        .and_then([&](const auto& s) { return parseDuration(s, mrRunResult); });
    if (!threadPullWaitTimeoutExp && threadPullWaitTimeoutExp.error() == TPR_PANIC) return TPR_PANIC;
    mThreadPullWaitTimeout = threadPullWaitTimeoutExp.value_or(50ms);

    for (uint32_t i = 0; i < mShortPoolSize; i++) {
        auto thread = mThreads.insert_or_assign(mThreadCounter, std::make_shared<Thread>(mThreadCounter)).first->second;
        thread->thread = std::jthread([this, thread](std::stop_token stop) {
            thread->ready.wait(false);
            shortThread(stop, thread);
        });
        thread->ready.store_true();
        thread->ready.notify_all();
        mLogger.trace() << "Created short thread " << mThreadCounter;
        mThreadCounter++;
    }

    mInitialised = true;

    return TPR_SUCCESS;
}

void Scheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mInitialised = false;
    }
    for (auto [id, thread] : mThreads) {
        thread->thread.request_stop();
        mLogger.trace() << "Requiested thread " << thread->id << " to stop";
    }
    mQueue.notify();
    for (auto [id, thread] : mThreads) {
        mLogger.trace() << "Joining thread " << thread->id;
        thread->thread.join();
    }
}

Scheduler::~Scheduler() {}


void Scheduler::shortThread(std::stop_token stop, std::shared_ptr<Thread> thread) noexcept {
    std::string name = std::format("short{}", thread->id);
    #ifdef POSIX
        pthread_setname_np(pthread_self(), name.c_str());
    #endif
    #ifdef WINDOWS
        SetThreadDescription(GetCurrentThread(), name.c_str());
    #endif
    while (!stop.stop_requested()) {
        mSpamLogger.debug() << "Thread " << marker_italic << thread->id << marker_no_italic << " calls queue pull";
        auto launch = mQueue.pull(stop, mThreadPullWaitTimeout);
        if (!launch.has_value()) {
            mSpamLogger.debug() << "Thread " << marker_italic << thread->id << marker_no_italic << " returned from queue pull with std::nullopt";
            break;
        }
        mSpamLogger.debug() << "Thread " << marker_italic << thread->id << marker_no_italic << " returned from queue pull with a launch";
        mSpamLogger.debug() << "Thread " << marker_italic << thread->id << marker_no_italic << " processes Job "
            << marker_underline << get_basic_handle_index(launch->meta.handle);
        processLaunch(launch.value());
    }
}

void Scheduler::processLaunch(JobLaunch launch) {

    std::vector<JobLaunch> plannedLaunches;

    if (launch.meta.entry->invalidated.load()) return;
    if (launch.meta.entry->destroyed.load()) return;

    if (launch.meta.entry->destructionPended.load()) {
        launch.meta.entry->destroyed.store_true();

    } else if (launch.meta.entry->function) {
        mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(launch.meta.handle) << marker_no_underline << " is launching";

        threadInfo.currentJob = launch.meta.handle;
        launch.meta.entry->function(launch.meta.entry->context);
        // Great job!
        threadInfo.currentJob.reset();

        mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(launch.meta.handle) << marker_no_underline << " is finished";
    }

    {
        std::lock_guard<std::mutex> launchJobLock(launch.meta.entry->mutex);
        launch.meta.entry->usage += launch.meta.entry->dependents.size();
        if (!launch.meta.entry->dependents.empty()) {
            mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(launch.meta.handle)
                << marker_no_underline << "'s usage is incremented to " << launch.meta.entry->usage << " because of dependent jobs";
            for (auto dependent : launch.meta.entry->dependents) {
                std::lock_guard<std::mutex> dependentLock(dependent.entry->mutex);
                dependent.entry->countdown--;
                if (dependent.entry->countdown == 0) {
                    mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(dependent.handle)
                        << marker_no_underline << "'s countdown is 0";
                    dependent.entry->countdown = dependent.entry->dependencies.size();
                    plannedLaunches.push_back({dependent, std::chrono::steady_clock::now()});
                }
            }
        }
        // decrementing here to cancel incrementation in JobQueue::pull
        launch.meta.entry->usage--;
    }

    for (const auto& launch : plannedLaunches) {
        mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(launch.meta.handle) << marker_no_underline
            << " is pushed to queue at " << std::chrono::duration_cast<std::chrono::nanoseconds>(launch.timepoint - mTimeBegin).count() << " ns";
        mQueue.push(launch);
    }

    for (auto depMeta : launch.meta.entry->dependencies) {
        auto dependency = depMeta.entry.lock();
        if (dependency) {
            std::lock_guard<std::mutex> dependencyLock(dependency->mutex);
            dependency->usage--;
            mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(depMeta.handle) << marker_no_underline
                << "'s usage is decremented to " << dependency->usage << " because of dependent Job " << marker_underline << get_basic_handle_index(launch.meta.handle);
            if (launch.meta.entry->destructionPended.load()) {
                auto it = std::ranges::find_if(dependency->dependents, [&](const auto& dep) { return dep.entry == launch.meta.entry; });
                if (it != dependency->dependents.end()) dependency->dependents.erase(it);
                mSpamLogger.debug() << "Removed Job " << marker_underline << get_basic_handle_index(launch.meta.handle) << marker_no_underline
                    << " from Job " << marker_underline << get_basic_handle_index(depMeta.handle) << marker_no_underline << "'s dependents";
            }
        }
    }

    // Some jobs' usages might have changed
    mQueue.notify();
}


expected<TprJob, TprResult> Scheduler::createJob(const TprJobCreateInfo& info) noexcept {
    switch (info.duration) {
        case TPR_JOB_DURATION_SHORT: case TPR_JOB_DURATION_LONG: break;
        default: return unexpected(TPR_ERROR_INVALID_VALUE);
    }
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mInitialised) return unexpected(TPR_ERROR_INVALID_OPERATION);
    try {
        std::shared_ptr<JobEntry> entry;
        TprJob h = construct_basic_handle<TprJob>(mJobCounter, 0, handle_type::job);

        switch (info.triggerType) {
            case TPR_JOB_TRIGGER_TYPE_DEPENDENCIES: {
                if (!info.pDependencies) return unexpected(TPR_ERROR_INVALID_VALUE);
                if (info.dependencyCount == 0) return unexpected(TPR_ERROR_INVALID_VALUE);

                std::vector<SharedJobMeta> deps;
                deps.reserve(info.dependencyCount);
                for (auto it = info.pDependencies; it != info.pDependencies + info.dependencyCount; it++) {
                    TprJob h = *it;
                    if (get_basic_handle_type(h) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
                    auto handleIt = mJobs.find(get_basic_handle_index(h));
                    if (handleIt == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

                    deps.push_back({handleIt->second.entry, h});
                }

                entry = std::make_shared<JobEntry>(info, deps.begin(), deps.end());
                entry->countdown = entry->dependencies.size();

                for (auto dep : deps) {
                    std::lock_guard<std::mutex> lock(dep.entry->mutex);
                    dep.entry->dependents.push_back({entry, h});
                }
                break;
            }

            case TPR_JOB_TRIGGER_TYPE_SCHEDULE: {
                entry = std::make_shared<JobEntry>(info);
                break;
            }

            default: return unexpected(TPR_ERROR_INVALID_VALUE);
        }
        mJobs.insert_or_assign(mJobCounter, JobHandle{.entry = entry});
        mLogger.trace() << "Created job " << mJobCounter;
        mJobCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprJob, TprResult> Scheduler::createJobCapability(TprJob job, TprJobCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mInitialised) return unexpected(TPR_ERROR_INVALID_OPERATION);
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        std::lock_guard<std::mutex> lock(handleIt->second.entry->mutex);
        mJobs.insert_or_assign(mJobCounter, JobHandle{handleIt->second.capability & mask, handleIt->second.entry});
        TprJob h = construct_basic_handle<TprJob>(mJobCounter, 0, handle_type::job);
        mLogger.trace() << "Created Job capability " << mJobCounter << " for Job " << handleIt->first;
        mJobCounter++;

        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

TprResult Scheduler::scheduleJob(TprJob job, uint64_t timepoint) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mInitialised) return TPR_ERROR_INVALID_OPERATION;
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return TPR_ERROR_INVALID_VALUE;
        auto entry = handleIt->second.entry;
        if (!entry->dependencies.empty()) return TPR_ERROR_INVALID_OPERATION;
        if (entry->invalidated.load()) return TPR_ERROR_INVALID_OPERATION;
        mQueue.push({{entry, job}, mTimeBegin + std::chrono::nanoseconds(timepoint)});
        mSpamLogger.debug() << "Job " << marker_underline << get_basic_handle_index(job) << marker_no_underline
            << " is scheduled to queue at " << timepoint << " ns";
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

void Scheduler::invalidateJob(TprJob job) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mInitialised) return;
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return;
        auto entry = handleIt->second.entry;
        entry->invalidated.store_true();

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}

void Scheduler::destroyJob(TprJob job) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return;
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mInitialised) return;
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return;
        auto entry = handleIt->second.entry;
        entry->destructionPended.store_true();
        mQueue.push(JobLaunch{{entry, job}, {}});
        mSpamLogger.debug() << "Pended destruction of job " << marker_underline << get_basic_handle_index(job);

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}


uint64_t Scheduler::now() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - mTimeBegin).count();
}

std::chrono::steady_clock::time_point Scheduler::timeBegin() {
    return mTimeBegin;
}

