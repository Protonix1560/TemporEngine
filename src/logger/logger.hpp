
#ifndef LOGGER_LOGGER_HPP_
#define LOGGER_LOGGER_HPP_


#include "plugin_core.h"

#include <concepts>
#include <mutex>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>


enum class format_marker_id {
    reset,
    foreground0_dark,
    foreground1_dark,
    foreground2_dark,
    foreground3_dark,
    foreground4_dark,
    foreground5_dark,
    foreground6_dark,
    foreground7_dark,
    foreground0_bright,
    foreground1_bright,
    foreground2_bright,
    foreground3_bright,
    foreground4_bright,
    foreground5_bright,
    foreground6_bright,
    foreground7_bright,
    no_foreground,
    background0_dark,
    background1_dark,
    background2_dark,
    background3_dark,
    background4_dark,
    background5_dark,
    background6_dark,
    background7_dark,
    background0_bright,
    background1_bright,
    background2_bright,
    background3_bright,
    background4_bright,
    background5_bright,
    background6_bright,
    background7_bright,
    no_background,
    bold,
    dim,
    normal,
    italic,
    no_italic,
    underline,
    double_underline,
    no_underline,
    blink,
    no_blink,
    inverse,
    no_inverse,
    strikethrough,
    no_strikethrough
};

struct format_marker {};

#define GEN_FORMAT_MARKER(name) \
    struct marker_##name##_t : format_marker{ constexpr static format_marker_id id = format_marker_id::name; }; \
    inline constexpr marker_##name##_t marker_##name;

GEN_FORMAT_MARKER(reset);

GEN_FORMAT_MARKER(foreground0_dark);
GEN_FORMAT_MARKER(foreground1_dark);
GEN_FORMAT_MARKER(foreground2_dark);
GEN_FORMAT_MARKER(foreground3_dark);
GEN_FORMAT_MARKER(foreground4_dark);
GEN_FORMAT_MARKER(foreground5_dark);
GEN_FORMAT_MARKER(foreground6_dark);
GEN_FORMAT_MARKER(foreground7_dark);
GEN_FORMAT_MARKER(foreground0_bright);
GEN_FORMAT_MARKER(foreground1_bright);
GEN_FORMAT_MARKER(foreground2_bright);
GEN_FORMAT_MARKER(foreground3_bright);
GEN_FORMAT_MARKER(foreground4_bright);
GEN_FORMAT_MARKER(foreground5_bright);
GEN_FORMAT_MARKER(foreground6_bright);
GEN_FORMAT_MARKER(foreground7_bright);
GEN_FORMAT_MARKER(no_foreground);

GEN_FORMAT_MARKER(background0_dark);
GEN_FORMAT_MARKER(background1_dark);
GEN_FORMAT_MARKER(background2_dark);
GEN_FORMAT_MARKER(background3_dark);
GEN_FORMAT_MARKER(background4_dark);
GEN_FORMAT_MARKER(background5_dark);
GEN_FORMAT_MARKER(background6_dark);
GEN_FORMAT_MARKER(background7_dark);
GEN_FORMAT_MARKER(background0_bright);
GEN_FORMAT_MARKER(background1_bright);
GEN_FORMAT_MARKER(background2_bright);
GEN_FORMAT_MARKER(background3_bright);
GEN_FORMAT_MARKER(background4_bright);
GEN_FORMAT_MARKER(background5_bright);
GEN_FORMAT_MARKER(background6_bright);
GEN_FORMAT_MARKER(background7_bright);
GEN_FORMAT_MARKER(no_background);

GEN_FORMAT_MARKER(bold);
GEN_FORMAT_MARKER(dim);
GEN_FORMAT_MARKER(normal);

GEN_FORMAT_MARKER(italic);
GEN_FORMAT_MARKER(no_italic);

GEN_FORMAT_MARKER(underline);
GEN_FORMAT_MARKER(double_underline);
GEN_FORMAT_MARKER(no_underline);

GEN_FORMAT_MARKER(blink);
GEN_FORMAT_MARKER(no_blink);

GEN_FORMAT_MARKER(inverse);
GEN_FORMAT_MARKER(no_inverse);

GEN_FORMAT_MARKER(strikethrough);
GEN_FORMAT_MARKER(no_strikethrough);

template <typename T>
concept is_format_marker = std::is_base_of_v<format_marker, T>;

