

#include "tempor.hpp"
#include "output_sink.hpp"
#include "plugin_core.h"
#include "thread_job_info.hpp"
#include "hardware_common_structs.hpp"
#include "plugin_common_structs.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>


void TemporEngine::sigint() noexcept {
    mSigInt = 1;
}

void TemporEngine::sigterm() noexcept {
    mSigTerm = 1;
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
        uint32_t id = threadLocalJobInfo.job.value();
        auto it = mJobPluginMap.find(id);
        if (it == mJobPluginMap.end()) {
            // the map is desynced for some reason
            // therefore, threadLocalJobInfo is probably corrupted
            mPanic.store(true);
            mLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced";
            return unexpected(TPR_PANIC);
        }
        return it->second;
    }
}


expected<PluginInfo, TprResult> TemporEngine::activePluginInfo() {
    if (!mpPlugLd) return unexpected(TPR_MODULE_NOT_LOADED);
    auto idExp = activePluginID();
    if (!idExp.has_value()) return unexpected(idExp.error());
    auto infoExp = mpPlugLd->getPluginInfo(idExp.value());
    if (!infoExp.has_value()) {
        mLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced";
        mPanic.store(true);
        return unexpected(TPR_PANIC);
    }
    return infoExp.value();
}


TemporEngine::TemporEngine(size_t verboseLevel, std::filesystem::path configPath, bool flushConfig, bool configEnabled)
    : mConfigPath(configPath), mFlushConfig(flushConfig), mConfigEnabled(configEnabled) {

    mVerbosity = static_cast<TprLogLevel>(verboseLevel);

    mpOutSink.store(
        std::visit(overload{
            [](auto sink) -> std::shared_ptr<LogSinkInterface> { return static_cast<std::shared_ptr<LogSinkInterface>>(sink); }
        }, mServHolder.construct<OutputSinkVariant>(std::make_shared<TermSink>(mVerbosity)))
    );

    mLogger.emplace(Logger(mpOutSink, ""));

    mLogger->info(TPR_LOG_STYLE_STANDART) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")\n";

}


int TemporEngine::runtime() {

    // init
    {
        auto initStartTimepoint = std::chrono::steady_clock::now();

        mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Initializing services";

        mpFileReg = &mServHolder.construct<FileRegistry>(Logger(mpOutSink, (type_name_v_s<FileRegistry> + ": "_ces).string()));

        mpSettings = &mServHolder.construct<Settings>(
            Logger(mpOutSink, (type_name_v_s<Settings> + ": "_ces).string()), *mpFileReg
        );
        TprResult r = mpSettings->init(mConfigPath, mFlushConfig, mConfigEnabled);
        if (r != TPR_SUCCESS) {
            mLogger->error() << "Failed to initialize Settings [" << r << "]";
            return -1;
        }

        // switching TermSink to TermFileSink
        {
            std::shared_ptr<TermSink> sink = std::get<std::shared_ptr<TermSink>>(mServHolder.get<OutputSinkVariant>());
            mServHolder.destruct<OutputSinkVariant>();
            auto& var = mServHolder.construct<OutputSinkVariant>(std::make_shared<TermFileSink>(
                *mpSettings, *mpFileReg, *sink.get(), mVerbosity
            ));
            std::shared_ptr<LogSinkInterface> shar = std::visit(overload{
                [](auto sink) -> std::shared_ptr<LogSinkInterface> { return static_cast<std::shared_ptr<LogSinkInterface>>(sink); }
            }, var);
            mpOutSink.store(shar);
        }

        threadLocalJobInfo.mainThread = true;
        mpThread = &mServHolder.construct<Threading>(Logger(mpOutSink, (type_name_v_s<Threading> + ": "_ces).string()), *mpSettings);

        mpSceneGraph = &mServHolder.construct<SceneGraph>(
            Logger(mpOutSink, (type_name_v_s<SceneGraph> + ": "_ces).string()),
            *mpSettings, *mpFileReg
        );

        {
            auto exp = mpSceneGraph->createComponent(sizeof(TprComponentRenderable));
            if (!exp.has_value()) return 1;
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

        // auto pluginsExp = mpFileReg->enumDir("plugins", TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT, 1);
        // if (!pluginsExp.has_value()) return -1;
        // auto plugins = pluginsExp.value();
        // for (const auto& plugin : plugins) {
        //     try {
        //         PluginLoadInfo info{};
        //         info.loadType = PluginLoadType::InThread;
        //         info.name = plugin.filename().string();
        //         info.pAPI = &mAPI;
        //         info.path = plugin;
        //         mpPlugLd->loadPlugin(&info);
        //     } catch (...) {}
        // }

        PluginLoadInfo info{};
        info.loadType = PluginLoadType::InThread;
        info.name = "test";
        info.pAPI = &mAPI;
        info.path = "plugins/test/libtest_plugin.so";
        mpPlugLd->loadPlugin(&info);

        mpSettings->finalizeRead();

        auto initEndTimepoint = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;

        mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Service initialization done in " << initTime.count() << " ms";
    }
    
    // run
    {

        mClock.begin();

        while (!mPanic.load() && mAliveTokens > 0 && !mMustShutdown) {

            mpWinMan->update();

            mpPlugLd->triggerCallback(PluginCallback::UpdatePerFrame);

            mpThread->update();

            if (mpHWLI) {
                TprResult r;
                r = mpHWLI->update();
                switch (r) {
                    case TPR_SUCCESS:
                        break;
                    case TPR_PANIC:
                        mPanic.store(true);
                        break;
                    default:
                        break;
                }
                r = mpHWLI->render();
                switch (r) {
                    case TPR_SUCCESS:
                        break;
                    case TPR_PANIC:
                        mPanic.store(true);
                        break;
                    default:
                        break;
                }
            }

            if (mSigInt || mSigTerm) {
                if (mSigInt) {
                    mLogger->info(TPR_LOG_STYLE_STANDART) << "\n";
                }
                auto l = mLogger->debug(TPR_LOG_STYLE_TIMESTAMP1);
                l << "Received ";
                if (mSigInt) {
                    l << "SIG_INT";
                } else if (mSigTerm) {
                    l << "SIG_TERM";
                }
                l << " signal";
                mMustShutdown = true;
            }

            mClock.tick();
        }

        if (mPanic.load()) {
            mLogger->error(TPR_LOG_STYLE_STANDART) << "\033[95mEngine panicked!";
        }
    }
    
    // shutdown
    {

        auto shutdownStartTimepoint = std::chrono::steady_clock::now();

        mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down";

        mpPlugLd->triggerCallback(PluginCallback::PreShutdown).value();

        mpThread->joinAll();
        
        mpPlugLd->triggerCallback(PluginCallback::Shutdown).value();

        mServHolder.destruct<PluginLoader>();
        if (mpHWLI) mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
        mServHolder.destruct<SceneGraph>();
        mServHolder.destruct<WindowManager>();
        mServHolder.destruct<AssetStore>();
        mServHolder.destruct<Threading>();

        // switching TermFileSink back to TermSink
        {
            std::shared_ptr<TermFileSink> sink = std::get<std::shared_ptr<TermFileSink>>(mServHolder.get<OutputSinkVariant>());
            mServHolder.destruct<OutputSinkVariant>();
            auto& var = mServHolder.construct<OutputSinkVariant>(std::make_shared<TermSink>(mVerbosity));
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

    return 0;
}


TemporEngine::~TemporEngine() noexcept {}

