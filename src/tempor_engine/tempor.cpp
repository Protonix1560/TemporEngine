

#include "tempor.hpp"
#include "hardware_common_structs.hpp"
#include "core.hpp"
#include "data_bridge.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "tempor_api.hpp"
#include "window_manager.hpp"

#include <chrono>
#include <csignal>
#include <cstddef>
#include <exception>
#include <memory>

#include <glm/packing.hpp>
#include <nlohmann/json.hpp>

using njson = nlohmann::json;




SIG_SAFE void sigintHandler(int) noexcept {
    gGetServiceLocator()->get<TemporEngine>().sigint();
}

SIG_SAFE void sigtermHandler(int) noexcept {
    gGetServiceLocator()->get<TemporEngine>().sigterm();
}




void TemporEngine::sigint() noexcept {
    mSigInt = 1;
}


void TemporEngine::sigterm() noexcept {
    mSigTerm = 1;
}



int TemporEngine::run(int argc, char* argv[]) noexcept {

    try {

        gGetServiceLocator()->provide(this);
        std::signal(SIGINT, sigintHandler);
        std::signal(SIGTERM, sigtermHandler);

        gGetServiceLocator()->provide(&mLogger);

        mLogger.setVerbosityLevel(6);

        mLogger.info(TPR_LOG_STYLE_STANDART) << "\033[0m";

        ArgParser argParser("tempor");

        auto vFlag = argParser.declareFlag(
            'v', nullptr, 0, 
            "Sets verbosity level.\n"
            "-v is equivalent to --verbose=3.\n"
            "-vv is equivalent to --verbose=4.\n"
            "-vvv is equivalent to --verbose=5.\n"
        );
        argParser.declareFlag(
            'h', "help", ArgParser::FlagParams::FLAG_HELP_MSG, 
            "Shows this message and exits"
        );
        auto vrFlag = argParser.declareFlag(
            0, "verbose", ArgParser::FlagParams::FLAG_HAS_VALUE, 
            "Sets verbosity level [0-6].\nOverrides -v flag"
        );

        ArgParser::ErrCode argParseErr = argParser.parse(argc, argv);
        if (argParseErr != ArgParser::ErrCode::Success) {
            if (argParseErr == ArgParser::ErrCode::HelpCalled) {
                return 0;
            }
            return argParseErr;
        }

        int verboseLevel = 0;
        if (vrFlag.present()) verboseLevel = vrFlag.value<int>();
        else if (vFlag.count() >= 3) verboseLevel = 5;
        else if (vFlag.count() >= 2) verboseLevel = 4;
        else if (vFlag.count() >= 1) verboseLevel = 3;

        init(verboseLevel);
        mainloop();

    } catch (const Exception& e) {
        auto l = mLogger.error(TPR_LOG_STYLE_ERROR1);
        l << "Shutdown after an expected exception [" << e.code() << "]: " << e.what() << "\n";
    } catch (const std::exception& e) {
        auto l = mLogger.error(TPR_LOG_STYLE_ERROR1);
        l << "Shutdown after an unexpected exception: " << e.what() << "\n";
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Shutdown after an unknown exception\n";
    }

    shutdown();
    return 0;
}


