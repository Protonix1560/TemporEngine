

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


TemporEngine::TemporEngine(size_t verboseLevel, std::string configPath) : mConfigPath(configPath) {

    mpLogger = &mServHolder.construct<Logger>(verboseLevel);

    mpLogger->info(TPR_LOG_STYLE_STANDART) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")\n";
    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Infrastructure service initialization now\n";

    mpResReg = &mServHolder.construct<ResourceRegistry>(*mpLogger);

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Infrastructure service initialization done\n";

}


int TemporEngine::init() {

    mpLogger->info(TPR_LOG_STYLE_STANDART) << "\033[0m";

    auto initStartTimepoint = std::chrono::steady_clock::now();

    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Runtime service initialization now\n";

    mpSettings = &mServHolder.construct<Settings>(*mpLogger, *mpResReg, mConfigPath);

    threadLocalJobInfo.mainThread = true;
    mpThread = &mServHolder.construct<Threading>(*mpLogger, *mpSettings);

    mpSceneGraph = &mServHolder.construct<SceneGraph>(*mpLogger, *mpSettings, *mpResReg);

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

            mpLogger->debug() << "Trying hardware layer " << manifest.name << " with GraphicsBackend=" << graphicsBackendName[to_underlying(manifest.graphicsBackend)] << "\n";

            WindowManager* localWinMan = &mServHolder.construct<WindowManager>(manifest.graphicsBackend, *mpLogger, mAliveTokens);

            auto layerExp = manifest.factory(*mpLogger, *mpResReg, *localWinMan, *mpSettings, *mpSceneGraph, mComponentRenderable, 0, 1, 0, 0);
            HardwareLayer* localHWLI = mServHolder.construct<std::unique_ptr<HardwareLayer>>(std::move(layerExp.value())).get();

            mpWinMan = localWinMan;
            mpHWLI = localHWLI;

        } catch (const std::exception& e) {
            mpLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << ":\n" << e.what() << "\n";
            mServHolder.destruct<WindowManager>();
            mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
            
        } catch (...) {
            mpLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << "\n";
            mServHolder.destruct<WindowManager>();
            mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
        }

    }

    if (!mpHWLI) {
        mpLogger->warn(TPR_LOG_STYLE_WARN1) << "Failed to initialize any hardware layer. Continuing without it\n";
        mpWinMan = &mServHolder.construct<WindowManager>(GraphicsBackend::None, *mpLogger, mAliveTokens);
    }

    mpAssetStore = &mServHolder.construct<AssetStore>(*mpLogger, *mpResReg, *mpHWLI);

    mpPlugLd = &mServHolder.construct<PluginLoader>(*mpLogger, *mpSettings, mAliveTokens);

    registerAPI();

    auto plugins = mpResReg->enumDir("plugins", TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT, 1);
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

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Runtime service initialization done in " << initTime.count() << " ms\n";
    
    return 0;
}


expected<uint32_t, TprResult> TemporEngine::activePluginID() {
    if (!mpPlugLd) return unexpected(TPR_MODULE_NOT_LOADED);
    if (threadLocalJobInfo.mainThread) {
        auto pluginOpt = mpPlugLd->getActivePluginID();
        if (!pluginOpt.has_value()) {
            // the caller is probably not a plugin
            return unexpected(TPR_INVALID_OPERATION);
        }
        return pluginOpt.value();
    } else {
        if (!threadLocalJobInfo.job.has_value()) {
            // the caller is probably not a plugin
            return unexpected(TPR_INVALID_OPERATION);
        }
        uint32_t id = threadLocalJobInfo.job.value();
        auto it = mJobPluginMap.find(id);
        if (it == mJobPluginMap.end()) {
            // the map is desynced for some reason
            // therefore, threadLocalJobInfo is probably corrupted
            mPanic.store(true);
            mpLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced\n";
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
        mpLogger->error(TPR_LOG_STYLE_PANIC1) << "TemporEngine.mJobPluginMap is desynced\n";
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
            mpHWLI->update();
            mpHWLI->render();
        }

        if (mSigInt || mSigTerm) {
            if (mSigInt) {
                mpLogger->info(TPR_LOG_STYLE_STANDART) << "\n";
            }
            auto l = mpLogger->debug(TPR_LOG_STYLE_TIMESTAMP1);
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
        mpLogger->error(TPR_LOG_STYLE_STANDART) << "\033[95mEngine panicked!\n";
    }

    return 0;
}


void TemporEngine::shutdown() {

    auto shutdownStartTimepoint = std::chrono::steady_clock::now();

    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down\n";

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

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms\n";
}


TemporEngine::~TemporEngine() noexcept {

    mServHolder.destruct<Logger>();

}

