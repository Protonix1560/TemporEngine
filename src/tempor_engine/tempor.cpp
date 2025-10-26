

#include "tempor.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "vulkan_backend.hpp"

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

        initialize(verboseLevel);
        mainloop();

    } catch (const Exception& e) {
        mLogger.error(LogStyle::Error1) << "Shutdown after an expected exception [" << e.code() << "]: " << e.what() << "\n";
    } catch (const std::exception& e) {
        mLogger.error(LogStyle::Error1) << "Shutdown after an unexpected exception: " << e.what() << "\n";
    } catch (...) {
        mLogger.error(LogStyle::Error1) << "Shutdown after an unknown exception\n";
    }

    shutdown();
    return 0;
}


void TemporEngine::initialize(int verboseLevel) {

    mLogger.setVerbosityLevel(verboseLevel);

    // registring global services
    mServiceLocator.provide(&mIO);
    mServiceLocator.provide(&mLogger);
    mServiceLocator.provide(&mSettings);

    // loading main config
    std::filesystem::path mainJsonPath;
    if (auto path = std::getenv("TEMPOR_MAIN_PATH")) mainJsonPath = path;
    else mainJsonPath = "tp-main.json";
    if (!std::filesystem::is_regular_file(mainJsonPath)) {
        throw Exception(ErrCode::IOError, "Failed to locate main config");
    }

    auto mainJsonBin = mIO.readAll(mainJsonPath);

    mLogger.info(LogStyle::Timestamp1) << "Located main config at `" << mainJsonPath.string() << "`\n";

    njson mainJson;
    try {
        mainJson = njson::parse(mainJsonBin.begin(), mainJsonBin.end());
    } catch (const njson::exception& e) {
        throw Exception(ErrCode::FormatError, "Failed to parse main config: `"s + mainJsonPath.string() + "`"s);
    }

    // reading configs
    if (!(
        mainJson.contains("asset") && mainJson["asset"].contains("format") &&
        mainJson["asset"]["format"].get<std::string>() == "tempor" && mainJson["asset"].contains("version")
    )) {
        throw Exception(ErrCode::FormatError, "Main config `"s + mainJsonPath.string() + "` is corrupted"s);
    }

    int major = 0, minor = 0, patch = 0;
    {
        std::string mainJsonVersionStr = mainJson["asset"]["version"].get<std::string>();
        int* numbers[] = {&major, &minor, &patch};
        size_t verDigitSize = 0;
        int i = 0;
        while (verDigitSize != std::string::npos) {
            if (i == std::size(numbers)) throw Exception(ErrCode::FormatError, "Invalid main config `"s + mainJsonPath.string() + "` version"s);
            verDigitSize = mainJsonVersionStr.find('.');
            *numbers[i] = std::stoi(mainJsonVersionStr.substr(0, verDigitSize));
            mainJsonVersionStr = mainJsonVersionStr.substr(verDigitSize + 1);
            i++;
        }
    }

    // loading settings
    njson settingsJson;
    if (mainJson.contains("settings")) {
        auto settingsBin = mIO.readAll(mainJson["settings"].get<std::string>());
        try {
            settingsJson = njson::parse(settingsBin.begin(), settingsBin.end());
        } catch (const njson::exception& e) {
            throw Exception(ErrCode::FormatError, "Failed to parse settings: `"s + mainJsonPath.string() + "`"s);
        }
    }

    // reading settings
    {
        mSettings.vulkanBackend_debug_useKhronosValidationLayer = settingsJson["settings"].value("vulkanBackend.debug.useKhronosValidationLayer", false);
    }


    mWindowManager = std::make_unique<WindowManager>();
    
    // finding sufficient backend realization
    BackendRealization backends[] = {BackendRealization::VULKAN};
    for (const auto& real : backends) {
        try {
            mWindowManager->init(1300, 800, "Tempor Testing Initiative", real);

            switch (real) {
                case BackendRealization::VULKAN:
                    mBackend = std::make_unique<BackendVk>();
                    mBackend->init(mWindowManager.get(), &mServiceLocator);
            }

        } catch (const Exception& e) {
            mLogger.error(LogStyle::Error1) << "Exception [" << e.code() << "]: " << e.what() << "\n";
            mBackend.reset();
            mWindowManager->destroy();
        }
    }
    if (!mBackend) throw Exception(ErrCode::NoSupportError, "No supported backend realization on this machine");

    mWindowManager->showWindow();

    // TPMLParser TPMLparser(&mServiceLocator);
    // TPMLparser.parse("tp-gui-schema.tpml");
    {
        GUISystem gui;
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";
        
        gui.blocks.emplace_back();
        gui.blocks.back().text = "SOME TEXT\n SOME\nOTHER\n TEXT\n";

        gui.blocks.emplace_back();
        gui.blocks.back().type = GUIBlock::BlockType::Array;
        gui.blocks.back().innerPadding = 1.0f;
        gui.blocks.back().elementsPadding = 1.0f;
        gui.blocks.back().childrenIndices = {0, 1};
        gui.blocks.back().typeData.arrayData.alignX = GUIBlock::ArrayAlign::SpaceAround;
        gui.blocks.back().widthType = GUIBlock::DType::Absolute;
        gui.blocks.back().setWidth = 15.0f;
        gui.blocks.back().heightType = GUIBlock::DType::Absolute;
        gui.blocks.back().setHeight = 10.0f;
        gui.blocks.back().typeData.arrayData.alignX = GUIBlock::ArrayAlign::SpaceAround;

        gui.blocks.emplace_back();
        GUIBlock& body = gui.blocks.back();
        body.type = GUIBlock::BlockType::Array;
        body.typeData.arrayData.orientation = GUIBlock::Orientation::Horizontal;
        body.innerPadding = 1.0f;
        body.elementsPadding = 1.0f;
        body.childrenIndices = {2, 3};
        body.widthType = GUIBlock::DType::ScreenPercentage;
        body.setWidth = 90.0f;
        body.heightType = GUIBlock::DType::ScreenPercentage;
        body.setHeight = 90.0f;
        body.typeData.arrayData.alignX = GUIBlock::ArrayAlign::SpaceAround;
        body.typeData.arrayData.alignY = GUIBlock::ArrayAlign::End;

        mGUIProcessor.init(&mServiceLocator, gui);
    }
    
}