void TemporEngine::init(int verboseLevel) {

    auto initStartTimepoint = std::chrono::steady_clock::now();

    auto serviceLocator = gGetServiceLocator();

    mLogger.setVerbosityLevel(verboseLevel);

    mLogger.info(TPR_LOG_STYLE_STANDART) << "Tempor Engine v0.0.0 (build datetime: " << BUILD_DATETIME << ")\033[0m\n";
    mLogger.info(TPR_LOG_STYLE_STARTSTAMP1) << "Startup now\n";

    // mVFSManager.init();
    // serviceLocator->provide(&mVFSManager);
    // mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service VFSManager is ready\n";

    mResourceRegistry.init();
    serviceLocator->provide(&mResourceRegistry);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service ResourceRegistry is ready\n";

    // loading main config
    TprResource confResource = mResourceRegistry.openResource("config.json");
    std::filesystem::path confPath = mResourceRegistry.getResourceFilepath(confResource);
    const std::byte* confBegin = mResourceRegistry.getResourceRawRODataPointer(confResource);
    const std::byte* confEnd = mResourceRegistry.sizeofResource(confResource) + confBegin;

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Located config \"" << confPath.string() << "\"\n";

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

    // loading settings
    njson settingsJson;
    if (mainJson.contains("settings")) {
        // ROFile settingsPacket = mVFSManager.openRO(mainJson["settings"].get<std::string>());
        // ROSpan settingsSpan = settingsPacket.read();
        TprResource settingsRes = mResourceRegistry.openResource(mainJson["settings"].get<std::string>());
        const std::byte* settingsBegin = mResourceRegistry.getResourceRawRODataPointer(settingsRes);
        const std::byte* settingsEnd = settingsBegin + mResourceRegistry.sizeofResource(settingsRes);
        try {
            settingsJson = njson::parse(settingsBegin, settingsEnd);
        } catch (const njson::exception& e) {
            throw Exception(ErrCode::FormatError, "Failed to parse settings \""s + confPath.string() + "\"");
        }
    }

    // reading known settings
    {
        mSettings.vulkanBackendDebugUseKhronosValidationLayer = settingsJson["settings"].value("vulkanBackend.debug.useKhronosValidationLayer", false);
    }

    serviceLocator->provide(&mSettings);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service Settings is ready\n";

    
    // finding sufficient renderer
    auto& rendererFactories = serviceLocator->getListFactories<HardwareLayer>();
    for (const auto& rendererFactory : rendererFactories) {
        try {
            mpHardwareLayer = std::move(rendererFactory());
            mWindowManager.init(mpHardwareLayer->getGraphicsBackend());
            serviceLocator->provide(&mWindowManager);
            mpHardwareLayer->init();

        } catch (const Exception& e) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
            mpHardwareLayer.reset();
            mWindowManager.shutdown();
            serviceLocator->provide<WindowManager>(nullptr);
        } catch (const std::exception& e) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Unexpected exception: " << e.what() << "\n";
            mpHardwareLayer.reset();
            mWindowManager.shutdown();
            serviceLocator->provide<WindowManager>(nullptr);
        } catch (...) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
            mpHardwareLayer.reset();
            mWindowManager.shutdown();
            serviceLocator->provide<WindowManager>(nullptr);
        }
    }
    if (!mpHardwareLayer) throw Exception(ErrCode::NoSupportError, "No supported renderer on this machine");
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service WindowManager is ready\n";
    serviceLocator->provide(mpHardwareLayer.get());
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service HardwareLayer is ready\n";

    auto readyOpeningWindowTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> readyOpeningWindowTime = readyOpeningWindowTimepoint - initStartTimepoint;

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Ready to open a window in " << readyOpeningWindowTime.count() << " ms\n";

    /*
        TODO:
        Potentially minimum amount of loading before showing a loading screen is finished,
        everything past this comment should be moved to another threads
    */


    // TPMLParser TPMLparser(&mServiceLocator);
    // TPMLparser.parse("tp-gui-schema.tpml");
    // {
    //     GUISystem gui;
        
    //     gui.blocks.emplace_back();
    //     gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
    //     gui.blocks.back().bgColour = 0xAA1122FF;
        
    //     gui.blocks.emplace_back();
    //     gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
    //     gui.blocks.back().bgColour = 0xAA1122FF;
        
    //     gui.blocks.emplace_back();
    //     gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
    //     gui.blocks.back().bgColour = 0xAA1122FF;
    //     gui.blocks.back().type = GUIBlock::BlockType::Array;
    //     gui.blocks.back().setWidth = 6.0f;
    //     gui.blocks.back().widthType = GUIBlock::DType::Absolute;
    //     gui.blocks.back().heightType = GUIBlock::DType::Expand;

    //     gui.blocks.emplace_back();
    //     gui.blocks.back().type = GUIBlock::BlockType::Array;
    //     gui.blocks.back().innerPadding = 1.0f;
    //     gui.blocks.back().elementsPadding = 1.0f;
    //     gui.blocks.back().childrenIndices = {0};
    //     gui.blocks.back().typeData.array.alignX = GUIBlock::ArrayAlign::End;
    //     gui.blocks.back().typeData.array.alignY = GUIBlock::ArrayAlign::Centre;
    //     gui.blocks.back().typeData.array.orientation = GUIBlock::Orientation::Vertical;
    //     gui.blocks.back().widthType = GUIBlock::DType::ScreenRelative;
    //     gui.blocks.back().setWidth = -24.0f;
    //     gui.blocks.back().heightType = GUIBlock::DType::Expand;
    //     gui.blocks.back().bgColour = 0xDD11EEFF;

    //     gui.blocks.emplace_back();
    //     GUIBlock& body = gui.blocks.back();
    //     body.type = GUIBlock::BlockType::Array;
    //     body.typeData.array.orientation = GUIBlock::Orientation::Horizontal;
    //     body.innerPadding = 2.0f;
    //     body.elementsPadding = 2.0f;
    //     body.childrenIndices = {1, 3, 2};
    //     body.widthType = GUIBlock::DType::ScreenRelative;
    //     body.setWidth = -4.0f;
    //     body.heightType = GUIBlock::DType::ScreenRelative;
    //     body.setHeight = -4.0f;
    //     body.typeData.array.alignX = GUIBlock::ArrayAlign::SpaceBetween;
    //     body.typeData.array.alignY = GUIBlock::ArrayAlign::Start;
    //     gui.blocks.back().bgColour = 0x050501FF;

    //     mGUIProcessor.init(gui);
    //     serviceLocator->provide(&mGUIProcessor);
    //     mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service GUIProcessor ready\n";
    // }

    mApi.log = &tpr_api::log::log;
    mApi.logInfo = &tpr_api::log::logInfo;
    mApi.logWarn = &tpr_api::log::logWarn;
    mApi.logError = &tpr_api::log::logError;
    mApi.logDebug = &tpr_api::log::logDebug;
    mApi.logTrace = &tpr_api::log::logTrace;
    mApi.logStyled = &tpr_api::log::logStyled;
    mApi.logInfoStyled = &tpr_api::log::logInfoStyled;
    mApi.logWarnStyled = &tpr_api::log::logWarnStyled;
    mApi.logErrorStyled = &tpr_api::log::logErrorStyled;
    mApi.logDebugStyled = &tpr_api::log::logDebugStyled;
    mApi.logTraceStyled = &tpr_api::log::logTraceStyled;

    mApi.scene.registerComponent = &tpr_api::scene::registerComponent;
    mApi.scene.acquireComponent = &tpr_api::scene::acquireComponent;
    mApi.scene.createEntity = &tpr_api::scene::createEntity;
    mApi.scene.destroyEntity = &tpr_api::scene::destroyEntity;
    mApi.scene.modifyEntityComponentSet = &tpr_api::scene::modifyEntityComponentSet;
    mApi.scene.copyEntityComponentData = &tpr_api::scene::copyEntityComponentData;
    mApi.scene.readEntityComponent8bit = &tpr_api::scene::readEntityComponent8bit;
    mApi.scene.readEntityComponent16bit = &tpr_api::scene::readEntityComponent16bit;
    mApi.scene.readEntityComponent32bit = &tpr_api::scene::readEntityComponent32bit;
    mApi.scene.readEntityComponent64bit = &tpr_api::scene::readEntityComponent64bit;
    mApi.scene.writeEntityComponentData = &tpr_api::scene::writeEntityComponentData;
    mApi.scene.writeEntityComponent8bit = &tpr_api::scene::writeEntityComponent8bit;
    mApi.scene.writeEntityComponent16bit = &tpr_api::scene::writeEntityComponent16bit;
    mApi.scene.writeEntityComponent32bit = &tpr_api::scene::writeEntityComponent32bit;
    mApi.scene.writeEntityComponent64bit = &tpr_api::scene::writeEntityComponent64bit;

    mApi.render.growEntityDrawOArray = &tpr_api::render::growEntityDrawOArray;
    mApi.render.endEntityDrawOArray = &tpr_api::render::endEntityDrawOArray;
    mApi.render.sizeofEntityDrawOArray = &tpr_api::render::sizeofEntityDrawOArray;

    mApi.wm.openWindow = &tpr_api::wm::openWindow;
    mApi.wm.closeWindow = &tpr_api::wm::closeWindow;

    mApi.vfs.openPathResource = &tpr_api::vfs::openPathResource;
    mApi.vfs.openRefResource = &tpr_api::vfs::openRefResource;
    mApi.vfs.openEmptyResource = &tpr_api::vfs::openEmptyResource;
    mApi.vfs.openCapabilityResource = &tpr_api::vfs::openCapabilityResource;
    mApi.vfs.openPathResourceLifetimed = &tpr_api::vfs::openPathResourceLifetimed;
    mApi.vfs.openRefResourceLifetimed = &tpr_api::vfs::openRefResourceLifetimed;
    mApi.vfs.openEmptyResourceLifetimed = &tpr_api::vfs::openEmptyResourceLifetimed;
    mApi.vfs.openCapabilityResourceLifetimed = &tpr_api::vfs::openCapabilityResourceLifetimed;
    mApi.vfs.resetResourceLifetime = &tpr_api::vfs::resetResourceLifetime;
    mApi.vfs.resizeResource = &tpr_api::vfs::resizeResource;
    mApi.vfs.sizeofResource = &tpr_api::vfs::sizeofResource;
    mApi.vfs.getResourceRawRWDataPointer = &tpr_api::vfs::getResourceRawRWDataPointer;
    mApi.vfs.getResourceRawRODataPointer = &tpr_api::vfs::getResourceRawRODataPointer;
    mApi.vfs.closeResource = &tpr_api::vfs::closeResource;

    mApi.geo.parseAsset = &tpr_api::geo::parseAsset;

    serviceLocator->provide(&mApi);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service API is ready\n";

    mSceneManager.init();
    serviceLocator->provide(&mSceneManager);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service SceneManager is ready\n";

    mDataBridge.init();
    serviceLocator->provide(&mDataBridge);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service DataBridge is ready\n";

    mAssetStore.init();
    serviceLocator->provide(&mAssetStore);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service AssetStore is ready\n";

    mPluginLauncher.init(&mApi);
    serviceLocator->provide(&mPluginLauncher);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service PluginLauncher is ready\n";

    auto plugins = mResourceRegistry.enumDir("plugins", TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT, 1);
    for (const auto& plugin : plugins) {
        try {
            mPluginLauncher.load(plugin);
        } catch (...) {}
    }

    auto initEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> initTime = initEndTimepoint - initStartTimepoint;

    mHWMemOptimizator.init();
    serviceLocator->provide(&mHWMemOptimizator);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Service HardwareMemoryOptimizator is ready\n";

    mLogger.info(TPR_LOG_STYLE_ENDSTAMP1) << "Initialization done in " << initTime.count() << " ms\n";
    
}