constexpr std::string_view format_ansi(format_marker_id id) {
    switch (id) {
        case format_marker_id::reset: return "\e[0m";
        case format_marker_id::foreground0_dark: return "\e[30m";
        case format_marker_id::foreground1_dark: return "\e[31m";
        case format_marker_id::foreground2_dark: return "\e[32m";
        case format_marker_id::foreground3_dark: return "\e[33m";
        case format_marker_id::foreground4_dark: return "\e[34m";
        case format_marker_id::foreground5_dark: return "\e[35m";
        case format_marker_id::foreground6_dark: return "\e[36m";
        case format_marker_id::foreground7_dark: return "\e[37m";
        case format_marker_id::foreground0_bright: return "\e[90m";
        case format_marker_id::foreground1_bright: return "\e[91m";
        case format_marker_id::foreground2_bright: return "\e[92m";
        case format_marker_id::foreground3_bright: return "\e[93m";
        case format_marker_id::foreground4_bright: return "\e[94m";
        case format_marker_id::foreground5_bright: return "\e[95m";
        case format_marker_id::foreground6_bright: return "\e[96m";
        case format_marker_id::foreground7_bright: return "\e[97m";
        case format_marker_id::no_foreground: return "\e[39m";
        case format_marker_id::background0_dark: return "\e[40m";
        case format_marker_id::background1_dark: return "\e[41m";
        case format_marker_id::background2_dark: return "\e[42m";
        case format_marker_id::background3_dark: return "\e[43m";
        case format_marker_id::background4_dark: return "\e[44m";
        case format_marker_id::background5_dark: return "\e[45m";
        case format_marker_id::background6_dark: return "\e[46m";
        case format_marker_id::background7_dark: return "\e[47m";
        case format_marker_id::background0_bright: return "\e[100m";
        case format_marker_id::background1_bright: return "\e[101m";
        case format_marker_id::background2_bright: return "\e[102m";
        case format_marker_id::background3_bright: return "\e[103m";
        case format_marker_id::background4_bright: return "\e[104m";
        case format_marker_id::background5_bright: return "\e[105m";
        case format_marker_id::background6_bright: return "\e[106m";
        case format_marker_id::background7_bright: return "\e[107m";
        case format_marker_id::no_background: return "\e[49m";
        case format_marker_id::bold: return "\e[1m";
        case format_marker_id::dim: return "\e[2m";
        case format_marker_id::normal: return "\e[22m";
        case format_marker_id::italic: return "\e[3m";
        case format_marker_id::no_italic: return "\e[23m";
        case format_marker_id::underline: return "\e[4m";
        case format_marker_id::double_underline: return "\e[21m";
        case format_marker_id::no_underline: return "\e[24m";
        case format_marker_id::blink: return "\e[5m";
        case format_marker_id::no_blink: return "\e[25m";
        case format_marker_id::inverse: return "\e[7m";
        case format_marker_id::no_inverse: return "\e[27m";
        case format_marker_id::strikethrough: return "\e[9m";
        case format_marker_id::no_strikethrough: return "\e[29m";
        default: return "";
    }
}

class format_sequence {
    private:
        std::vector<std::variant<std::string, format_marker_id>> m_data;

    public:
        format_sequence() = default;
        format_sequence(std::string_view str) {
            m_data.emplace_back(std::string(str));
        }

        template <is_format_marker M>
        format_sequence& operator<<(const M& marker) {
            m_data.emplace_back(marker.id);
            return *this;
        }

        template <typename T>
        requires requires(std::ostream& stream, const T& str) {
            { stream << str } -> std::same_as<std::ostream&>;
        }
        format_sequence& operator<<(const T& str) {
            m_data.emplace_back((std::ostringstream{} << str).str());
            return *this;
        }

        size_t count() const {
            return m_data.size();
        }

        size_t length() const {
            size_t l = 0;
            for (const auto& el : m_data) {
                if (std::holds_alternative<std::string>(el)){
                    l += std::get<std::string>(el).size();
                }
            }
            return l;
        }

        bool empty() const { return length() == 0; }

        void clear() { m_data.clear(); }
        void reserve(size_t n) { m_data.reserve(n); }
        void shrink_to_fit() { m_data.shrink_to_fit(); }

        auto begin() const { return m_data.begin(); }
        auto end() const { return m_data.end(); }
};


// from "log_sink.hpp"
class OutputSink;


class LogEntry;

class Logger {
    private:
        std::mutex mMutex;
        OutputSink* mpSink = nullptr;
        format_sequence mPrefix;

    public:
        Logger() = default;
        Logger(OutputSink* pSink, const format_sequence& prefix);
        Logger(OutputSink* pSink);

        Logger(const Logger& other) : mpSink(other.mpSink), mPrefix(other.mPrefix) {}
        Logger& operator=(const Logger& other) {
            mpSink = other.mpSink;
            mPrefix = other.mPrefix;
            return *this;
        }

        Logger(Logger&& other) : mpSink(std::move(other.mpSink)), mPrefix(std::move(other.mPrefix)) {}
        Logger& operator=(Logger&& other) {
            mpSink = std::move(other.mpSink);
            mPrefix = std::move(other.mPrefix);
            return *this;
        }

        OutputSink* sink() const noexcept;
        LogEntry prefix();
        Logger& setPrefix(const format_sequence& prefix);

        LogEntry operator()(TprLogLevel level = TPR_LOG_LEVEL_INFO, TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry panic(TprLogStyle style = TPR_LOG_STYLE_PANIC1) const;
        LogEntry error(TprLogStyle style = TPR_LOG_STYLE_ERROR1) const;
        LogEntry warn(TprLogStyle style = TPR_LOG_STYLE_WARN1) const;
        LogEntry info(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry debug(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
        LogEntry trace(TprLogStyle style = TPR_LOG_STYLE_6IDENT) const;
};


struct LoggerPrefixFlusher {
    private:
        Logger& mrLogger;
    public:
        LoggerPrefixFlusher(Logger& rLogger) : mrLogger(rLogger) {}
        void operator()(const format_sequence& seq) {
            mrLogger.setPrefix(seq);
        }
};


#endif  // LOGGER_LOGGER_HPP_
