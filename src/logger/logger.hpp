

#ifndef UTILS_LOGGER_LOGGER_HPP_
#define UTILS_LOGGER_LOGGER_HPP_


#include "core.hpp"
#include "plugin_core.h"

#include <mutex>
#include <atomic>
#include <sstream>


constexpr size_t maxVerbosity = 6;


class LogSink;
class Logger;


class LogEntry {
    private:
        LogSink& mrSink;
        std::ostringstream mBuffer;
        bool mDummy = false;
        TprLogLevel mLevel;
        TprLogStyle mStyle;

        LogEntry(LogSink& rSink, std::string_view prefix, TprLogLevel level, TprLogStyle style);

        friend class Logger;

    public:
        ~LogEntry();

        void flush();

        template <typename T>
        LogEntry& operator<<(const T& msg) {
            if (!mDummy) mBuffer << msg;
            return *this;
        }
};



class Logger {
    private:
        LogSink& mrSink;
        std::string mPrefix;

        Logger(LogSink& rSink, std::string_view prefix);

        friend class LogSink;

    public:
        Logger& sink() const noexcept;
        Logger derive(const std::string& prefix) const;

        LogEntry operator()(TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry panic(TprLogStyle style = TPR_LOG_STYLE_PANIC1) const;
        LogEntry error(TprLogStyle style = TPR_LOG_STYLE_ERROR1) const;
        LogEntry warn(TprLogStyle style = TPR_LOG_STYLE_WARN1) const;
        LogEntry info(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry debug(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry trace(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
};


class LogSink {
    private:
        std::mutex mMutex;
        std::atomic<size_t> mVerbosity;
    
    public:
        LogSink(size_t verbosity);
        Logger createLogger(std::string_view prefix);
        void setVerbosity(size_t verbosity);
        size_t verbosity() const;
        void write(std::string_view msg, TprLogDestination dest, TprLogLevel level, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept;
};

REGISTER_TYPE_NAME_S(LogSink, "LgSk");


#endif  // UTILS_LOGGER_LOGGER_HPP_
