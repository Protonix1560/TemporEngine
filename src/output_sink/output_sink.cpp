
#include "output_sink.hpp"
#include "plugin_core.h"
#include "time.hpp"
#include "settings.hpp"
#include "file_registry.hpp"

#include <cstdio>
#include <mutex>



expected<std::string, TprResult> formatted(std::string_view msg, TprLogStyle style) {
    std::string f;

    std::string nsep;

    switch (style) {
        case TPR_LOG_STYLE_2IDENT:
            f = "  "; nsep = "  "; break;
        case TPR_LOG_STYLE_6IDENT:
            f = "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_TIMESTAMP1:
            f = "  > [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ERROR1:
            f = "      \033[91m"; nsep = "      "; break;
        case TPR_LOG_STYLE_WARN1:
            f =  "      \033[93m"; nsep = "      "; break;
        case TPR_LOG_STYLE_SUCCESS1:
            f = "      \033[102m"; nsep = "      "; break;
        case TPR_LOG_STYLE_STARTSTAMP1:
            f = "\033[35mr->\033[0m [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ENDSTAMP1:
            f = "\033[32ml->\033[0m [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_PANIC1:
            f = "\033[95m"; break;
        case TPR_LOG_STYLE_STANDART: break;
        default: return unexpected(TPR_ERROR_INVALID_VALUE);
    }

    if (style != TPR_LOG_STYLE_STANDART) {
        std::string_view view = msg;
        size_t nlinepos = 0;
        while (nlinepos != std::string_view::npos) {
            nlinepos = view.find('\n');
            if (nlinepos == std::string_view::npos) {
                f += view;
                f += "\n";
            } else {
                f += view.substr(0, nlinepos + 1);
                view = view.substr(nlinepos + 1);
                if (!view.empty()) {
                    f += nsep;
                }
                if (view.empty()) break;
            }
        }
    } else {
        f = msg;
    }
    f += "\033[0m";
    return f;
}


expected<std::string, TprResult> formattedColourless(std::string_view msg, TprLogStyle style) {
    std::string f;

    std::string nsep;

    switch (style) {
        case TPR_LOG_STYLE_2IDENT:
            f = "  "; nsep = "  "; break;
        case TPR_LOG_STYLE_6IDENT:
            f = "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_TIMESTAMP1:
            f = "  > [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ERROR1:
            f = "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_WARN1:
            f =  "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_SUCCESS1:
            f = "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_STARTSTAMP1:
            f = "r-> [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ENDSTAMP1:
            f = "l-> [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_PANIC1:
            f = ""; break;
        case TPR_LOG_STYLE_STANDART: break;
        default: return unexpected(TPR_ERROR_INVALID_VALUE);
    }

    if (style != TPR_LOG_STYLE_STANDART) {
        std::string_view view = msg;
        size_t nlinepos = 0;
        while (nlinepos != std::string_view::npos) {
            nlinepos = view.find('\n');
            if (nlinepos == std::string_view::npos) {
                f += view;
                f += "\n";
            } else {
                f += view.substr(0, nlinepos + 1);
                view = view.substr(nlinepos + 1);
                if (!view.empty()) {
                    f += nsep;
                }
                if (view.empty()) break;
            }
        }
    } else {
        f = msg;
    }
    return f;
}


TermSink::TermSink(TprLogLevel termVerbosity, bool colourEnabled) : mTermVerbosity(termVerbosity), mColourEnabled(colourEnabled) {
    std::fprintf(stderr, "\033[0m");
    std::fflush(stderr);
}

void TermSink::writeLog(std::string_view msg, TprLogLevel level, TprLogStyle style) noexcept {
    if (msg.empty()) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto expColourless = formattedColourless(msg, style);
        if (!expColourless.has_value()) return;
        mLogHistory.emplace_back(expColourless.value(), level);
        if (level <= mTermVerbosity) {
            if (mColourEnabled) {
                auto expNorm = formatted(msg, style);
                if (!expNorm.has_value()) return;
                std::fprintf(stderr, "%s", expNorm.value().c_str());
            } else {
                std::fprintf(stderr, "%s", expColourless.value().c_str());
            }
            std::fflush(stderr);
        }
    } catch (...) {}
}

