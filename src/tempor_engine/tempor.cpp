

#include "tempor.hpp"
#include "output_sink.hpp"
#include "plugin_core.h"
#include "thread_job_info.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>

using namespace std::chrono_literals;


void TemporEngine::signal(int sig) noexcept {
    mSignal = sig;
}


expected<uint32_t, TprResult> TemporEngine::activePluginID() {
    if (!mpPlugLd) return unexpected(TPR_MODULE_NOT_LOADED);
    if (threadLocalJobInfo.mainThread) {
        auto pluginOpt = mpPlugLd->getActivePluginID();
        if (!pluginOpt.has_value()) {
            // the caller is probably not a plugin
            return unexpected(TPR_ERROR_INVALID_OPERATION);
        }
        return pluginOpt.value();
    } else {
        if (!threadLocalJobInfo.job.has_value()) {
            // the caller is probably not a plugin
            return unexpected(TPR_ERROR_INVALID_OPERATION);
        }
        TprJob job = threadLocalJobInfo.job.value();
        auto it = mJobPluginMap.find(job._d);
        if (it == mJobPluginMap.end()) {
            // the map is desynced for some reason
            // therefore, threadLocalJobInfo is probably corrupted
            mLogger->panic() << "Corrupted internal structures: job " << job._d << " doesn't appear in mJobPluginMap";
            return unexpected(TPR_PANIC);
        }
        return it->second;
    }
}


expected<PluginInfo, TprResult> TemporEngine::activePluginInfo() {
    if (!mpPlugLd) return unexpected(TPR_MODULE_NOT_LOADED);
    auto pluginExp = activePluginID();
    if (!pluginExp.has_value()) return unexpected(pluginExp.error());
    auto infoExp = mpPlugLd->getPluginInfo(pluginExp.value());
    if (!infoExp.has_value()) {
        mLogger->panic() << "Corrupted internal structures: PluginLoader.getPluginInfo doesn't"
            "recognise plugin id that was returned by PluginLoader.getActivePluginID";
        return unexpected(TPR_PANIC);
    }
    return infoExp.value();
}


TemporEngine::TemporEngine(
    size_t verboseLevel, std::filesystem::path configPath, bool flushConfig, bool configEnabled, bool colourEnabled
) : mConfigPath(configPath), mFlushConfig(flushConfig), mConfigEnabled(configEnabled), mColourEnabled(colourEnabled) {

    mVerbosity = static_cast<TprLogLevel>(verboseLevel);

    mpOutSink.store(
        std::visit(overload{
            [](auto sink) -> std::shared_ptr<LogSinkInterface> { return static_cast<std::shared_ptr<LogSinkInterface>>(sink); }
        }, mServHolder.construct<OutputSinkVariant>(std::make_shared<TermSink>(mVerbosity, mColourEnabled)))
    );

    mLogger.emplace(Logger(mpOutSink, ""));

    mLogger->info(TPR_LOG_STYLE_STANDART) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")\n";

}