void TemporEngine::mainloop() {

    mClock.begin();

    while (!mWindowManager.lost() && !shouldStop) {

        mPluginLauncher.triggerHook<TPR_HOOK_UPDATE_PER_FRAME>();

        mDataBridge.update();
        mResourceRegistry.update();
        mPluginLauncher.update();
        mSceneManager.update();
        mAssetStore.update();
        mHWMemOptimizator.update();
        mWindowManager.update();
        mpHardwareLayer->update();

        // render
        {
            RenderGraph graph;
            
            for (auto& handle : mWindowManager.getWindows()) {
                RenderGraph::WindowConfig& conf = graph.windows.emplace_back(handle, RenderGraph::WindowConfig{}).second;
                conf.scissor = Scissor{0, 0, mpHardwareLayer->getFrameWidth(handle), mpHardwareLayer->getFrameHeight(handle)};
                conf.viewport = Viewport{
                    0.0f, 0.0f, static_cast<float>(mpHardwareLayer->getFrameWidth(handle)), 
                    static_cast<float>(mpHardwareLayer->getFrameHeight(handle)), 0.0f, 1.0f
                };

            }

            auto drawEntityOArray = mDataBridge.registerOArray<TprOArrayEntityDrawDesc>(
                [&graph]() -> void* { return graph.entites.data(); },
                [&graph](size_t newSize) { graph.entites.resize(newSize); }, 0
            );
            mPluginLauncher.triggerHook<TPR_HOOK_GET_ENTITY_DRAW_ARRAY>(drawEntityOArray);

            mpHardwareLayer->render(graph);
        }

        if (mSigInt || mSigTerm) {
            if (mSigInt) {
                mLogger << "\n";
            }
            auto l = mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1);
            l << "Received ";
            if (mSigInt) {
                l << "SIG_INT";
            } else if (mSigTerm) {
                l << "SIG_TERM";
            }
            l << " signal\n";
            shouldStop = true;
        }

        // setting window name with fps count
        // if (mTitleUpdateClock.ticked()) {
        //     std::string title = "Tempor Testing Initiative | fps: "s + std::to_string(mClock.estimateFps());
        //     mWindowManager.setTitle(title.c_str());
        // }

        mClock.tick();
    }

}



