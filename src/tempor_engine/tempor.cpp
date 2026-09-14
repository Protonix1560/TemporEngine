

#include "tempor.hpp"
#include "i_graphics_device.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "scene_graph.hpp"
#include "scheduler.hpp"
#include "log_entry.hpp"

#include "sleep_clock.hpp"
#include "thread_info.hpp"

#include <chrono>
#include <cstddef>
#include <optional>

using namespace std::chrono_literals;


void TemporEngine::signal(int sig) noexcept {
    mSignal = sig;
}


TemporEngine::TemporEngine(
    size_t verboseLevel, std::filesystem::path configPath, bool flushConfig, bool configEnabled, bool allowTermColour
) : mConfigPath(configPath), mFlushConfig(flushConfig), mConfigEnabled(configEnabled),
    mAllowTermColour(allowTermColour), mTermLevel(static_cast<TprLogLevel>(verboseLevel)) {

    mpOutSink = &constructService<OutputSink>(mTermLevel, mAllowTermColour);
    mLogger.emplace(mpOutSink);

    mLogger->info(TPR_LOG_STYLE_NORMAL) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")";
}


int TemporEngine::runtime() {
    if (auto r = runtimeInit(); r != TPR_SUCCESS) {
        runtimeShutdown();
        return 2;
    }
    if (auto r = runtimeRun(); r != TPR_SUCCESS) {
        runtimeShutdown();
        return 3;
    }
    runtimeShutdown();
    return 0;
}