void TemporEngine::mainloop() {

    static std::chrono::time_point<std::chrono::high_resolution_clock> lastTitleUpdateTime = std::chrono::high_resolution_clock::now();

    while (!mWindowManager->shouldClose()) {

        mWindowManager->update();
        mGUIProcessor.update(mWindowManager->getWidth(), mWindowManager->getHeight());
        mBackend->update();
        mBackend->beginRenderPass();

        std::vector<DebugLineVertex> lines;
        
        {
            auto guiLines = mGUIProcessor.getDebugViewLines();
            lines.insert(
                lines.end(),
                std::make_move_iterator(guiLines.begin()),
                std::make_move_iterator(guiLines.end())
            );
        }

        // lines.push_back({{0.0f, 0.0f, 0.0f}, glm::packUnorm4x8({0.0f, 1.0f, 0.0f, 1.0f})});
        // lines.push_back({{0.2f, 0.2f, 1.0f}, glm::packUnorm4x8({1.0f, 0.0f, 0.0f, 1.0f})});

        mBackend->renderDebugLines(lines);
        mBackend->endRenderPass();

        mClock.tick();

        // setting window name with fps count
        {
            auto currentTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsedTime = currentTime - lastTitleUpdateTime;
            if (elapsedTime.count() >= 0.25) {
                std::string title = "Tempor Testing Initiative | fps: "s + std::to_string(static_cast<int>(std::ceil(mClock.estimateFps())));
                mWindowManager->setTitle(title.c_str());
                lastTitleUpdateTime = currentTime;
            }
        }
    }

}



void TemporEngine::shutdown() noexcept {

    mLogger.info(LogStyle::Timestamp1) << "Shutting down...\n";

    if (mBackend) mBackend->destroy();

    if (mWindowManager) mWindowManager->destroy();

    mLogger.info(LogStyle::Timestamp1) << "Shutdown finished\n";

}


