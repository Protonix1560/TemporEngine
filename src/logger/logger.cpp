
#include "logger.hpp"
#include "output_sink.hpp"
#include "plugin_core.h"
#include "log_entry.hpp"

#include <mutex>


Logger::Logger(OutputSink* pSink) : mpSink(pSink) {}

Logger::Logger(OutputSink* pSink, const format_sequence& prefix) : mpSink(pSink), mPrefix(prefix) {}

LogEntry Logger::operator()(TprLogLevel level, TprLogStyle style) const {
    if (mpSink && level <= mpSink->maxLevel()) return LogEntry(OutputSinkFlusher(*mpSink, level, style), mPrefix);
    return LogEntry();
}

OutputSink* Logger::sink() const noexcept {
    return mpSink;
}

LogEntry Logger::prefix() {
    return LogEntry(LoggerPrefixFlusher(*this));
}

Logger& Logger::setPrefix(const format_sequence& prefix) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPrefix = prefix;
    return *this;
}

LogEntry Logger::panic(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_PANIC, style); }
LogEntry Logger::error(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_ERROR, style); }
LogEntry Logger::warn(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_WARN, style); }
LogEntry Logger::info(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_INFO, style); }
LogEntry Logger::debug(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_DEBUG, style); }
LogEntry Logger::trace(TprLogStyle style) const { return (*this)(TPR_LOG_LEVEL_TRACE, style); }

