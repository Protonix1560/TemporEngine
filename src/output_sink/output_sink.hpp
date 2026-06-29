
#ifndef OUTPUT_SINK_OUTPUT_SINK_HPP_
#define OUTPUT_SINK_OUTPUT_SINK_HPP_

#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#include <mutex>
#include <vector>


constexpr TprLogLevel maxLogLevel = TPR_LOG_LEVEL_TRACE;


struct HistoryEntry {
    std::string log;
    TprLogLevel level;
};

class TermSink : public LogSinkInterface {
    private:
        mutable std::mutex mMutex;
        TprLogLevel mTermVerbosity;
        std::vector<HistoryEntry> mLogHistory;
    
    public:
        TermSink(TprLogLevel termVerbosity);

        std::span<const HistoryEntry> logHistory() const;
        TprLogLevel termVerbosity() const;

        TprResult writeData(std::span<const std::byte> data) noexcept;

        TprLogLevel maxVerbosity() const override;
        void writeLog(std::string_view msg, TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept override;
};
REGISTER_TYPE_NAME_S(TermSink, "TmSk");


// from "settings.hpp"
class Settings;

// from "file_registry.hpp"
class FileRegistry;

struct LogDest {
    TprFile file;
    TprLogLevel verbosity;
};

class TermFileSink : public LogSinkInterface {
    private:
        Settings& mrSettings;
        FileRegistry& mrFileReg;
        std::mutex mMutex;
        std::vector<LogDest> mLogDests;
        TprLogLevel mMaxVerbosity;
        TprLogLevel mTermVerbosity;

        void writeToFiles(std::string_view msg, TprLogLevel level);
    
    public:
        TermFileSink(Settings& rSettings, FileRegistry& rResReg, const TermSink& rTermSink, TprLogLevel termVerbosity);

        TprResult writeData(std::span<const std::byte> data) noexcept;

        TprLogLevel maxVerbosity() const override;
        void writeLog(std::string_view msg, TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept override;
};
REGISTER_TYPE_NAME_S(TermFileSink, "TFSk");


#endif  // OUTPUT_SINK_OUTPUT_SINK_HPP_
