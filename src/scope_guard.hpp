
#ifndef SCOPE_GUARD_HPP_
#define SCOPE_GUARD_HPP_

#include <utility>
#include <optional>
#include <functional>

class scope_guard {
    private:
        std::optional<std::function<void()>> m_f;
    public:
        scope_guard() noexcept = default;

        template <typename F>
        scope_guard(F&& f) : m_f(std::move(f)) {}

        ~scope_guard() {
            if (m_f.has_value()) m_f.value()();
        }

        scope_guard(const scope_guard& other) = delete;
        scope_guard& operator=(const scope_guard& other) = delete;

        scope_guard(scope_guard&& other) : m_f(std::move(other.m_f)) {
            other.release();
        }
        scope_guard& operator=(scope_guard&& other) {
            if (this != &other) {
                if (m_f.has_value()) m_f.value()();
                m_f = std::move(other.m_f);
                other.release();
            }
            return *this;
        }

        void release() noexcept {
            m_f.reset();
        }
};

class unlock_guard {
    private:
        scope_guard m_guard;
    public:
        unlock_guard() = default;

        template <typename T>
        unlock_guard(T& lock) {
            lock.unlock();
            m_guard = [&]() {
                lock.lock();
            };
        }
        
        unlock_guard(const unlock_guard& other) = delete;

        unlock_guard(unlock_guard&& other) : m_guard(std::move(other.m_guard)) {}
};

#endif  // SCOPE_GUARD_HPP_
