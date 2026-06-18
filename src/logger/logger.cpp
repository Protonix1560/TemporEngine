
#include "logger.hpp"
#include "plugin_core.h"
#include "time.hpp"

#include <cstdio>
#include <string_view>



LogSink::LogSink(size_t verbosity) {
    if (verbosity > maxVerbosity) verbosity = maxVerbosity;
    mVerbosity = verbosity;
}

void LogSink::writeLog(std::string_view msg, TprLogLevel level, TprLogStyle style) noexcept {
    if (msg.empty()) return;
    if (level > mVerbosity.load()) return;

    std::lock_guard<std::mutex> lock(mMutex);

    std::string nsep;
    std::string begin;

    switch (style) {
        case TPR_LOG_STYLE_2IDENT:
            begin = "  "; nsep = "  "; break;
        case TPR_LOG_STYLE_6IDENT:
            begin = "      "; nsep = "      "; break;
        case TPR_LOG_STYLE_TIMESTAMP1:
            begin = "  > [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ERROR1:
            begin = "      \033[91m"; nsep = "      "; break;
        case TPR_LOG_STYLE_WARN1:
            begin =  "      \033[93m"; nsep = "      "; break;
        case TPR_LOG_STYLE_SUCCESS1:
            begin = "      \033[102m"; nsep = "      "; break;
        case TPR_LOG_STYLE_STARTSTAMP1:
            begin = "\033[35mr->\033[0m [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_ENDSTAMP1:
            begin = "\033[32ml->\033[0m [" + current_time() + "]: "; nsep = "    "; break;
        case TPR_LOG_STYLE_PANIC1:
            begin = "\033[95m"; break;
        case TPR_LOG_STYLE_STANDART: break;
        default: return;  // invalid enum value
    }

    if (style != TPR_LOG_STYLE_STANDART) {
        std::string_view view = msg;
        std::fprintf(stderr, "%s", begin.data());
        size_t nlinepos = 0;
        while (nlinepos != std::string_view::npos) {
            nlinepos = view.find('\n');
            if (nlinepos == std::string_view::npos) {
                std::fprintf(stderr, "%s", view.data());
                std::fprintf(stderr, "\n");
            } else {
                std::fprintf(stderr, "%s", view.substr(0, nlinepos + 1).data());
                view = view.substr(nlinepos + 1);
                if (!view.empty()) {
                    std::fprintf(stderr, "%s", nsep.data());
                }
                if (view.empty()) break;
            }
        }
    } else {
        std::fprintf(stderr, "%s", msg.data());
    }
    std::fprintf(stderr, "\033[0m");
    std::fflush(stderr);
}

TprResult LogSink::writeData(std::span<const std::byte> data) noexcept {
    if (data.empty()) return TPR_SUCCESS;
    std::lock_guard<std::mutex> lock(mMutex);
    std::fprintf(stdout, "%p", data.data());
    std::fflush(stdout);
    return TPR_SUCCESS;
}

Logger LogSink::createLogger(std::string_view name) {
    return Logger(*this, name);
}

size_t LogSink::verbosity() const {
    return mVerbosity.load();
}

void LogSink::setVerbosity(size_t verbosity) {
    if (verbosity > maxVerbosity) verbosity = maxVerbosity;
    mVerbosity.store(verbosity);
}



Logger::Logger(LogSink& rSink, std::string_view prefix) : mrSink(rSink), mPrefix(prefix) {}

LogEntry Logger::operator()(TprLogLevel level, TprLogStyle style) const {
    return LogEntry(mrSink, mPrefix, level, style);
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



LogEntry::LogEntry(LogSink& rSink, std::string_view prefix, TprLogLevel level, TprLogStyle style) : mrSink(rSink), mLevel(level), mStyle(style) {
    if (mrSink.verbosity() < level) mDummy = true;
    if (!mDummy) mBuffer << prefix;
}

LogEntry::~LogEntry() {
    flush();
}

void LogEntry::flush() {
    if (!mDummy) {
        mrSink.writeLog(mBuffer.str(), mLevel, mStyle);
        mBuffer.str("");
        mBuffer.clear();
    }
}