TprResult TemporEngine::runtimeInit() {
    auto initStartTimepoint = std::chrono::steady_clock::now();
    mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Initializing services";

    mpFileReg = &constructService<FileRegistry>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<FileRegistry> + ": "_ces)),
        mRunResult
    );

    mpSettings = &constructService<Settings>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<Settings> + ": "_ces)),
        *mpFileReg, mRunResult
    );
    if (auto r = mpSettings->init(mConfigPath, mFlushConfig, mConfigEnabled); r != TPR_SUCCESS) {
        mLogger->error() << "Failed to initialize Settings [" << r << "]";
        return TPR_PANIC;
    }

    auto logsRootExp = mpSettings->createSetting(mpSettings->getRoot(), "logFiles");
    if (!logsRootExp.has_value() && logsRootExp.error() == TPR_PANIC) return TPR_PANIC;
    auto logsRoot = logsRootExp.value();
    auto logCountExp = mpSettings->getSettingArraySize(logsRoot);
    if (!logCountExp.has_value() && logCountExp.error() == TPR_PANIC) return TPR_PANIC;
    for (size_t i = 0; i < logCountExp.value(); i++) {
        auto elExp = mpSettings->getSettingArrayElement(logsRoot, i);
        if (!elExp.has_value()) {
            if (elExp.error() == TPR_PANIC) return TPR_PANIC;
            continue;
        }
        auto el = elExp.value();
        auto levelExp = mpSettings->createSetting(el, "level");
        if (!levelExp.has_value()) {
            if (levelExp.error() == TPR_PANIC) return TPR_PANIC;
            continue;
        }
        auto levelIntExp = mpSettings->getSettingInteger(levelExp.value());
        if (!levelIntExp.has_value()) {
            if (levelIntExp.error() == TPR_PANIC) return TPR_PANIC;
            continue;
        }
        auto levelInt = levelIntExp.value();
        if (levelInt < 0) levelInt = 0;
        auto pathExp = mpSettings->createSetting(el, "path");
        if (!pathExp.has_value()) {
            if (pathExp.error() == TPR_PANIC) return TPR_PANIC;
            continue;
        }
        auto pathStringExp = mpSettings->getSettingString(pathExp.value());
        if (!pathStringExp.has_value()) {
            if (pathStringExp.error() == TPR_PANIC) return TPR_PANIC;
            continue;
        }
        mpOutSink->addLogFile(static_cast<TprLogLevel>(levelInt), pathStringExp.value());
        mLogger->info() << "Added log file " << pathStringExp.value();
    }
    mpOutSink->stopHistory();

    threadInfo.currentPlugin = std::make_shared<PluginContext>(true);
    mpSched = &constructService<Scheduler>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<Scheduler> + ": "_ces)),
        *mpSettings, mRunResult
    );
    if (auto r = mpSched->init(); r != TPR_SUCCESS) {
        mLogger->error() << "Failed to initialize Scheduler [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    mpSceneGraph = &constructService<SceneGraph>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<SceneGraph> + ": "_ces)),
        *mpSettings, *mpFileReg
    );

    mpAssetStore = &constructService<AssetStore>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<AssetStore> + ": "_ces)),
        *mpFileReg, mRunResult
    );

    // searching for a sufficient Graphics Device
    for (
        auto it = static_registry<GraphicsDeviceBackendInfo, 0>::instance().begin();
        it < static_registry<GraphicsDeviceBackendInfo, 0>::instance().end();
        it++
    ) {
        const auto& info = *it;

        mLogger->debug() << "Trying Graphics Device backend " << info.name << " with " << kGraphicsBackendName[to_underlying(info.graphics)] << "";

        Windowing* localWindowing = &constructService<Windowing>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<Windowing> + ": "_ces)),
            *mpSched, mRunResult
        );

        IGraphicsDevice* localGDev = constructService<PGraphicsDevice>(std::move(info.factory(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<PGraphicsDevice> + ": "_ces)),
            *mpFileReg, *localWindowing, *mpSettings, *mpSceneGraph, *mpSched, *mpAssetStore, mRunResult, 0
        ))).get();

        if (auto r = localWindowing->init(localGDev, info.graphics); r != TPR_SUCCESS) {
            mLogger->error() << "Failed to initialize Windowing [" << r << "]";
            mServHolder.destruct<Windowing>();
            mServHolder.destruct<PGraphicsDevice>();
            continue;
        }

        if (auto r = localGDev->init(); r != TPR_SUCCESS) {
            mLogger->error() << "Failed to initialize Graphics Device [" << r << "]";
            mServHolder.destruct<Windowing>();
            mServHolder.destruct<PGraphicsDevice>();
            continue;
        }

        mpWindowing = localWindowing;
        mpGDev = localGDev;
    }
    if (!mpGDev) {
        mLogger->error() << "Failed to initialize any graphics device backend";
        return TPR_ERROR_NOT_LOADED;
    }
    if (auto r = mpAssetStore->init(mpGDev); r != TPR_SUCCESS) {
        mLogger->error() << "Failed to initialize AssetStore [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    mpPlugLd = &constructService<PluginLoader>(
        Logger(mLogger.value()).setPrefix(format_sequence{} << (type_name_v_s<PluginLoader> + ": "_ces)),
        *mpSettings, *mpSched, &mAPI, mRunResult
    );
    if (auto r = mpPlugLd->init(); r != TPR_SUCCESS) {
        mLogger->error() << "Failed to initialize PluginLoader [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    registerAPI();

    // loading plugins
    {
        TprJobCreateInfo info{};
        info.context = this;
        info.duration = TPR_JOB_DURATION_LONG;
        info.function = [](void* ctx) noexcept {
            auto* engine = reinterpret_cast<TemporEngine*>(ctx);
            auto result = engine->mpPlugLd->loadPlugins();
            if (result != TPR_SUCCESS) engine->mRunResult.store(result);
            engine->mpSettings->finalizeRead();
            engine->mpSched->destroyJob(engine->mLoadPluginsJob);
            engine->mPluginsFinishedLoading.store_true();
        };
        auto exp = mpSched->createJob(info);
        if (!exp.has_value()) {
            mLogger->error() << "Failed to create pluginLoadJob [" << exp.error() << "]";
            return TPR_ERROR_NOT_LOADED;
        }
        mLoadPluginsJob = exp.value();
        if (auto r = mpSched->scheduleJob(mLoadPluginsJob, mpSched->now()); r != TPR_SUCCESS) {
            mLogger->error() << "Failed to schedule pluginLoadJob [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
    }

    auto initEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;
    mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Service initialization done in " << initTime.count() << " ms";

    return TPR_SUCCESS;
}

TprResult TemporEngine::runtimeRun() {
    sleep_clock clock{200.0};
    mpWindowing->eventLoopStarted();
    // polling because:
    // 1. signal handling in cross-platform C++ is dumb
    // 2. cocoa is dumb, only main thread is allowed for UI for some reason
    while (mRunResult.load() == _TPR_RESULT_MAX_ENUM) {
        clock.tick();
        if (auto r = mpWindowing->update(); r != TPR_SUCCESS) {
            mLogger->error() << "Failed to update Windowing [" << r << "]";
        }
        mpPlugLd->update();
        if (mpPlugLd->loadedPluginCount() == 0 && mPluginsFinishedLoading.load()) {
            mLogger->info() << "Shutting down because no plugins are loaded";
            mRunResult.store(TPR_SUCCESS);
        }
        if (mSignal != 0) mRunResult.store(TPR_SUCCESS);
    }

    mpWindowing->eventLoopEnded();

    auto runResult = mRunResult.load();
    if (runResult == TPR_PANIC) {
        mLogger->panic(TPR_LOG_STYLE_TIMESTAMP1) << "Engine panicked!";
    } else if (runResult != TPR_SUCCESS && runResult != _TPR_RESULT_MAX_ENUM) {
        mLogger->error(TPR_LOG_STYLE_TIMESTAMP1) << "Engine runtime failed [" << runResult << "]";
    }

    return runResult;
}

void TemporEngine::runtimeShutdown() {
    auto shutdownStartTimepoint = std::chrono::steady_clock::now();
    mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down";

    if (mpPlugLd) mpPlugLd->shutdown();

    if (mpSched) mpSched->shutdown();

    destructService<PluginLoader>();
    if (mpGDev) destructService<std::unique_ptr<IGraphicsDevice>>();
    destructService<SceneGraph>();
    destructService<Windowing>();
    destructService<AssetStore>();
    destructService<Scheduler>();
    destructService<Settings>();
    destructService<FileRegistry>();

    auto shutdownEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

    mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms";
}


TemporEngine::~TemporEngine() noexcept {
    mLogger.reset();
    destructService<OutputSink>();
}

