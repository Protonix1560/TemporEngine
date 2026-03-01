

#ifndef LOGGER_LOGGER_HPP_
#define LOGGER_LOGGER_HPP_


#include "logger.hpp"
#include "plugin_core.h"

#include <iostream>
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

    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 10000;

    std::ostringstream oss;
    oss << std::put_time(&buf, "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(4) << ms.count();

    return oss.str();
}



void Logger::write(LogEntry& logEntry) {

    if (logEntry.mBuffer.str().empty()) return;

    if (logEntry.mAlways || static_cast<int>(logEntry.mLogLevel) <= verboseLevel.load()) {
        std::lock_guard<std::mutex> lock(mMutex);
        
        auto& stream = (logEntry.mLogLevel == TPR_LOG_LEVEL_ERROR) ? std::cerr : std::cout;
        std::string nsep;

        std::string div;
        if (mStartstampCount == 0) {
            div = " ";
        } else if (mStartstampCount == 1) {
            div = "\033[2;90m`\033[0m";
        } else if (mStartstampCount == 2) {
            div = "\033[2;90m'\033[0m";
        } else if (mStartstampCount == 3) {
            div = "\033[2;90m:\033[0m";
        } else if (mStartstampCount == 4) {
            div = "\033[90m:\033[0m";
        } else if (mStartstampCount == 5) {
            div = "\033[2m;\033[0m";
        } else if (mStartstampCount == 6) {
            div = "!";
        } else if (mStartstampCount >= 7) {
            div = "|";
        }

        switch (logEntry.mLogStyle) {
            case TPR_LOG_STYLE_2IDENT:
                stream << div + " "; nsep = div + " "; break;
            case TPR_LOG_STYLE_6IDENT:
                stream << div + "     "; nsep = div + "     "; break;
            case TPR_LOG_STYLE_TIMESTAMP1:
                stream << div + " > [" << currTime() << "]: "; nsep = div + "   "; break;
            case TPR_LOG_STYLE_ERROR1:
                stream << div + "     \033[91m"; nsep = div + "     "; break;
            case TPR_LOG_STYLE_WARN1:
                stream << div + "     \033[93m"; nsep = div + "     "; break;
            case TPR_LOG_STYLE_SUCCESS1:
                stream << div + "     \033[102m"; nsep = div + "     "; break;
            case TPR_LOG_STYLE_ENDSTAMP1:
                stream << "\033[32ml->\033[0m [" << currTime() << "]: "; nsep = "    "; mStartstampCount--; break;
            case TPR_LOG_STYLE_STARTSTAMP1:
                stream << "\033[35mr->\033[0m [" << currTime() << "]: "; nsep = "    "; mStartstampCount++; break;
            case TPR_LOG_STYLE_STANDART: break;
            default: ;
        }

        if (logEntry.mLogStyle != TPR_LOG_STYLE_STANDART) {
            std::string buffer = logEntry.mBuffer.str();
            size_t nlinepos = 0;
            while (nlinepos != std::string::npos) {
                nlinepos = buffer.find('\n');
                stream << buffer.substr(0, (nlinepos != std::string::npos) ? nlinepos + 1 : nlinepos);
                if (nlinepos < buffer.size() - 1) stream << nsep;
                buffer = buffer.substr(nlinepos + 1);
            }
        } else {
            stream << logEntry.mBuffer.str();
        }
        stream << "\033[0m";
        stream.flush();
    }
}

void Logger::setVerbosityLevel(size_t level) {
    verboseLevel.store(level);
}

LogEntry Logger::operator()(TprLogLevel logLevel, TprLogStyle logStyle) {
    return LogEntry(*this, logLevel, logStyle);
}

LogEntry Logger::log(TprLogLevel logLevel, TprLogStyle logStyle) {
    return LogEntry(*this, logLevel, logStyle);
}

LogEntry Logger::error(TprLogStyle logStyle) {
    return LogEntry(*this, TPR_LOG_LEVEL_ERROR, logStyle);
}

LogEntry Logger::warn( TprLogStyle logStyle) {
    return LogEntry(*this, TPR_LOG_LEVEL_WARN, logStyle);
}

LogEntry Logger::info(TprLogStyle logStyle) {
    return LogEntry(*this, TPR_LOG_LEVEL_INFO, logStyle);
}

LogEntry Logger::debug(TprLogStyle logStyle) {
    return LogEntry(*this, TPR_LOG_LEVEL_DEBUG, logStyle);
}

LogEntry Logger::trace(TprLogStyle logStyle) {
    return LogEntry(*this, TPR_LOG_LEVEL_TRACE, logStyle);
}

LogEntry Logger::always(TprLogLevel logLevel, TprLogStyle logStyle) {
    LogEntry logEntry = LogEntry(*this, logLevel, logStyle);
    logEntry.mAlways = true;
    return logEntry;
}



LogEntry::LogEntry(LogEntry&& other)
    : mrLogger(other.mrLogger), mLogLevel(other.mLogLevel), mLogStyle(other.mLogStyle) {
    std::lock_guard<std::mutex> lock(other.mMutex);
    mBuffer << other.mBuffer.str();
    other.mBuffer.str("");
    other.mBuffer.clear();
}

LogEntry::LogEntry(Logger& rLogger, TprLogLevel logLevel, TprLogStyle logStyle)
    : mrLogger(rLogger), mLogLevel(logLevel), mLogStyle(logStyle) {
}

void LogEntry::flush()  {
    std::lock_guard<std::mutex> lock(mMutex);
    mrLogger.write(*this);
    mBuffer.str("");
    mBuffer.clear();
}

LogEntry::~LogEntry() {
    flush();
}



#endif  // LOGGER_LOGGER_HPP_
