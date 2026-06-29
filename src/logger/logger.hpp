

#ifndef LOGGER_LOGGER_HPP_
#define LOGGER_LOGGER_HPP_


#include "core.hpp"
#include "plugin_core.h"

#include <sstream>
#include <atomic>
#include <memory>


class LogSinkInterface {
    public:
        virtual void writeLog(std::string_view msg, TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept = 0;
        virtual TprLogLevel maxVerbosity() const = 0;
};
REGISTER_TYPE_NAME_S(LogSinkInterface, "LgSI");


class Logger;


class LogEntry {
    private:
        std::shared_ptr<LogSinkInterface> mpSink;
        std::ostringstream mBuffer;
        bool mDummy = false;
        TprLogLevel mLevel;
        TprLogStyle mStyle;

        LogEntry(std::shared_ptr<LogSinkInterface> rSink, std::string_view prefix, TprLogLevel level, TprLogStyle style);

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
        const std::atomic<std::shared_ptr<LogSinkInterface>>& mrSink;
        std::string mPrefix;

    public:
        Logger(const std::atomic<std::shared_ptr<LogSinkInterface>>& rSink, std::string_view prefix);

        std::shared_ptr<LogSinkInterface> sink() const noexcept;
        Logger derive(const std::string& prefix) const;

        LogEntry operator()(TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry panic(TprLogStyle style = TPR_LOG_STYLE_PANIC1) const;
        LogEntry error(TprLogStyle style = TPR_LOG_STYLE_ERROR1) const;
        LogEntry warn(TprLogStyle style = TPR_LOG_STYLE_WARN1) const;
        LogEntry info(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry debug(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry trace(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
};


#endif  // LOGGER_LOGGER_HPP_
