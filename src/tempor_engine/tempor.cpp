

#include "tempor.hpp"
#include "core.hpp"
#include "io.hpp"
#include "plugin.h"
#include "renderer.hpp"
#include "logger.hpp"
#include "renderer_vulkan.hpp"

#include <memory>

#include <glm/packing.hpp>
#include <nlohmann/json.hpp>

using njson = nlohmann::json;





int TemporEngine::run(int argc, char* argv[]) noexcept {

    try {

        mLogger.setVerbosityLevel(6);

        ArgParser argParser("tempor");

        auto vFlag = argParser.declareFlag(
            'v', nullptr, 0, 
            "Sets verbosity level to 3 when occures once.\n"
            "Sets verbosity level to 4 when occures twice.\n"
            "Sets verbosity level to 5 when occures 3 times"
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

        int verboseLevel;
        if (vrFlag.present()) verboseLevel = vrFlag.value<int>();
        else if (vFlag.count() >= 3) verboseLevel = 5;
        else if (vFlag.count() >= 2) verboseLevel = 4;
        else if (vFlag.count() >= 1) verboseLevel = 3;

        init(verboseLevel);
        mainloop();

    } catch (const Exception& e) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Shutdown after an expected exception [" << e.code() << "]: " << e.what() << "\n";
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Shutdown after an unexpected exception: " << e.what() << "\n";
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Shutdown after an unknown exception\n";
    }

    shutdown();
    return 0;
}


void TemporEngine::init(int verboseLevel) {

    mLogger.setVerbosityLevel(verboseLevel);
    mIO.init(&mServiceLocator);

    mApi.log = &tprapi::log;
    mApi.logInfo = &tprapi::logInfo;
    mApi.logWarn = &tprapi::logWarn;
    mApi.logError = &tprapi::logError;
    mApi.logDebug = &tprapi::logDebug;
    mApi.logTrace = &tprapi::logTrace;
    mApi.logStyled = &tprapi::logStyled;
    mApi.logInfoStyled = &tprapi::logInfoStyled;
    mApi.logWarnStyled = &tprapi::logWarnStyled;
    mApi.logErrorStyled = &tprapi::logErrorStyled;
    mApi.logDebugStyled = &tprapi::logDebugStyled;
    mApi.logTraceStyled = &tprapi::logTraceStyled;

    // registring global services
    mServiceLocator.provide(&mIO);
    mServiceLocator.provide(&mLogger);
    mServiceLocator.provide(&mSettings);
    mServiceLocator.provide(&mApi);

    swapServiceLocator(&mServiceLocator);



    // loading main config
    std::filesystem::path confPath;
    if (auto path = std::getenv("TEMPOR_CONF_PATH")) confPath = path;
    else confPath = "config.json";
    if (!std::filesystem::is_regular_file(confPath)) {
        throw Exception(ErrCode::IOError, "Failed to locate config");
    }

    ROPacket confPacket = mIO.openRO(confPath);
    ROSpan confSpan = confPacket.read();

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Located config \"" << confPath.string() << "\"\n";

    njson mainJson;
    try {
        mainJson = njson::parse(confSpan.begin(), confSpan.end());
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
        ROPacket settingsPacket = mIO.openRO(mainJson["settings"].get<std::string>());
        ROSpan settingsSpan = settingsPacket.read();
        try {
            settingsJson = njson::parse(settingsSpan.begin(), settingsSpan.end());
        } catch (const njson::exception& e) {
            throw Exception(ErrCode::FormatError, "Failed to parse settings \""s + confPath.string() + "\"");
        }
    }

    // reading settings
    {
        mSettings.vulkanBackend_debug_useKhronosValidationLayer = settingsJson["settings"].value("vulkanBackend.debug.useKhronosValidationLayer", false);
    }


    mpWindowManager = std::make_unique<WindowManager>();
    
    // finding sufficient renderer
    BackendRealization renderers[] = {BackendRealization::VULKAN};
    for (const auto& renderer : renderers) {
        try {
            mpWindowManager->init(1300, 800, "Tempor Testing Initiative", renderer);

            switch (renderer) {
                case BackendRealization::VULKAN:
                    mpRenderer = std::make_unique<RendererVk>();
                    mpRenderer->init(mpWindowManager.get(), &mServiceLocator);
            }

        } catch (const Exception& e) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Exception [" << e.code() << "]: " << e.what() << "\n";
            mpRenderer.reset();
            mpWindowManager->shutdown();
        }
    }
    if (!mpRenderer) throw Exception(ErrCode::NoSupportError, "No supported renderer on this machine");

    mpWindowManager->showWindow();

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Ready to open a window\n";

    /*
        TODO:
        Potentially minimum amount of loading before showing a loading screen is finished,
        everything past this comment should be moved to another threads
    */

    // TPMLParser TPMLparser(&mServiceLocator);
    // TPMLparser.parse("tp-gui-schema.tpml");
    {
        GUISystem gui;
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
        gui.blocks.back().bgColour = 0xAA1122FF;
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
        gui.blocks.back().bgColour = 0xAA1122FF;
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
        gui.blocks.back().bgColour = 0xAA1122FF;
        gui.blocks.back().type = GUIBlock::BlockType::Array;
        gui.blocks.back().setWidth = 6.0f;
        gui.blocks.back().widthType = GUIBlock::DType::Absolute;
        gui.blocks.back().heightType = GUIBlock::DType::Expand;

        gui.blocks.emplace_back();
        gui.blocks.back().type = GUIBlock::BlockType::Array;
        gui.blocks.back().innerPadding = 1.0f;
        gui.blocks.back().elementsPadding = 1.0f;
        gui.blocks.back().childrenIndices = {0};
        gui.blocks.back().typeData.array.alignX = GUIBlock::ArrayAlign::End;
        gui.blocks.back().typeData.array.alignY = GUIBlock::ArrayAlign::Centre;
        gui.blocks.back().typeData.array.orientation = GUIBlock::Orientation::Vertical;
        gui.blocks.back().widthType = GUIBlock::DType::ScreenRelative;
        gui.blocks.back().setWidth = -24.0f;
        gui.blocks.back().heightType = GUIBlock::DType::Expand;
        gui.blocks.back().bgColour = 0xDD11EEFF;

        gui.blocks.emplace_back();
        GUIBlock& body = gui.blocks.back();
        body.type = GUIBlock::BlockType::Array;
        body.typeData.array.orientation = GUIBlock::Orientation::Horizontal;
        body.innerPadding = 2.0f;
        body.elementsPadding = 2.0f;
        body.childrenIndices = {1, 3, 2};
        body.widthType = GUIBlock::DType::ScreenRelative;
        body.setWidth = -4.0f;
        body.heightType = GUIBlock::DType::ScreenRelative;
        body.setHeight = -4.0f;
        body.typeData.array.alignX = GUIBlock::ArrayAlign::SpaceBetween;
        body.typeData.array.alignY = GUIBlock::ArrayAlign::Start;
        gui.blocks.back().bgColour = 0x050501FF;

        mGUIProcessor.init(&mServiceLocator, gui);
    }


    mPluginLauncher.init(&mServiceLocator, &mApi);

    try {
        mPluginLauncher.load("plugins/libtest_plugin.so");
    } catch (...) {
    }
    
}



