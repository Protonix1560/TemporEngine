

#include "tempor.hpp"
#include "asset_store.hpp"
#include "hardware_common_structs.hpp"
#include "core.hpp"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "resource_registry.hpp"
#include "scene_graph.hpp"
#include "settings.hpp"
#include "threading.hpp"

#include "thread_job_info.hpp"

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


TemporEngine::TemporEngine(size_t verboseLevel, std::filesystem::path configPath, bool flushConfig) : mConfigPath(configPath), mFlushConfig(flushConfig) {

    mpLogSink = &mServHolder.construct<LogSink>(verboseLevel);

    mLogger.emplace(mpLogSink->createLogger(""));

    mLogger->info(TPR_LOG_STYLE_STANDART) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")\n";
    mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Infrastructure service initialization now\n";

    mpResReg = &mServHolder.construct<ResourceRegistry>(mpLogSink->createLogger((type_name_v_s<ResourceRegistry> + ": "_ces).string()));

    mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Infrastructure service initialization done\n";

}


int TemporEngine::init() {

    mLogger->info(TPR_LOG_STYLE_STANDART) << "\033[0m";

    auto initStartTimepoint = std::chrono::steady_clock::now();

    mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Runtime service initialization now\n";

    mpSettings = &mServHolder.construct<Settings>(
        mpLogSink->createLogger((type_name_v_s<Settings> + ": "_ces).string()),
        *mpResReg, mConfigPath, mFlushConfig
    );

    threadLocalJobInfo.mainThread = true;
    mpThread = &mServHolder.construct<Threading>(mpLogSink->createLogger((type_name_v_s<Threading> + ": "_ces).string()), *mpSettings);

    mpSceneGraph = &mServHolder.construct<SceneGraph>(
        mpLogSink->createLogger((type_name_v_s<SceneGraph> + ": "_ces).string()),
        *mpSettings, *mpResReg
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

            mLogger->debug() << "Trying hardware layer " << manifest.name << " with GraphicsBackend = " << graphicsBackendName[to_underlying(manifest.graphicsBackend)] << "\n";

            WindowManager* localWinMan = &mServHolder.construct<WindowManager>(
                manifest.graphicsBackend,
                mpLogSink->createLogger((type_name_v_s<WindowManager> + ": "_ces).string()),
                mAliveTokens
            );

            auto layerExp = manifest.factory(
                mpLogSink->createLogger((type_name_v_s<PHardwareLayer> + ": "_ces).string()),
                *mpResReg, *localWinMan, *mpSettings, *mpSceneGraph, mComponentRenderable, 0, 1, 0, 0
            );
            HardwareLayer* localHWLI = mServHolder.construct<std::unique_ptr<HardwareLayer>>(std::move(layerExp.value())).get();

            mpWinMan = localWinMan;
            mpHWLI = localHWLI;

        } catch (const std::exception& e) {
            mLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << ":\n" << e.what() << "\n";
            mServHolder.destruct<WindowManager>();
            mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
            
        } catch (...) {
            mLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << "\n";
            mServHolder.destruct<WindowManager>();
            mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
        }

    }

    if (!mpHWLI) {
        mLogger->warn(TPR_LOG_STYLE_WARN1) << "Failed to initialize any hardware layer. Continuing without it\n";
        mpWinMan = &mServHolder.construct<WindowManager>(
            GraphicsBackend::None,
            mpLogSink->createLogger((type_name_v_s<WindowManager> + ": "_ces).string()),
            mAliveTokens
        );
    }

    mpAssetStore = &mServHolder.construct<AssetStore>(
        mpLogSink->createLogger((type_name_v_s<AssetStore> + ": "_ces).string()),
        *mpResReg, *mpHWLI
    );

    mpPlugLd = &mServHolder.construct<PluginLoader>(
        mpLogSink->createLogger((type_name_v_s<PluginLoader> + ": "_ces).string()),
        *mpSettings, mAliveTokens
    );

    registerAPI();

    auto pluginsExp = mpResReg->enumDir("plugins", TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT, 1);
    if (!pluginsExp.has_value()) return -1;
    auto plugins = pluginsExp.value();
    for (const auto& plugin : plugins) {
        try {
            PluginLoadInfo info{};
            info.loadType = PluginLoadType::InThread;
            info.name = plugin.filename().string();
            info.pAPI = &mAPI;
            info.path = plugin;
            mpPlugLd->loadPlugin(&info);
        } catch (...) {}
    }

    mpSettings->finalizeRead();

    auto initEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;

    mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Runtime service initialization done in " << initTime.count() << " ms\n";
    
    return 0;
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
            mLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced\n";
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
        mLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced\n";
        mPanic.store(true);
        return unexpected(TPR_PANIC);
    }
    return infoExp.value();
}


int TemporEngine::run() {

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
            l << " signal\n";
            mMustShutdown = true;
        }

        mClock.tick();
    }

    if (mPanic.load()) {
        mLogger->error(TPR_LOG_STYLE_STANDART) << "\033[95mEngine panicked!\n";
    }

    return 0;
}


void TemporEngine::shutdown() {

    auto shutdownStartTimepoint = std::chrono::steady_clock::now();

    mLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down\n";

    mpPlugLd->triggerCallback(PluginCallback::PreShutdown).value();

    mpThread->joinAll();
    
    mpPlugLd->triggerCallback(PluginCallback::Shutdown).value();

    mServHolder.destruct<PluginLoader>();
    if (mpHWLI) mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
    mServHolder.destruct<SceneGraph>();
    mServHolder.destruct<WindowManager>();
    mServHolder.destruct<AssetStore>();
    mServHolder.destruct<Threading>();
    mServHolder.destruct<Settings>();
    mServHolder.destruct<ResourceRegistry>();

    auto shutdownEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

    mLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms\n";
}


TemporEngine::~TemporEngine() noexcept {

    mServHolder.destruct<LogSink>();

}

