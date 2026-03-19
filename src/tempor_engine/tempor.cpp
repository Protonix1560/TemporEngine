

#include "tempor.hpp"
#include "hardware_common_structs.hpp"
#include "core.hpp"
#include "plugin.h"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"

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



Logger& TemporEngine::getLogger() {
    return mServHolder.get<Logger>();
}

ResourceRegistry& TemporEngine::getResourceRegistry() {
    return mServHolder.get<ResourceRegistry>();
}

WindowManager& TemporEngine::getWindowManager() {
    return mServHolder.get<WindowManager>();
}



TemporEngine::TemporEngine(size_t verboseLevel, const TprEngineAPI* api)
    : mpAPI(api) {

    mpLogger = &mServHolder.construct<Logger>(5);
    mpLogger->setVerbosityLevel(verboseLevel);
    gGetServiceLocator()->provide(mpLogger);

    mpLogger->info(TPR_LOG_STYLE_STANDART) << "Tempor Engine " << BUILD_VERSION << " (build datetime: " << BUILD_DATETIME << ")\n";
    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Infrastructure service initialization now\n";

    mpResReg = &mServHolder.construct<ResourceRegistry>(*mpLogger);

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Infrastructure service initialization done\n";

}


int TemporEngine::init() {

    mpLogger->info(TPR_LOG_STYLE_STANDART) << "\033[0m";

    auto initStartTimepoint = std::chrono::steady_clock::now();

    mpLogger->info(TPR_LOG_STYLE_STARTSTAMP1) << "Runtime service initialization now\n";

    // loading main config
    TprResource confResource = mpResReg->openResource("config.json").value();
    std::filesystem::path confPath = mpResReg->getResourceFilepath(confResource);
    const std::byte* confBegin = mpResReg->getResourceConstPointer(confResource).value();
    const std::byte* confEnd = mpResReg->sizeofResource(confResource).value() + confBegin;

    mpLogger->debug(TPR_LOG_STYLE_TIMESTAMP1) << "Located config \"" << confPath.string() << "\"\n";

    njson mainJson;
    try {
        mainJson = njson::parse(confBegin, confEnd);
    } catch (const njson::exception& e) {
        throw Exception(ErrCode::FormatError, "Failed to parse config \""s + confPath.string() + "\""s);
    }

    // reading configs
    if (!(
        mainJson.contains("asset") && mainJson["asset"].contains("format") &&
        mainJson["asset"]["format"].get<std::string>() == "tempor" && mainJson["asset"].contains("version")
    )) {
        throw Exception(ErrCode::FormatError, "Config \""s + confPath.string() + "\" is corrupted"s);
    }

    int major = 0, minor = 0, patch = 0;
    {
        std::string mainJsonVersionStr = mainJson["asset"]["version"].get<std::string>();
        int* numbers[] = {&major, &minor, &patch};
        size_t verDigitSize = 0;
        int i = 0;
        while (verDigitSize != std::string::npos) {
            if (i == std::size(numbers)) throw Exception(ErrCode::FormatError, "Invalid config \""s + confPath.string() + "\" version"s);
            verDigitSize = mainJsonVersionStr.find('.');
            *numbers[i] = std::stoi(mainJsonVersionStr.substr(0, verDigitSize));
            mainJsonVersionStr = mainJsonVersionStr.substr(verDigitSize + 1);
            i++;
        }
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
            gGetServiceLocator()->provide(localWinMan);
            HWLCreateInfo hWLCreateInfo = {
                *localWinMan, *mpLogger, *mpResReg
            };
            HardwareLayer* localHWLI = mServHolder.construct<std::unique_ptr<HardwareLayer>>(manifest.factory(hWLCreateInfo)).get();

            mpWinMan = localWinMan;
            mpHWLI = localHWLI;

        } catch (const std::exception& e) {
            mpLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << ":\n" << e.what() << "\n";
            gGetServiceLocator()->provide<WindowManager>(nullptr);
            mServHolder.destruct<WindowManager>();
            mServHolder.destruct<std::unique_ptr<HardwareLayer>>();

        } catch (...) {
            mpLogger->error(TPR_LOG_STYLE_ERROR1) << "Failed to initialize hardware layer " << manifest.name << "\n";
            gGetServiceLocator()->provide<WindowManager>(nullptr);
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
        TODO:
        Potentially minimum amount of loading before showing a loading screen is finished,
        everything past this comment should be moved to another threads
    */


    mpAssetStore = &mServHolder.construct<AssetStore>(*mpLogger, *mpResReg);
    mpPlugLd = &mServHolder.construct<PluginLoader>(*mpLogger, mAliveTokens);
    mpHWMO = &mServHolder.construct<HardwareMemoryOptimizator>();
    mpSceneGraph = &mServHolder.construct<SceneGraph>(*mpLogger);

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

    auto initEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Runtime service initialization done in " << initTime.count() << " ms\n";
    
    return 0;
}


int TemporEngine::run() {

    mClock.begin();

    // mAliveTokens++;

    while (mAliveTokens > 0 && !mMustShutdown) {

        mpPlugLd->triggerCallback(PluginCallback::UpdatePerFrame);

        mpResReg->update();
        // mpPlugLd->update();
        mpSceneGraph->update();
        mpAssetStore->update();
        mpHWMO->update();
        mpWinMan->update();
        if (mpHWLI) mpHWLI->update();

        if (mpHWLI) {
            RenderGraph graph{};
            for (auto window : mpWinMan->getWindows()) {
                RenderGraph::WindowConfig cfg{};
                cfg.scissor.width = mpWinMan->getWindowWidth(window).value();
                cfg.scissor.height = mpWinMan->getWindowHeight(window).value();
                cfg.viewport.width = 1.0f;
                cfg.viewport.height = 1.0f;
                cfg.viewport.minDepth = 0.0f;
                cfg.viewport.maxDepth = 1.0f;
                graph.windows.emplace_back(window, cfg);
            }
            mpHWLI->render(graph);
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

    mServHolder.destruct<SceneGraph>();
    mServHolder.destruct<PluginLoader>();
    mServHolder.destruct<HardwareMemoryOptimizator>();
    mServHolder.destruct<std::unique_ptr<HardwareLayer>>();
    mServHolder.destruct<WindowManager>();
    mServHolder.destruct<AssetStore>();
    mServHolder.destruct<ResourceRegistry>();

    auto shutdownEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

    mpLogger->info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms\n";
}


TemporEngine::~TemporEngine() noexcept {

    mServHolder.destruct<Logger>();

}


