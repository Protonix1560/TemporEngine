
#ifndef LOGGER_LOG_SINK_HPP_
#define LOGGER_LOG_SINK_HPP_

#include "logger.hpp"
#include "tempor.h"
#include "core.hpp"
#include "time.hpp"

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

        template <typename S>
        expected<std::string, TprResult> formatted(const basic_format_sequence<S>& msg, TprLogStyle style, bool allowAnsi) {
            std::string message;
            std::string middle;
            if (allowAnsi) {
                switch (style) {
                    case TPR_LOG_STYLE_2IDENT:        message += "  ";                                          middle = "  ";       break;
                    case TPR_LOG_STYLE_6IDENT:        message += "      ";                                      middle = "      ";   break;
                    case TPR_LOG_STYLE_TIMESTAMP1:    message += "  > [" + current_time() + "]: ";              middle = "    ";     break;
                    case TPR_LOG_STYLE_ERROR1:        message += "      \e[91m";                                middle = "      ";   break;
                    case TPR_LOG_STYLE_WARN1:         message += "      \e[93m";                                middle = "      ";   break;
                    case TPR_LOG_STYLE_SUCCESS1:      message += "      \e[102m";                               middle = "      ";   break;
                    case TPR_LOG_STYLE_STARTSTAMP1:   message += "\e[35mr->\e[0m [" + current_time() + "]: ";   middle = "    ";     break;
                    case TPR_LOG_STYLE_ENDSTAMP1:     message += "\e[32ml->\e[0m [" + current_time() + "]: ";   middle = "    ";     break;
                    case TPR_LOG_STYLE_PANIC1:        message += "\e[95m";                                      middle = "";         break;
                    case TPR_LOG_STYLE_NORMAL:      message += "";                                            middle = "";         break;
                    default: return unexpected(TPR_ERROR_INVALID_VALUE);
                }
            } else {
                switch (style) {
                    case TPR_LOG_STYLE_2IDENT:        message += "  ";                               middle = "  ";       break;
                    case TPR_LOG_STYLE_6IDENT:        message += "      ";                           middle = "      ";   break;
                    case TPR_LOG_STYLE_TIMESTAMP1:    message += "  > [" + current_time() + "]: ";   middle = "    ";     break;
                    case TPR_LOG_STYLE_ERROR1:        message += "      ";                           middle = "      ";   break;
                    case TPR_LOG_STYLE_WARN1:         message += "      ";                           middle = "      ";   break;
                    case TPR_LOG_STYLE_SUCCESS1:      message += "      ";                           middle = "      ";   break;
                    case TPR_LOG_STYLE_STARTSTAMP1:   message += "r-> [" + current_time() + "]: ";   middle = "    ";     break;
                    case TPR_LOG_STYLE_ENDSTAMP1:     message += "l-> [" + current_time() + "]: ";   middle = "    ";     break;
                    case TPR_LOG_STYLE_PANIC1:        message += "";                                 middle = "";         break;
                    case TPR_LOG_STYLE_NORMAL:      message += "";                                 middle = "";         break;
                    default: return unexpected(TPR_ERROR_INVALID_VALUE);
                }
            }

            size_t lastnlinepos = 0;
            size_t currpos = 0;
            for (const auto& el : msg) {
                std::visit(overload{
                    [&](const std::string& str) {
                        std::string_view view{str};
                        while (!view.empty()) {
                            if (lastnlinepos == currpos && currpos != 0) {
                                message += middle;
                            }
                            size_t nlinepos = view.find('\n');
                            if (nlinepos == std::string_view::npos) {
                                message += view;
                                currpos += view.size();
                                break;
                            } else {
                                message += view.substr(0, nlinepos + 1);
                                view = view.substr(nlinepos + 1);
                                currpos += nlinepos + 1;
                                lastnlinepos = currpos;
                            }
                        }
                    },
                    [&](format_marker_id id) {
                        if (allowAnsi) {
                            message += format_ansi(id);
                        }
                    }
                }, el);
            }
            if (message.back() != '\n') message += '\n';
            if (allowAnsi) message += "\e[0m";
            return message;
        }
    
    public:
        OutputSink(TprLogLevel termLevel, bool allowTermColour);
        ~OutputSink() noexcept;

        void addLogFile(TprLogLevel level, std::string_view path);
        void stopHistory();

        TprResult writeData(std::span<const std::byte> data) noexcept;

        template <typename S>
        void writeLog(const basic_format_sequence<S>& message, TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) noexcept {
            if (message.empty()) return;
            std::lock_guard<std::mutex> lock(mMutex);
            std::optional<std::string> colourful;
            std::optional<std::string> colourless;
            if (mSaveHistory) {
                mHistory.emplace_back(message, level, style);
            }
            for (const auto& dest : mLogDests) {
                if (level > dest.level) continue;
                if (dest.allowAnsiColour) {
                    if (!colourful.has_value()) {
                        auto exp = formatted(message, style, true);
                        if (!exp.has_value()) return;
                        colourful.emplace(std::move(exp.value()));
                    }
                    std::fprintf(dest.file, "%s", colourful.value().c_str());
                    std::fflush(dest.file);
                } else {
                    if (!colourless.has_value()) {
                        auto exp = formatted(message, style, false);
                        if (!exp.has_value()) return;
                        colourless.emplace(std::move(exp.value()));
                    }
                    std::fprintf(dest.file, "%s", colourless.value().c_str());
                    std::fflush(dest.file);
                }
            }
        }

        TprLogLevel maxLevel() const;
};


struct OutputSinkFlusher {
    private:
        OutputSink& mrSink;
        TprLogLevel mLevel;
        TprLogStyle mStyle;
    public:
        OutputSinkFlusher(OutputSink& rSink, TprLogLevel level, TprLogStyle style) : mrSink(rSink), mLevel(level), mStyle(style) {}
        template <typename S>
        void operator()(const basic_format_sequence<S>& seq) {
            mrSink.writeLog(seq, mLevel, mStyle);
        }
};


#endif  // LOGGER_LOG_SINK_HPP_
