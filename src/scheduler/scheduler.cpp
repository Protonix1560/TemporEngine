
#include "scheduler.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "settings.hpp"

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
#include <type_traits>


using namespace std::chrono_literals;


template<typename T>
concept boolean_testable = requires(T&& t) {
    requires std::convertible_to<T, bool>;
    { !std::forward<T>(t) } -> std::convertible_to<bool>;
};

template<typename T, typename U>
concept comparable = requires(const T& t, const U& u) {
    { t == u } -> std::convertible_to<bool>;
    { u == t } -> std::convertible_to<bool>;
};

template<typename T, typename U>
bool same_weak_object(const std::weak_ptr<T>& a, const std::weak_ptr<U>& b) {
    if constexpr (comparable<const typename std::shared_ptr<T>::element_type&, const typename std::shared_ptr<U>::element_type&>) {
        auto sa = a.lock();
        auto sb = b.lock();
        return sa == sb;
    } else {
        return false;
    }
}

template <std::integral T, std::integral Min, std::integral Max, std::integral Def>
T bounded_or(T value, Min min, Max max, Def def) {
    using common = std::common_type_t<T, Min, Max, Def>;
    static_assert(std::is_same_v<common, T> || std::is_same_v<T, std::common_type_t<T, common>>, "bounded_or: type conversion would lose information");
    if (static_cast<common>(value) > static_cast<common>(max) || 
        static_cast<common>(value) < static_cast<common>(min)) {
        return static_cast<T>(def);
    }
    return value;
}

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

expected<std::chrono::steady_clock::duration, TprResult> parseDuration(std::string_view input) {
    auto exp = parse_duration(input);
    if (exp) return exp.value();
    switch (exp.error()) {
        case -1:
        case -2:
            return unexpected(TPR_ERROR_INVALID_VALUE);
        default:
            return unexpected(TPR_PANIC);
    }
}


Scheduler::Scheduler(Logger logger, Settings& rSett) : mLogger(logger), mrSett(rSett), mTimeBegin(std::chrono::steady_clock::now()) {
//     if (logger.sink()->colourEnabled()) {
//         mSpamLogger = mLogger.derive("\e[44mspam:\e[0m ");
//     } else {
//         mSpamLogger = mLogger.derive("spam: ");
//     }
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
        .and_then([&](const auto& s) { return parseDuration(s); });
    if (!shortThreadMigrationTimeoutExp && shortThreadMigrationTimeoutExp.error() == TPR_PANIC) return TPR_PANIC;
    mShortThreadMigrationTimeout = shortThreadMigrationTimeoutExp.value_or(50ms);

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

    return TPR_SUCCESS;
}

void Scheduler::shutdown() {
    std::lock_guard<std::mutex> lock(mMutex);
    mInitialized = false;
    for (auto [id, thread] : mThreads) {
        thread->thread.request_stop();
    }
    for (auto [id, thread] : mThreads) {
        thread->thread.join();
    }
}

Scheduler::~Scheduler() {}


void Scheduler::shortThread(std::stop_token stop, std::shared_ptr<Thread> thread) noexcept {
    while (!stop.stop_requested()) {
        auto launch = mQueue.pull(stop);
        if (!launch.has_value()) break;
        mSpamLogger.debug() << "Processing Job " << get_basic_handle_index(launch->entry->mainHandle)
            << " in thread " << thread->id;
        processLaunch(launch.value());
    }
}

void Scheduler::processLaunch(JobLaunch launch) {

    std::vector<JobLaunch> plannedLaunches;

    if (launch.entry->destroyed.load()) return;

    if (launch.entry->destructionPended.load()) {
        std::lock_guard<std::mutex> schedLock(mMutex);
        launch.entry->destroyed.store_true();
        // entry->capabilities can't be accessed while Scheduler::mMutex is locked
        for (auto capability : launch.entry->capabilities) {
            mJobs.erase(capability);
            mLogger.trace() << "Destroyed job capability " << capability;
        }
        mJobs.erase(get_basic_handle_index(launch.entry->mainHandle));
        mLogger.trace() << "Destroyed job " << get_basic_handle_index(launch.entry->mainHandle);

    } else if (launch.entry->function) {
        mSpamLogger.debug() << "Launching Job " << get_basic_handle_index(launch.entry->mainHandle);

        threadInfo.currentJob = launch.entry->mainHandle;
        launch.entry->function(launch.entry->context, launch.entry->mainHandle);
        // Great job!
        threadInfo.currentJob.reset();

        mSpamLogger.debug() << "Job " << get_basic_handle_index(launch.entry->mainHandle) << " finished";
    }

    {
        std::lock_guard<std::mutex> launchJobLock(launch.entry->mutex);

        launch.entry->usage += launch.entry->dependents.size();
        mSpamLogger.debug() << "Incrementing (++) Job " << get_basic_handle_index(launch.entry->mainHandle)
            << "'s usage to " << launch.entry->usage << " because of dependent jobs";

        for (auto dependent : launch.entry->dependents) {
            std::lock_guard<std::mutex> dendentLock(dependent->mutex);
            if (launch.entry->destructionPended.load()) {
                dependent->destructionPended.store_true();
                mSpamLogger.debug() << "Pended destruction of job " << get_basic_handle_index(dependent->mainHandle);
            }
            dependent->countdown--;
            if (dependent->countdown == 0) {
                mSpamLogger.debug() << "Job " << get_basic_handle_index(dependent->mainHandle) << "'s countdown is 0";
                dependent->countdown = dependent->dependencies.size();
                plannedLaunches.push_back({dependent, std::chrono::steady_clock::now()});
            }
        }

        // decrementing here to cancel incrementation in JobQueue::pull
        launch.entry->usage--;
    }

    for (const auto& launch : plannedLaunches) {
        mSpamLogger.debug() << "Pushing Job " << get_basic_handle_index(launch.entry->mainHandle) << " to queue at "
            << std::chrono::duration_cast<std::chrono::nanoseconds>(launch.timepoint - mTimeBegin).count() << " ns";
        mQueue.push(launch);
    }

    for (auto weak : launch.entry->dependencies) {
        auto dependency = weak.lock();
        if (dependency) {
            std::lock_guard<std::mutex> dependencyLock(dependency->mutex);
            dependency->usage--;
            mSpamLogger.debug() << "Decrementing (--) Job " << get_basic_handle_index(dependency->mainHandle)
                << "'s usage to " << dependency->usage << " because of dependent job " << get_basic_handle_index(launch.entry->mainHandle);
            if (launch.entry->destructionPended.load()) {
                auto it = std::ranges::find(dependency->dependents, launch.entry);
                if (it != dependency->dependents.end()) dependency->dependents.erase(it);
            }
        }
    }

    // Some jobs' usages might have changed
    mQueue.notify();
}