void TemporEngine::shutdown() noexcept {

    auto shutdownStartTimepoint = std::chrono::steady_clock::now();

    mLogger.info(TPR_LOG_STYLE_STARTSTAMP1) << "Shutting down...\n";

    mPluginLauncher.triggerHook<TPR_HOOK_SHUTDOWN>();

    mHWMemOptimizator.shutdown();

    mPluginLauncher.shutdown();
    gGetServiceLocator()->provide<PluginLauncher>(nullptr);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service PluginLauncher\n";

    mDataBridge.shutdown();
    gGetServiceLocator()->provide<DataBridge>(nullptr);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service DataBridge\n";

    mSceneManager.shutdown();
    gGetServiceLocator()->provide<SceneManager>(nullptr);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service SceneManager\n";

    // mGUIProcessor.shutdown();

    if (mpHardwareLayer) {
        mpHardwareLayer->shutdown();
        gGetServiceLocator()->provide<HardwareLayer>(nullptr);
        mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service Renderer\n";
    }

    mWindowManager.shutdown();
    gGetServiceLocator()->provide<WindowManager>(nullptr);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service WindowManager\n";

    // mVFSManager.shutdown();
    // gGetServiceLocator()->provide<VFSManager>(nullptr);
    // mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service VFSManager\n";

    mResourceRegistry.shutdown();
    gGetServiceLocator()->provide<ResourceRegistry>(nullptr);
    mLogger.trace(TPR_LOG_STYLE_TIMESTAMP1) << "Stopped service ResourceRegistry\n";

    auto shutdownEndTimepoint = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> shutdownTime = shutdownEndTimepoint - shutdownStartTimepoint;

    mLogger.info(TPR_LOG_STYLE_ENDSTAMP1) << "Shutdown finished in " << shutdownTime.count() << " ms\n";

}