TprResult TermSink::writeData(std::span<const std::byte> data) noexcept {
    if (data.empty()) return TPR_SUCCESS;
    std::lock_guard<std::mutex> lock(mMutex);
    std::fprintf(stdout, "%p", data.data());
    std::fflush(stdout);
    return TPR_SUCCESS;
}

TprLogLevel TermSink::maxVerbosity() const {
    return maxLogLevel;
}

std::span<const HistoryEntry> TermSink::logHistory() const {
    return std::span(mLogHistory.begin(), mLogHistory.end());
}

TprLogLevel TermSink::termVerbosity() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mTermVerbosity;
}

bool TermSink::colourEnabled() const {
    return mColourEnabled;
}



TermFileSink::TermFileSink(
    Settings& rSettings, FileRegistry& rResReg, const TermSink& rTermSink, TprLogLevel termVerbosity, bool colourEnabled
) : mrSettings(rSettings), mrFileReg(rResReg), mTermVerbosity(termVerbosity), mColourEnabled(colourEnabled) {

    size_t maxVerbosity = mTermVerbosity;

    auto logsExp = mrSettings.createSetting(mrSettings.getRoot(), "logFiles");
    if (logsExp.has_value()) {
        auto logs = logsExp.value();
        auto sizeExp = mrSettings.getSettingArraySize(logs);
        if (sizeExp.has_value()) {
            auto size = sizeExp.value();
            for (uint32_t i = 0; i < size; i++) {
                auto logExp = mrSettings.getSettingArrayElement(logs, i);
                if (!logExp.has_value()) continue;
                auto log = logExp.value();

                auto pathSettingExp = mrSettings.createSetting(log, "path");
                if (!pathSettingExp.has_value()) continue;
                auto pathSetting = pathSettingExp.value();
                auto pathSizeExp = mrSettings.getSettingStringSize(pathSetting);
                if (!pathSizeExp.has_value()) continue;
                std::string path(pathSizeExp.value(), '\0');
                if (mrSettings.copySettingString(pathSetting, path.data()) != TPR_SUCCESS) continue;

                auto levelSettingExp = mrSettings.createSetting(log, "level");
                if (!levelSettingExp.has_value()) continue;
                auto levelSetting = levelSettingExp.value();
                auto levelExp = mrSettings.getSettingInteger(levelSetting);
                if (!levelExp.has_value()) continue;
                auto level = levelExp.value();
                if (level < 0 || level > maxLogLevel) continue;

                auto fileExp = mrFileReg.openFile(path, TPR_OPEN_FILE_SYNC_FLAG_BIT | TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT);
                if (!fileExp.has_value()) continue;

                mLogDests.emplace_back(fileExp.value(), static_cast<TprLogLevel>(level));
                if (level > maxVerbosity) {
                    maxVerbosity = level;
                }
            }
        }
    }
    mMaxVerbosity = static_cast<TprLogLevel>(maxVerbosity);
    auto history = rTermSink.logHistory();
    for (const auto& entry : history) {
        writeToFiles(entry.log, entry.level);
    }
}

void TermFileSink::writeToFiles(std::string_view msg, TprLogLevel level) {
    for (auto& logDest : mLogDests) {
        if (level <= logDest.verbosity) {
            mrFileReg.append(logDest.file, msg.size(), reinterpret_cast<const std::byte*>(msg.data()));
        }
    }
}

void TermFileSink::writeLog(std::string_view msg, TprLogLevel level, TprLogStyle style) noexcept {
    if (msg.empty()) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto expColourless = formattedColourless(msg, style);
        if (!expColourless.has_value()) return;
        writeToFiles(expColourless.value(), level);
        if (level <= mTermVerbosity) {
            if (mColourEnabled) {
                auto expNorm = formatted(msg, style);
                if (!expNorm.has_value()) return;
                std::fprintf(stderr, "%s", expNorm.value().c_str());
            } else {
                std::fprintf(stderr, "%s", expColourless.value().c_str());
            }
        }
    } catch (...) {}
}

TprResult TermFileSink::writeData(std::span<const std::byte> data) noexcept {
    if (data.empty()) return TPR_SUCCESS;
    std::lock_guard<std::mutex> lock(mMutex);
    std::fprintf(stdout, "%p", data.data());
    std::fflush(stdout);
    return TPR_SUCCESS;
}

TprLogLevel TermFileSink::maxVerbosity() const {
    return mMaxVerbosity;
}

bool TermFileSink::colourEnabled() const {
    return mColourEnabled;
}
