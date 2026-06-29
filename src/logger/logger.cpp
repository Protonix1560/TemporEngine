
#include "logger.hpp"
#include "plugin_core.h"

#include <memory>
#include <string_view>
#include <cstdio>


Logger::Logger(const std::atomic<std::shared_ptr<LogSinkInterface>>& rSink, std::string_view prefix)
    : mrSink(rSink), mPrefix(prefix) {}

LogEntry Logger::operator()(TprLogLevel level, TprLogStyle style) const {
    return LogEntry(mrSink.load(), mPrefix, level, style);
}

std::shared_ptr<LogSinkInterface> Logger::sink() const noexcept {
    return mrSink.load();
}

Logger Logger::derive(const std::string& prefix) const {
    return Logger(mrSink, mPrefix + prefix);
}

LogEntry Logger::panic(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_PANIC, style); }
LogEntry Logger::error(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_ERROR, style); }
LogEntry Logger::warn(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_WARN, style); }
LogEntry Logger::info(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_INFO, style); }
LogEntry Logger::debug(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_DEBUG, style); }
LogEntry Logger::trace(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_TRACE, style); }



LogEntry::LogEntry(std::shared_ptr<LogSinkInterface> rSink, std::string_view prefix, TprLogLevel level, TprLogStyle style)
    : mpSink(rSink), mLevel(level), mStyle(style) {
    if (mpSink->maxVerbosity() < mLevel) mDummy = true;
    if (!mDummy) mBuffer << prefix;
    if (mDummy) std::printf("%d\n", mpSink->maxVerbosity());
}

LogEntry::~LogEntry() {
    flush();
}

void LogEntry::flush() {
    if (!mDummy) {
        mpSink->writeLog(mBuffer.str(), mLevel, mStyle);
        mBuffer.str("");
        mBuffer.clear();
    }
}
