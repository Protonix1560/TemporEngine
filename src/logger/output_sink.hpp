
#ifndef LOGGER_LOG_SINK_HPP_
#define LOGGER_LOG_SINK_HPP_

#include "logger.hpp"
#include "plugin_core.h"

#include <mutex>
#include <vector>
#include <string_view>
#include <span>
#include <cstdio>


class OutputSink {
    private:
        struct LogDest {
            TprLogLevel level;
            FILE* file;
            bool allowAnsiColour;
        };

        struct HistoryEntry {
            format_sequence message;
            TprLogLevel level;
            TprLogStyle style;
        };

        std::mutex mMutex;
        std::vector<LogDest> mLogDests;
        TprLogLevel mMaxVerbosity{};
        std::vector<HistoryEntry> mHistory;
        bool mSaveHistory = true;
    
    public:
        OutputSink(TprLogLevel termLevel, bool allowTermColour);
        ~OutputSink() noexcept;

        void addLogFile(TprLogLevel level, std::string_view path);
        void stopHistory();

        TprResult writeData(std::span<const std::byte> data) noexcept;
        void writeLog(const format_sequence& message, TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept;

        TprLogLevel maxLevel() const;
};


struct OutputSinkFlusher {
    private:
        OutputSink& mrSink;
        TprLogLevel mLevel;
        TprLogStyle mStyle;
    public:
        OutputSinkFlusher(OutputSink& rSink, TprLogLevel level, TprLogStyle style) : mrSink(rSink), mLevel(level), mStyle(style) {}
        void operator()(const format_sequence& seq) {
            mrSink.writeLog(seq, mLevel, mStyle);
        }
};


#endif  // LOGGER_LOG_SINK_HPP_
