

#ifndef UTILS_LOGGER_LOGGER_HPP_
#define UTILS_LOGGER_LOGGER_HPP_


#include "core.hpp"
#include "plugin_core.h"

#include <mutex>
#include <atomic>
#include <sstream>


class Logger;


struct LogEntry {
    public:
        template <typename T> LogEntry& operator<<(T msg) {
            std::lock_guard<std::mutex> lockM(mMutex);
            mBuffer << msg;
            return *this;
        }

        LogEntry(LogEntry&& other);
        LogEntry(const LogEntry& other) = delete;
        void flush();
        ~LogEntry();

    private:
        TprLogLevel mLogLevel;
        TprLogStyle mLogStyle;
        std::ostringstream mBuffer;
        std::mutex mMutex;
        Logger& mrLogger;
        bool mAlways = false;
        LogEntry(Logger& rLogger, TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        friend class Logger;
};


class Logger {
    private:
        std::mutex mMutex;
        std::atomic<int> mStartstampCount{0};
        std::atomic<size_t> verboseLevel;
        void write(LogEntry& logEntry);
        friend struct LogEntry;

    public:

        Logger(size_t verbosity)
            : verboseLevel(verbosity) {}

        void setVerbosityLevel(size_t level);

        template <typename T>
        LogEntry operator<<(T msg) {
            LogEntry logEntry = LogEntry(*this);
            logEntry << msg;
            return logEntry;
        }

        LogEntry operator()(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry log(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry error(TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry warn( TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry info(TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry debug(TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry trace(TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
        LogEntry always(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_6IDENT);
};

REGISTER_TYPE_NAME(Logger);


#endif  // UTILS_LOGGER_LOGGER_HPP_
