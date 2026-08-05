
#ifndef HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_INTERVAL_UNION_HPP_
#define HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_INTERVAL_UNION_HPP_


#include <type_traits>
#include <vector>
#include <algorithm>
#include <span>


template <typename S>
requires std::is_arithmetic_v<S>
class interval_union;


template <typename S>
requires std::is_arithmetic_v<S>
class interval {
    private:
        S m_begin;
        S m_end;
        bool m_first_included;
        bool m_last_included;
        friend class interval_union<S>;
    public:
        interval() = default;
        interval(S begin, S end, bool first_included = true, bool last_included = false)
            : m_begin(begin), m_end(end), m_first_included(first_included), m_last_included(last_included) { assert(end >= begin); }

        S begin() const { return m_begin; }
        S end() const { return m_end; }
        bool first_included() const { return m_first_included; }
        bool last_included() const { return m_last_included; }
        bool empty() const { return begin() == end() && !(first_included() && last_included()); }
        S size() const { return end() - begin(); }

        bool operator<(const interval<S>& other) const {
            if (end() < other.begin()) return true;
            if (end() > other.begin()) return false;
            return !(last_included() && other.first_included());
        }
        bool operator>(const interval<S>& other) const {
            if (begin() > other.end()) return true;
            if (begin() < other.end()) return false;
            return !(first_included() && other.last_included());
        }
        bool operator&&(const interval<S>& other) const {
            if (empty() || other.empty()) return false;
            return !(*this < other) && !(other < *this);
        }

        bool mergeable(const interval<S>& other) const {
            return (
                *this && other ||
                (end() == other.begin() && (last_included() || other.first_included())) ||
                (begin() == other.end() && (first_included() || other.last_included()))
            );
        }

        bool is_subinterval(const interval<S>& other) const {
            return (
                (begin() > other.begin() || begin() == other.begin() && other.first_included() >= first_included()) &&
                (end() < other.end() || end() == other.end() && other.last_included() >= last_included())
            );
        }
};


template <typename S>
requires std::is_arithmetic_v<S>
class interval_union {
    private:
        std::vector<interval<S>> m_storage;

    public:
        interval_union() {}
        interval_union(const interval<S>& interval) : m_storage(1, interval) {}

        std::span<const interval<S>> data() const { return std::span(m_storage.begin(), m_storage.end()); }

        interval_union<S>& operator+=(const interval<S>& inter) {

            auto first = std::lower_bound(m_storage.begin(), m_storage.end(), inter, [](const interval<S>& a, const interval<S>& b) {
                return a < b && !a.mergeable(b);
            });

            if (first == m_storage.end() || !first->mergeable(inter)) {
                m_storage.insert(first, inter);
                return *this;
            }

            auto last = std::prev(std::upper_bound(first, m_storage.end(), inter,
                [](const interval<S>& a, const interval<S>& b) {
                    return a < b && !a.mergeable(b);
                }
            ));

            S begin;
            bool first_included;
            if (inter.begin() < first->begin()) {
                begin = inter.begin();
                first_included = inter.first_included();
            } else if (first->begin() < inter.begin()) {
                begin = first->begin();
                first_included = first->first_included();
            } else {
                begin = inter.begin();
                first_included = first->first_included() || inter.first_included();
            }

            S end;
            bool last_included;
            if (inter.end() > last->end()) {
                end = inter.end();
                last_included = inter.last_included();
            } else if (last->end() > inter.end()) {
                end = last->end();
                last_included = last->last_included();
            } else {
                end = inter.end();
                last_included = last->last_included() || inter.last_included();
            }

            auto pos = m_storage.erase(first, std::next(last));
            m_storage.insert(pos, interval<S>(begin, end, first_included, last_included));

            return *this;
        }

        interval_union<S>& operator+=(const interval_union<S>& un) {
            for (const interval<S>& inter : un.data()) {
                *this += inter;
            }
            return *this;
        }

        interval_union<S> operator+(const interval<S>& inter) {
            interval_union<S> m = *this;
            m += inter;
            return m;
        }

        interval_union<S> operator+(const interval_union<S>& un) {
            interval_union<S> m = *this;
            for (const interval<S>& inter : un.data()) {
                m += inter;
            }
            return m;
        }

        interval_union<S>& operator-=(const interval<S>& inter) {
            auto first = std::lower_bound(m_storage.begin(), m_storage.end(), inter, [](const interval<S>& a, const interval<S>& b) {
                return a < b;
            });

            if (first == m_storage.end() || !first->mergeable(inter)) return *this;
            
            auto last = std::prev(std::upper_bound(first, m_storage.end(), inter,
                [](const interval<S>& a, const interval<S>& b) {
                    return a < b;
                }
            ));

            if (first->is_subinterval(inter)) {
                if (last->is_subinterval(inter)) {
                    m_storage.erase(first, std::next(last));
                } else {
                    auto pos = m_storage.erase(first, last);
                    *pos = interval<S>(inter.end(), pos->end(), !inter.last_included(), pos->last_included());
                }
            } else {
                if (last->is_subinterval(inter)) {
                    auto pos = std::prev(m_storage.erase(std::next(first), std::next(last)));
                    *pos = interval<S>(pos->begin(), inter.begin(), pos->first_included(), !inter.first_included());
                } else {
                    if (first == last) {
                        auto new_last = m_storage.insert(std::next(first), interval<S>(inter.end(), last->end(), !inter.last_included(), last->last_included()));
                        auto new_first = std::prev(new_last);
                        *new_first = interval<S>(new_first->begin(), inter.begin(), new_first->first_included(), !inter.first_included());
                    } else {
                        auto new_last = m_storage.erase(std::next(first), last);
                        auto new_first = std::prev(new_last);
                        *new_last = interval<S>(inter.end(), new_last->end(), !inter.last_included(), new_last->last_included());
                        *new_first = interval<S>(new_first->begin(), inter.begin(), new_first->first_included(), !inter.first_included());
                    }
                }
            }

            return *this;
        }

        interval_union<S>& operator-=(const interval_union<S>& un) {
            for (const auto& inter : un.data()) {
                *this -= inter;
            }
            return *this;
        }

        interval_union<S> operator-(const interval<S>& inter) {
            interval_union<S> s = *this;
            s -= inter;
            return s;
        }

        interval_union<S> operator-(const interval_union<S>& un) {
            interval_union<S> s = *this;
            for (const auto& inter : un.data()) {
                s -= inter;
            }
            return s;
        }

};


#endif  // HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_INTERVAL_UNION_HPP_
