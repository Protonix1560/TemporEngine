#pragma once

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



enum class LogLevel {
    Quiet = 0, Error = 1, Warning = 2, Info = 3, Debug = 4, Trace = 5
};



enum class LogStyle {
    Normal, Timestamp1, Error1
};



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
        LogLevel mLogLevel;
        LogStyle mLogStyle;
        std::ostringstream mBuffer;
        std::mutex mMutex;
        Logger& mrLogger;
        bool mAlways = false;

        LogEntry(Logger& rLogger, LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal)
            : mrLogger(rLogger), mLogLevel(logLevel), mLogStyle(logStyle) {}

        friend class Logger;

};



class Logger {

    private:
        std::mutex mMutex;
        std::atomic<int> verboseLevel{0};

        void write(LogEntry& logEntry) {
            if (logEntry.mAlways || static_cast<int>(logEntry.mLogLevel) <= verboseLevel.load()) {
                std::lock_guard<std::mutex> lock(mMutex);
                auto& stream = (logEntry.mLogLevel == LogLevel::Error) ? std::cerr : std::cout;

                switch (logEntry.mLogStyle) {
                    case LogStyle::Normal:
                        stream << "  ";
                        break;
                    
                    case LogStyle::Timestamp1:
                        stream << "==> [" << currTime() << "]: ";
                        break;

                    case LogStyle::Error1:
                        stream << "\033[91m  ";
                        break;

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

        LogEntry operator()(LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, logLevel, logStyle);
        }

        LogEntry log(LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, logLevel, logStyle);
        }

        LogEntry error(LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, LogLevel::Error, logStyle);
        }

        LogEntry warn( LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, LogLevel::Warning, logStyle);
        }

        LogEntry info(LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, LogLevel::Info, logStyle);
        }

        LogEntry debug(LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, LogLevel::Debug, logStyle);
        }

        LogEntry trace(LogStyle logStyle = LogStyle::Normal) {
            return LogEntry(*this, LogLevel::Trace, logStyle);
        }

        LogEntry always(LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal) {
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
