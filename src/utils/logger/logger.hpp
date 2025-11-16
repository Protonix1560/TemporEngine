

#ifndef UTILS_LOGGER_LOGGER_HPP_
#define UTILS_LOGGER_LOGGER_HPP_


#include "plugin.h"

#include <iostream>
#include <mutex>
#include <atomic>
#include <iomanip>



inline std::string currTime() {
    using namespace std::chrono;
    
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
    
    std::tm buf;
    #if defined(_WIN32)
        localtime_s(&buf, &in_time_t);
    #else
        localtime_r(&in_time_t, &buf);
    #endif

    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}



class Logger;



struct LogEntry {

    public:
        template <typename T>
        LogEntry& operator<<(T msg) {
            std::lock_guard<std::mutex> lockM(mMutex);
            mBuffer << msg;
            return *this;
        }

        LogEntry(LogEntry&& other)
            : mrLogger(other.mrLogger), mLogLevel(other.mLogLevel), mLogStyle(other.mLogStyle) {
            
            std::lock_guard<std::mutex> lock(other.mMutex);
            mBuffer << other.mBuffer.str();
            other.mBuffer.str("");
            other.mBuffer.clear();
        }

        LogEntry(const LogEntry& other) = delete;

        void flush();

        ~LogEntry() {
            flush();
        }

    private:
        TprLogLevel mLogLevel;
        TprLogStyle mLogStyle;
        std::ostringstream mBuffer;
        std::mutex mMutex;
        Logger& mrLogger;
        bool mAlways = false;

        LogEntry(Logger& rLogger, TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_IDENT)
            : mrLogger(rLogger), mLogLevel(logLevel), mLogStyle(logStyle) {}

        friend class Logger;

};



class Logger {

    private:
        std::mutex mMutex;
        std::atomic<int> verboseLevel{0};

        void write(LogEntry& logEntry) {

            if (logEntry.mBuffer.str().empty()) return;

            if (logEntry.mAlways || static_cast<int>(logEntry.mLogLevel) <= verboseLevel.load()) {
                std::lock_guard<std::mutex> lock(mMutex);
                
                auto& stream = (logEntry.mLogLevel == TPR_LOG_LEVEL_ERROR) ? std::cerr : std::cout;

                switch (logEntry.mLogStyle) {
                    case TPR_LOG_STYLE_IDENT:
                        stream << "  ";
                        break;
                    
                    case TPR_LOG_STYLE_TIMESTAMP1:
                        stream << "==> [" << currTime() << "]: ";
                        break;

                    case TPR_LOG_STYLE_ERROR1:
                        stream << "\033[91m  ";
                        break;

                    case TPR_LOG_STYLE_WARN1:
                        stream << "\033[93m  ";
                        break;

                    default: break;
                }

                stream << logEntry.mBuffer.str() << "\033[0m";
                stream.flush();
            }
        }

        friend struct LogEntry;

    public:

        void setVerbosityLevel(int level) {
            verboseLevel.store(level);
        }

        template <typename T>
        LogEntry operator<<(T msg) {
            LogEntry logEntry = LogEntry(*this);
            logEntry << msg;
            return logEntry;
        }

        LogEntry operator()(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, logLevel, logStyle);
        }

        LogEntry log(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, logLevel, logStyle);
        }

        LogEntry error(TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, TPR_LOG_LEVEL_ERROR, logStyle);
        }

        LogEntry warn( TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, TPR_LOG_LEVEL_WARN, logStyle);
        }

        LogEntry info(TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, TPR_LOG_LEVEL_INFO, logStyle);
        }

        LogEntry debug(TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, TPR_LOG_LEVEL_DEBUG, logStyle);
        }

        LogEntry trace(TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            return LogEntry(*this, TPR_LOG_LEVEL_TRACE, logStyle);
        }

        LogEntry always(TprLogLevel logLevel = TPR_LOG_LEVEL_INFO, TprLogStyle logStyle = TPR_LOG_STYLE_IDENT) {
            LogEntry logEntry = LogEntry(*this, logLevel, logStyle);
            logEntry.mAlways = true;
            return logEntry;
        }

};



inline void LogEntry::flush()  {
    std::lock_guard<std::mutex> lock(mMutex);
    mrLogger.write(*this);
    mBuffer.str("");
    mBuffer.clear();
}




#endif  // UTILS_LOGGER_LOGGER_HPP_