int TemporEngine::runtime() {

    TprResult result;

    // init
    {
        auto initStartTimepoint = std::chrono::steady_clock::now();
        mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Initializing services";

        mpFileReg = &mServHolder.construct<FileRegistry>(Logger(mpOutSink, (type_name_v_s<FileRegistry> + ": "_ces).string()));

        mpSettings = &mServHolder.construct<Settings>(
            Logger(mpOutSink, (type_name_v_s<Settings> + ": "_ces).string()), *mpFileReg
        );
        result = mpSettings->init(mConfigPath, mFlushConfig, mConfigEnabled);
        if (result != TPR_SUCCESS) {
            mLogger->error() << "Failed to initialize Settings [" << result << "]";
            return 2;
        }

        // switching TermSink to TermFileSink
        {
            std::shared_ptr<TermSink> sink = std::get<std::shared_ptr<TermSink>>(mServHolder.get<OutputSinkVariant>());
            mServHolder.destruct<OutputSinkVariant>();
            auto& var = mServHolder.construct<OutputSinkVariant>(std::make_shared<TermFileSink>(
                *mpSettings, *mpFileReg, *sink.get(), mVerbosity, mColourEnabled
            ));
            std::shared_ptr<LogSinkInterface> shar = std::visit(overload{
                [](auto sink) -> std::shared_ptr<LogSinkInterface> { return static_cast<std::shared_ptr<LogSinkInterface>>(sink); }
            }, var);
            mpOutSink.store(shar);
        }

        threadLocalJobInfo.mainThread = true;
        mpSched = &mServHolder.construct<Scheduler>(Logger(mpOutSink, (type_name_v_s<Scheduler> + ": "_ces).string()), *mpSettings);
        result = mpSched->init();
        if (result != TPR_SUCCESS) {
            mLogger->error() << "Failed to initialize Scheduler [" << result << "]";
            return 2;
        }

        {
            TprJobCreateInfo renderJobInfo{};
            renderJobInfo.context = this;
            renderJobInfo.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
            renderJobInfo.duration = TPR_JOB_DURATION_LONG;
            renderJobInfo.function = [](void* ctx, TprJob job) noexcept {
                auto* engine = reinterpret_cast<TemporEngine*>(ctx);
                engine->runtimeRender();
            };
            auto exp = mpSched->createJob(&renderJobInfo);
            if (!exp.has_value()) {
                mLogger->error() << "Failed to create Render Job [" << result << "]";
                return 2;
            }
            mRenderJob = exp.value();
        }

        {
            TprJobCreateInfo shutdownJobInfo{};
            shutdownJobInfo.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
            shutdownJobInfo.duration = TPR_JOB_DURATION_LONG;
            auto exp = mpSched->createJob(&shutdownJobInfo);
            if (!exp.has_value()) {
                mLogger->error() << "Failed to create Shutdown Job [" << result << "]";
                return 2;
            }
            mShutdownJob = exp.value();
        }

        mpSceneGraph = &mServHolder.construct<SceneGraph>(
            Logger(mpOutSink, (type_name_v_s<SceneGraph> + ": "_ces).string()),
            *mpSettings, *mpFileReg
        );

        {
            auto exp = mpSceneGraph->createComponent(sizeof(TprComponentRenderable));
            if (!exp.has_value()) {
                mLogger->error() << "Failed to create TprComponentRenderable [" << result << "]";
                return 2;
            }
            mComponentRenderable = exp.value();
        }

        // finding sufficient HWL
        for (
            auto it = static_registry<HardwareLayerManifest, 0>::instance().begin();
            it < static_registry<HardwareLayerManifest, 0>::instance().end();
            it++
        ) {
            const auto& manifest = *it;

            try {

                mLogger->debug() << "Trying hardware layer " << manifest.name << " with GraphicsBackend = " << graphicsBackendName[to_underlying(manifest.graphicsBackend)] << "";

                WindowManager* localWinMan = &mServHolder.construct<WindowManager>(
                    manifest.graphicsBackend,
                    Logger(mpOutSink, (type_name_v_s<WindowManager> + ": "_ces).string()),
                    mAliveTokens
                );

                auto layerExp = manifest.factory(
                    Logger(mpOutSink, (type_name_v_s<PHardwareLayer> + ": "_ces).string()),
                    *mpFileReg, *localWinMan, *mpSettings, *mpSceneGraph, mComponentRenderable, 0, 1, 0, 0
                );
                HardwareLayer* localHWLI = mServHolder.construct<std::unique_ptr<HardwareLayer>>(std::move(layerExp.value())).get();

                mpWinMan = localWinMan;
                mpHWLI = localHWLI;

            } catch (const std::exception& e) {
                mLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << ":\n" << e.what() << "";
                mServHolder.destruct<WindowManager>();
                mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
                
            } catch (...) {
                mLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << "";
                mServHolder.destruct<WindowManager>();
                mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
            }

        }

        if (!mpHWLI) {
            mLogger->warn(TPR_LOG_STYLE_WARN1) << "Failed to initialize any hardware layer. Continuing without it";
            mpWinMan = &mServHolder.construct<WindowManager>(
                GraphicsBackend::None,
                Logger(mpOutSink, (type_name_v_s<WindowManager> + ": "_ces).string()),
                mAliveTokens
            );
        }

        mpAssetStore = &mServHolder.construct<AssetStore>(
            Logger(mpOutSink, (type_name_v_s<AssetStore> + ": "_ces).string()),
            *mpFileReg, *mpHWLI
        );

        mpPlugLd = &mServHolder.construct<PluginLoader>(
            Logger(mpOutSink, (type_name_v_s<PluginLoader> + ": "_ces).string()),
            *mpSettings, mAliveTokens
        );

        registerAPI();

        auto initEndTimepoint = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;
        mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Service initialization done in " << initTime.count() << " ms";

        auto pluginStartTimepoint = std::chrono::steady_clock::now();
        mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Initializing plugins";

        PluginLoadInfo info{};
        info.loadType = PluginLoadType::InThread;
        info.name = "test";
        info.pAPI = &mAPI;
        info.path = "plugins/test/libtest_plugin.so";
        mpPlugLd->loadPlugin(&info);

        auto pluginEndTimepoint = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> pluginTime = pluginEndTimepoint - pluginStartTimepoint;
        mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Plugin initialization done in " << pluginTime.count() << " ms";

        mpSettings->finalizeRead();

        mRenderLastLaunch = mpSched->now();
        mpSched->scheduleJob(mRenderJob, mRenderLastLaunch);
    }

    std::unique_lock<std::mutex> lock(mMainThreadMutex);
    // polling because signals in C++ are dumb
    while (!mRunResult.has_value() && mSignal == 0) {
        mMainThreadCv.wait_for(lock, 10ms);
    }

    if (mRunResult && mRunResult.value() == TPR_PANIC) mLogger->panic(TPR_LOG_STYLE_TIMESTAMP1) << "Engine panicked!";

    // shutdown
    {
        auto shutdownStartTimepoint = std::chrono::steady_clock::now();
        mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down";

        mpSched->scheduleJob(mShutdownJob, 0);

        mpSched->shutdown();

        mServHolder.destruct<PluginLoader>();
        if (mpHWLI) mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
        mServHolder.destruct<SceneGraph>();
        mServHolder.destruct<WindowManager>();
        mServHolder.destruct<AssetStore>();

        mServHolder.destruct<Scheduler>();

        // switching TermFileSink back to TermSink
        {
            std::shared_ptr<TermFileSink> sink = std::get<std::shared_ptr<TermFileSink>>(mServHolder.get<OutputSinkVariant>());
            mServHolder.destruct<OutputSinkVariant>();
            auto& var = mServHolder.construct<OutputSinkVariant>(std::make_shared<TermSink>(mVerbosity, mColourEnabled));
            std::shared_ptr<LogSinkInterface> shar = std::visit(overload{
                [](auto sink) -> std::shared_ptr<LogSinkInterface> { return static_cast<std::shared_ptr<LogSinkInterface>>(sink); }
            }, var);
            mpOutSink.store(shar);
        }

        mServHolder.destruct<Settings>();
        mServHolder.destruct<FileRegistry>();

        auto shutdownEndTimepoint = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

        mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms";
    }

    return (mRunResult && mRunResult.value() != TPR_SUCCESS) ? 3 : 0;
}


void TemporEngine::runtimeRender() {

    std::optional<TprResult> runResult;
    {
        std::lock_guard<std::mutex> lock(mMainThreadMutex);
        runResult = mRunResult;
    }
    if (!runResult.has_value()) {

        mpWinMan->update();

        if (mpHWLI) {
            if (auto result = mpHWLI->update(); result != TPR_SUCCESS) {
                mLogger->error() << "Failed to update PHWL [" << result << "]";
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(result);
                mMainThreadCv.notify_all();
                return;
            }
            if (auto result = mpHWLI->render(); result != TPR_SUCCESS) {
                mLogger->error() << "Failed to render [" << result << "]";
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(result);
                mMainThreadCv.notify_all();
                return;
            }
        }
    }

    mRenderLastLaunch += 16'666'667;
    mpSched->scheduleJob(mRenderJob, mRenderLastLaunch);
}


TemporEngine::~TemporEngine() noexcept {}

