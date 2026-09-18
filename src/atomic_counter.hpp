
#ifndef ATOMIC_COUNTER_HPP_
#define ATOMIC_COUNTER_HPP_

#include <atomic>
#include <limits>


template <typename T>
class atomic_counter {
    public:
        atomic_counter() : value() {}
        atomic_counter(T v) : value(v) {}

        T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
            return value.load(order);
        }
        T exchange_increment(std::memory_order order = std::memory_order_seq_cst) noexcept {
            T v = value.load(order);
            while (true) {
                if (v == std::numeric_limits<T>::max()) return v;
                bool ok = value.compare_exchange_weak(v, v + 1, order);
                if (ok) return v;
            }
        }
        void wait(T v, std::memory_order order = std::memory_order_seq_cst) const noexcept {
            value.wait(v, order);
        }
        void notify_all() noexcept {
            value.notify_all();
        }
        void notify_one() noexcept {
            value.notify_one();
        }
    private:
        std::atomic<T> value;
};

template <>
class atomic_counter<bool> {
    public:
        atomic_counter() : value() {}
        atomic_counter(bool value) : value(value) {}

        bool load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
            return value.load(order);
        }
        void store_true(std::memory_order order = std::memory_order_seq_cst) noexcept {
            value.store(true, order);
        }
        bool exchange_true(std::memory_order order = std::memory_order_seq_cst) noexcept {
            return value.exchange(true, order);
        }
        void wait(bool v, std::memory_order order = std::memory_order_seq_cst) const noexcept {
            value.wait(v, order);
        }
        void notify_all() noexcept {
            value.notify_all();
        }
        void notify_one() noexcept {
            value.notify_one();
        }
    private:
        std::atomic<bool> value;
};


#endif  // ATOMIC_COUNTER_HPP_
