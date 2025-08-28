

#include "tempor.hpp"
#include "core.hpp"
#include "vulkan/backend.hpp"

#include <cassert>
#include <exception>
#include <filesystem>
#include <memory>

#include <nlohmann/json.hpp>
#include <string>

using njson = nlohmann::json;





int TemporEngine::run(int argc, char* argv[]) noexcept {

    try {

        initialize();
        mainloop();

    } catch (const Exception& e) {
        mLogger << "Shutdown after an expected exception [" << e.code() << "]: " << e.what() << "\n";
    } catch (const std::exception& e) {
        mLogger << "Shutdown after an unexpected exception: " << e.what() << "\n";
    } catch (...) {
        mLogger << "Shutdown after an unknown exception\n";
    }

    shutdown();
    return 0;
}


void TemporEngine::initialize() {

    // registring global services
    mServiceLocator.provide(&mIO);
    mServiceLocator.provide(&mLogger);

    // locating main json file
    std::filesystem::path mainJsonPath;
    if (auto path = std::getenv("TEMPOR_MAIN_PATH")) mainJsonPath = path;
    else mainJsonPath = "tp-main.json";
    if (!std::filesystem::is_regular_file(mainJsonPath)) {
        throw Exception(ErrCode::IOError, "Failed to locate main json file");
    }

    auto mainJsonBin = mIO.readAll(mainJsonPath);

    mLogger << "Located main json file\n";

    njson mainJson;
    try {
        mainJson = njson::parse(mainJsonBin.begin(), mainJsonBin.end());
    } catch (const njson::exception& e) {
        throw Exception(ErrCode::FormatError, "Failed to parse main json file "s + mainJsonPath.string());
    }

    // reading configs
    if (!(
        mainJson.contains("asset") && mainJson["asset"].contains("format") &&
        mainJson["asset"]["format"].get<std::string>() == "tempor" && mainJson["asset"].contains("version")
    )) {
        throw Exception(ErrCode::FormatError, "Main json file "s + mainJsonPath.string() + " is corrupted"s);
    }

    int major = 0, minor = 0, patch = 0;
    {
        std::string mainJsonVersionStr = mainJson["asset"]["version"].get<std::string>();
        int* numbers[] = {&major, &minor, &patch};
        size_t verDigitSize = 0;
        int i = 0;
        while (verDigitSize != std::string::npos) {
            if (i == std::size(numbers)) throw Exception(ErrCode::FormatError, "Invalid main json file "s + mainJsonPath.string() + " version"s);
            verDigitSize = mainJsonVersionStr.find('.');
            *numbers[i] = std::stoi(mainJsonVersionStr.substr(0, verDigitSize));
            mainJsonVersionStr = mainJsonVersionStr.substr(verDigitSize + 1);
            i++;
        }
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
            mLogger << "Exception [" << e.code() << "]: " << e.what() << "\n";
            mBackend.reset();
        }
    }
    if (!mBackend) throw Exception(ErrCode::NoSupportError, "No supported backend realization on machine");

    mWindowManager->showWindow();
    
}



void TemporEngine::mainloop() {

    static std::chrono::time_point<std::chrono::high_resolution_clock> lastTitleUpdateTime = std::chrono::high_resolution_clock::now();

    while (!mWindowManager->shouldClose()) {

        mWindowManager->update();

        mBackend->update();
        mBackend->render();

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

    mLogger << "Shutting down...\n";

    if (mBackend) mBackend->destroy();

    if (mWindowManager) mWindowManager->destroy();

    mLogger << "Reached shutdown\n";

}


