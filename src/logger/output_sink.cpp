
#include "core.hpp"
#include "logger.hpp"
#include "tempor.h"
#include "output_sink.hpp"

#include <cstdio>
#include <mutex>
#include <string_view>


OutputSink::OutputSink(TprLogLevel termLevel, bool allowTermColour) {
    if (termLevel > 0) {
        LogDest dest{};
        // best-effort heuristic
        #ifdef POSIX
            if (allowTermColour) dest.allowAnsiColour = isatty(STDOUT_FILENO);
        #endif
        dest.file = stderr;
        dest.level = termLevel;
        mLogDests.push_back(dest);
        mMaxVerbosity = termLevel;
    }
}

OutputSink::~OutputSink() noexcept {
    for (const auto& dest : mLogDests) {
        if (dest.file && dest.file != stderr) {
            fclose(dest.file);
        }
    }
}

void OutputSink::addLogFile(TprLogLevel level, std::string_view path) {
    auto file = fopen(path.data(), "w");
    if (!file) return;
    std::lock_guard<std::mutex> lock(mMutex);
    auto& dest = mLogDests.emplace_back(level, file);
    for (const auto& entry : mHistory) {
        auto colourlessExp = formatted(entry.message, entry.style, false);
        if (!colourlessExp.has_value()) continue;
        std::fprintf(dest.file, "%s", colourlessExp.value().c_str());
    }
    std::fflush(dest.file);
}

TprResult OutputSink::writeData(std::span<const std::byte> data) noexcept {
    if (data.empty()) return TPR_SUCCESS;
    std::lock_guard<std::mutex> lock(mMutex);
    std::fprintf(stdout, "%p", data.data());
    std::fflush(stdout);
    return TPR_SUCCESS;
}

TprLogLevel OutputSink::maxLevel() const {
    return mMaxVerbosity;
}

void OutputSink::stopHistory() {
    std::lock_guard<std::mutex> lock(mMutex);
    mSaveHistory = false;
    mHistory.clear();
}