expected<TprJob, TprResult> Scheduler::createJob(const TprJobCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);
    switch (pInfo->duration) {
        case TPR_JOB_DURATION_SHORT: case TPR_JOB_DURATION_LONG: break;
        default: return unexpected(TPR_ERROR_INVALID_VALUE);
    }
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialized);
    try {
        std::shared_ptr<JobEntry> entry;
        TprJob h = construct_basic_handle<TprJob>(mJobCounter, 0, handle_type::job);

        switch (pInfo->triggerType) {
            case TPR_JOB_TRIGGER_TYPE_DEPENDENCIES: {
                if (!pInfo->pDependencies) return unexpected(TPR_ERROR_INVALID_VALUE);
                if (pInfo->dependencyCount == 0) return unexpected(TPR_ERROR_INVALID_VALUE);

                std::vector<std::shared_ptr<JobEntry>> deps;
                deps.reserve(pInfo->dependencyCount);
                for (auto it = pInfo->pDependencies; it != pInfo->pDependencies + pInfo->dependencyCount; it++) {
                    TprJob h = *it;
                    if (get_basic_handle_type(h) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
                    auto handleIt = mJobs.find(get_basic_handle_index(h));
                    if (handleIt == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

                    deps.push_back(handleIt->second.entry);
                }

                entry = std::make_shared<JobEntry>(*pInfo, h, deps.begin(), deps.end());

                for (auto dep : deps) {
                    std::lock_guard<std::mutex> lock(dep->mutex);
                    dep->dependents.push_back(entry);
                }
                break;
            }

            case TPR_JOB_TRIGGER_TYPE_SCHEDULE: {
                entry = std::make_shared<JobEntry>(*pInfo, h);
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
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

expected<TprJob, TprResult> Scheduler::createJobCapability(TprJob job, TprJobCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialized);
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        std::lock_guard<std::mutex> lock(handleIt->second.entry->mutex);
        mJobs.insert_or_assign(mJobCounter, JobHandle{handleIt->second.capability & mask, handleIt->second.entry});
        TprJob h = construct_basic_handle<TprJob>(mJobCounter, 0, handle_type::job);
        handleIt->second.entry->capabilities.push_back(mJobCounter);
        mLogger.trace() << "Created job capability " << mJobCounter << " for job " << handleIt->first;
        mJobCounter++;

        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

TprResult Scheduler::scheduleJob(TprJob job, uint64_t timepoint) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialized);
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return TPR_ERROR_INVALID_VALUE;
        auto entry = handleIt->second.entry;
        if (!entry->dependencies.empty()) return TPR_ERROR_INVALID_OPERATION;
        mQueue.push({entry, mTimeBegin + std::chrono::nanoseconds(timepoint)});
        mSpamLogger.debug() << "Scheduled Job " << get_basic_handle_index(entry->mainHandle) << " to queue at " << timepoint << " ns";
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

void Scheduler::pendJobDestruction(TprJob job) noexcept {
    if (get_basic_handle_type(job) != handle_type::job) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialized);
    try {
        auto handleIt = mJobs.find(get_basic_handle_index(job));
        if (handleIt == mJobs.end()) return;
        auto entry = handleIt->second.entry;
        std::lock_guard<std::mutex> entryLock(entry->mutex);
        entry->destructionPended.store_true();
        mQueue.push(JobLaunch{entry, {}});
        mSpamLogger.debug() << "Pended destruction of job " << get_basic_handle_index(job);

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return;
    }
}


uint64_t Scheduler::now() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - mTimeBegin).count();
}

std::chrono::steady_clock::time_point Scheduler::timeBegin() {
    return mTimeBegin;
}

