
#ifndef LOGGER_LOG_ENTRY_HPP_
#define LOGGER_LOG_ENTRY_HPP_

#include "logger.hpp"

#include <optional>
#include <functional>


class LogEntry {
    private:
        std::optional<std::function<void(const format_sequence& seq)>> mFlusher;
        format_sequence mBuffer;

    public:
        LogEntry() = default;

        template <typename F>
        LogEntry(F&& flusher) : mFlusher(flusher) {}

        template <typename F>
        LogEntry(F&& flusher, const format_sequence& prefix) : mFlusher(flusher), mBuffer(prefix) {}

        ~LogEntry() {
            flush();
        }

        void flush() {
            if (mFlusher.has_value()) {
                mFlusher.value()(mBuffer);
                mBuffer.clear();
            }
        }

        template <typename T>
        LogEntry& operator<<(const T& msg) {
            if (mFlusher.has_value()) {
                mBuffer << msg;
            }
            return *this;
        }
};

#endif  // LOGGER_LOG_ENTRY_HPP_
