

#ifndef UTILS_LOGGER_LOGGER_HPP_
#define UTILS_LOGGER_LOGGER_HPP_


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
        LogEntry(Logger& rLogger, TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        friend class Logger;
};


class Logger {
    private:
        std::mutex mMutex;
        std::atomic<int> verboseLevel{0};
        void write(LogEntry& logEntry);
        friend struct LogEntry;

    public:
        void setVerbosityLevel(int level);

        template <typename T>
        LogEntry operator<<(T msg) {
            LogEntry logEntry = LogEntry(*this);
            logEntry << msg;
            return logEntry;
        }

        LogEntry operator()(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry log(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry error(TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry warn( TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry info(TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry debug(TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry trace(TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
        LogEntry always(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_2IDENT);
};


#endif  // UTILS_LOGGER_LOGGER_HPP_
