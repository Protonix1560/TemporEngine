

#include "tempor.hpp"
#include "asset_store.hpp"
#include "hardware_common_structs.hpp"
#include "core.hpp"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "plugin.h"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "resource_registry.hpp"
#include "scene_graph.hpp"
#include "settings.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>

#include <glm/packing.hpp>
#include <nlohmann/json.hpp>

using njson = nlohmann::json;




void TemporEngine::sigint() noexcept {
    mSigInt = 1;
}

void TemporEngine::sigterm() noexcept {
    mSigTerm = 1;
}



Logger* TemporEngine::getLogger() noexcept {
    if (!mServHolder.alive<Logger>()) return nullptr;
    return &mServHolder.get<Logger>();
}

ResourceRegistry* TemporEngine::getResourceRegistry() noexcept {
    if (!mServHolder.alive<ResourceRegistry>()) return nullptr;
    return &mServHolder.get<ResourceRegistry>();
}

WindowManager* TemporEngine::getWindowManager() noexcept {
    if (!mServHolder.alive<WindowManager>()) return nullptr;
    return &mServHolder.get<WindowManager>();
}

HardwareLayer* TemporEngine::getPHWL() noexcept {
    if (!mServHolder.alive<PHardwareLayer>()) return nullptr;
    return mServHolder.get<PHardwareLayer>().get();
}

SceneGraph* TemporEngine::getSceneGraph() noexcept {
    if (!mServHolder.alive<SceneGraph>()) return nullptr;
    return &mServHolder.get<SceneGraph>();
}

AssetStore* TemporEngine::getAssetStore() noexcept {
    if (!mServHolder.alive<AssetStore>()) return nullptr;
    return &mServHolder.get<AssetStore>();
}

PluginLoader* TemporEngine::getPluginLoader() noexcept {
    if (!mServHolder.alive<PluginLoader>()) return nullptr;
    return &mServHolder.get<PluginLoader>();
}

Settings* TemporEngine::getSettings() noexcept {
    if (!mServHolder.alive<Settings>()) return nullptr;
    return &mServHolder.get<Settings>();
}


TprComponent TemporEngine::getComponentRenderable() noexcept {
    return mComponentRenderable;
}



TemporEngine::TemporEngine(size_t verboseLevel, const TprEngineAPI* api)
    : mpAPI(api) {

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

    mpSettings = &mServHolder.construct<Settings>(*mpLogger, *mpResReg);

    mpSceneGraph = &mServHolder.construct<SceneGraph>(*mpLogger, *mpSettings, *mpResReg);

    {
        auto exp = mpSceneGraph->createComponent(sizeof(TprComponentRenderable));
        if (!exp.has_value()) return -1;  // TODO: standardize exit codes finally
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

            mpLogger->debug() << "Trying hardware layer " << manifest.name << " with GraphicsBackend=" << graphicsBackendName[to_underlying(manifest.graphicsBackend)] << "...\n";

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

    auto readyOpeningWindowTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> readyOpeningWindowTime = readyOpeningWindowTimepoint - initStartTimepoint;

    mpLogger->info(TPR_LOG_STYLE_TIMESTAMP1) << "Ready to open a window in " << readyOpeningWindowTime.count() << " ms\n";

    /*
        Potentially minimum amount of loading before showing a loading screen is finished
    */

    mpAssetStore = &mServHolder.construct<AssetStore>(*mpLogger, *mpResReg, *mpHWLI);
    mpPlugLd = &mServHolder.construct<PluginLoader>(*mpLogger, *mpSettings, mAliveTokens);

    auto plugins = mpResReg->enumDir("plugins", TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT, 1);
    for (const auto& plugin : plugins) {
        try {
            PluginLoadInfo info{};
            info.loadType = PluginLoadType::InThread;
            info.name = plugin.filename().string();
            info.pAPI = mpAPI;
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


int TemporEngine::run() {

    mClock.begin();

    while (mAliveTokens > 0 && !mMustShutdown) {

        mpWinMan->update();

        mpPlugLd->triggerCallback(PluginCallback::UpdatePerFrame);

        if (mpHWLI) {
            TprResult ret;
            ret = mpHWLI->update();
            if (ret == TPR_UNKNOWN_ERROR) {
                mpLogger->error(TPR_LOG_STYLE_ERROR1) << "HWL.update failed [" << ret << "]\n";
                return -1;
            } else if (ret != TPR_SUCCESS) {
                mpLogger->warn(TPR_LOG_STYLE_WARN1) << "HWL.update failed [" << ret << "]\n";
            }
            ret = mpHWLI->render();
            if (ret == TPR_UNKNOWN_ERROR) {
                mpLogger->error(TPR_LOG_STYLE_ERROR1) << "HWL.render failed [" << ret << "]\n";
                return -1;
            } else if (ret != TPR_SUCCESS) {
                mpLogger->warn(TPR_LOG_STYLE_WARN1) << "HWL.render failed [" << ret << "]\n";
            }
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


    return 0;
}


void TemporEngine::shutdown() {

    auto shutdownStartTimepoint = std::chrono::steady_clock::now();

    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down\n";

    mpPlugLd->triggerCallback(PluginCallback::PreShutdown).value();
    mpPlugLd->triggerCallback(PluginCallback::Shutdown).value();

    mServHolder.destruct<PluginLoader>();
    if (mpHWLI) mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
    mServHolder.destruct<SceneGraph>();
    mServHolder.destruct<WindowManager>();
    mServHolder.destruct<AssetStore>();
    mServHolder.destruct<Settings>();
    mServHolder.destruct<ResourceRegistry>();

    auto shutdownEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms\n";
}


TemporEngine::~TemporEngine() noexcept {

    mServHolder.destruct<Logger>();

}