void TemporEngine::mainloop() {

    // static auto lastTitleUpdateTime = std::chrono::high_resolution_clock::now();

    mMainClock.begin();
    mTitleUpdateClock.begin();

    while (!mpWindowManager->shouldClose()) {

        mPluginLauncher.triggerHook(TPR_HOOK_UPDATE_PER_FRAME);

        mPluginLauncher.update();
        mpWindowManager->update();
        mGUIProcessor.update(mpWindowManager->getWidth(), mpWindowManager->getHeight());
        mpRenderer->update();

        mpRenderer->beginRenderPass();

        mpRenderer->nextSubpass();

        mpRenderer->renderGUI(mGUIProcessor.getDrawDesc());

        std::vector<DebugLineVertex> lines;
        {
            auto guiLines = mGUIProcessor.getDebugViewLines();
            lines.insert(
                lines.end(),
                std::make_move_iterator(guiLines.begin()),
                std::make_move_iterator(guiLines.end())
            );
        }
        // mBackend->renderDebugLines(lines);

        mpRenderer->endRenderPass();

        mMainClock.tick();

        // setting window name with fps count
        if (mTitleUpdateClock.ticked()) {
            std::string title = "Tempor Testing Initiative | fps: "s + std::to_string(mMainClock.estimateFps());
            mpWindowManager->setTitle(title.c_str());
        }
    }

}



void TemporEngine::shutdown() noexcept {

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Shutting down...\n";

    mPluginLauncher.shutdown();

    mGUIProcessor.shutdown();

    if (mpRenderer) mpRenderer->shutdown();

    if (mpWindowManager) mpWindowManager->shutdown();

    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Shutdown finished\n";

}


