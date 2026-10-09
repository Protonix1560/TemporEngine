
#ifndef INTERVAL_UNION_HPP_
#define INTERVAL_UNION_HPP_


#include <algorithm>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>
#include <span>
#include <cassert>


template <typename B>
requires std::is_arithmetic_v<B>
class interval {
    private:
        B m_begin{};
        B m_end{};
        bool m_first_included{};
        bool m_last_included{};
    public:
        using boundary_type = B;

        constexpr static interval<B> empty_set() noexcept { return interval<B>{}; }

        constexpr interval() noexcept = default;
        constexpr interval(B begin, B end, bool first_included = true, bool last_included = false) noexcept
            : m_begin(begin), m_end(end), m_first_included(first_included), m_last_included(last_included) {
            assert(end >= begin);
        }

        constexpr B set_begin() const noexcept { return m_begin; }
        constexpr B set_end() const noexcept { return m_end; }
        constexpr bool first_included() const noexcept { return m_first_included; }
        constexpr bool last_included() const noexcept { return m_last_included; }
        constexpr bool empty() const noexcept { return set_begin() == set_end() && !(first_included() && last_included()); }
        constexpr B length() const noexcept { return set_end() - set_begin(); }
        constexpr bool equivalent(const interval<B>& other) const noexcept {
            if (empty() || other.empty()) return empty() && other.empty();
            return (
                set_begin() == other.set_begin() && set_end() == other.set_end() &&
                first_included() == other.first_included() && last_included() == other.last_included()
            );
        }
        constexpr bool strictly_before(const interval<B>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_end() < other.set_begin()) return true;
            if (set_end() > other.set_begin()) return false;
            return !(last_included() && other.first_included());
        }
        constexpr bool strictly_after(const interval<B>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_begin() > other.set_end()) return true;
            if (set_begin() < other.set_end()) return false;
            return !(first_included() && other.last_included());
        }
        constexpr bool intersects(const interval<B>& other) const noexcept {
            if (empty() || other.empty()) return false;
            return !(strictly_before(other)) && !(other.strictly_before(*this));
        }
        constexpr bool mergeable(const interval<B>& other) const noexcept {
            if (empty() || other.empty()) return true;
            if (intersects(other)) return true;
            return (
                (set_end() == other.set_begin() && (last_included() || other.first_included())) ||
                (set_begin() == other.set_end() && (first_included() || other.last_included()))
            );
        }
        constexpr bool is_subinterval_of(const interval<B>& other) const noexcept {
            if (empty()) return true;
            if (other.empty()) return false;
            return (
                (set_begin() > other.set_begin() || (set_begin() == other.set_begin() && other.first_included() >= first_included())) &&
                (set_end() < other.set_end() || (set_end() == other.set_end() && other.last_included() >= last_included()))
            );
        }

        constexpr interval<B>& operator>>(B offset) noexcept {
            assert(offset <= std::numeric_limits<B>::max() - end());
            m_begin += offset;
            m_end += offset;
            return *this;
        }
        constexpr interval<B>& operator<<(B offset) noexcept {
            assert(offset <= begin() - std::numeric_limits<B>::lowest());
            m_begin -= offset;
            m_end -= offset;
            return *this;
        }

        constexpr interval<B>& operator|=(const interval<B>& other) noexcept {
            if (other.empty()) return *this;
            if (empty()) {
                *this = other;
                return *this;
            }
            assert(mergeable(other));
            if (other.set_begin() < set_begin()) {
                m_begin = other.set_begin();
                m_first_included = other.first_included();
            } else if (other.set_begin() == set_begin()) {
                m_first_included = other.first_included() || first_included();
            }
            if (other.set_end() > set_end()) {
                m_end = other.set_end();
                m_last_included = other.last_included();
            } else if (other.set_end() == set_end()) {
                m_last_included = other.last_included() || last_included();
            }
            return *this;
        }

        constexpr interval<B> operator|(const interval<B>& other) const noexcept {
            interval<B> inter = *this;
            inter |= other;
            return inter;
        }
        
        constexpr interval<B>& operator&=(const interval<B>& other) noexcept {
            if (!intersects(other)) {
                *this = {};
                return *this;
            }
            if (set_begin() < other.set_begin()) {
                m_begin = other.set_begin();
                m_first_included = other.first_included();
            } else if (other.set_begin() == set_begin()) {
                m_first_included = first_included() && other.first_included();
            }
            if (set_end() > other.set_end()) {
                m_end = other.set_end();
                m_last_included = other.last_included();
            } else if (other.set_end() == set_end()) {
                m_last_included = last_included() && other.last_included();
            }
            return *this;
        }

        constexpr interval<B> operator&(const interval<B>& other) const noexcept {
            interval<B> inter = *this;
            inter &= other;
            return inter;
        }

        constexpr std::pair<interval<B>, interval<B>> operator^(const interval<B>& other) const noexcept {
            if (empty()) return {other, *this};
            if (other.empty()) return {*this, other};
            if (!intersects(other)) {
                if (this->strictly_before(other)) return {*this, other};
                return {other, *this};
            }
            interval<B> m = *this | other;
            interval<B> i = *this & other;
            return {
                {m.set_begin(), i.set_begin(), m.first_included(), !i.first_included()},
                {i.set_end(), m.set_end(), !i.last_included(), m.last_included()}
            };
        }

        constexpr std::pair<interval<B>, interval<B>> operator-(const interval<B>& other) const noexcept {
            if (empty() || other.empty() || !intersects(other)) return {*this, {}};
            const interval<B> i = *this & other;
            interval<B> left{};
            interval<B> right{};
            if (set_begin() < i.set_begin()) left = {set_begin(), i.set_begin(), first_included(), !i.first_included()};
            if (i.set_end() < set_end()) right = {i.set_end(), set_end(), !i.last_included(), last_included()};
            return {left, right};
        }
};

template <typename B>
constexpr bool equivalent(const interval<B>& a, const interval<B>& b) noexcept {
    return a.equivalent(b);
}
template <typename B>
constexpr bool intersects(const interval<B>& a, const interval<B>& b) noexcept {
    return a.intersects(b);
}
template <typename B>
constexpr bool mergeable(const interval<B>& a, const interval<B>& b) noexcept {
    return a.mergeable(b);
}


template <typename B>
requires std::is_arithmetic_v<B>
class interval_union {
    private:
        std::vector<interval<B>> m_data;

    public:
        using interval_type = interval<B>;

        constexpr interval_union() noexcept = default;
        constexpr interval_union(const interval<B>& inter) { if (!inter.empty()) m_data.push_back(inter); }
        constexpr interval_union(const std::pair<interval<B>, interval<B>>& intervals) {
            if (intervals.first.mergeable(intervals.second)) {
                if (!intervals.first.empty() || !intervals.second.empty()) m_data.emplace_back(intervals.first | intervals.second);
            } else {
                assert(intervals.first < intervals.second);
                m_data.reserve(2);
                if (!intervals.first.empty()) m_data.emplace_back(intervals.first);
                if (!intervals.second.empty()) m_data.emplace_back(intervals.second);
            }
        }

        constexpr bool empty() const noexcept { return m_data.empty(); }
        constexpr B set_begin() const noexcept {
            assert(!m_data.empty());
            return m_data.front().set_begin();
        }
        constexpr B set_end() const noexcept {
            assert(!m_data.empty());
            return m_data.back().set_end();
        }
        constexpr bool first_included() const noexcept {
            assert(!m_data.empty());
            return m_data.front().first_included();
        }
        constexpr bool last_included() const noexcept {
            assert(!m_data.empty());
            return m_data.back().last_included();
        }

        constexpr auto begin() noexcept { return m_data.begin(); }
        constexpr auto begin() const noexcept { return m_data.begin(); }
        constexpr auto cbegin() noexcept { return m_data.cbegin(); }
        constexpr auto rbegin() noexcept { return m_data.rbegin(); }
        constexpr auto rbegin() const noexcept { return m_data.rbegin(); }
        constexpr auto crbegin() noexcept { return m_data.crbegin(); }

        constexpr auto end() noexcept { return m_data.end(); }
        constexpr auto end() const noexcept { return m_data.end(); }
        constexpr auto cend() noexcept { return m_data.cend(); }
        constexpr auto rend() noexcept { return m_data.rend(); }
        constexpr auto rend() const noexcept { return m_data.rend(); }
        constexpr auto crend() noexcept { return m_data.crend(); }

        constexpr interval_union<B>& operator|=(const interval<B>& inter) {
            if (inter.empty()) return *this;
            interval<B> merged = inter;
            auto first = m_data.begin();
            for (; first != m_data.end() && first->strictly_before(merged); first++) {}
            if (first != m_data.begin()) {
                auto prev = std::prev(first);
                if (prev->mergeable(merged)) first = prev;
            }
            auto last = first;
            for (; last != m_data.end() && last->mergeable(merged); last++) merged |= *last;
            auto pos = m_data.erase(first, last);
            m_data.insert(pos, merged);
            return *this;
        }
        constexpr interval_union<B>& operator|=(const interval_union<B>& un) {
            if (&un == this) return *this;
            for (const interval<B>& inter : un) *this |= inter;
            return *this;
        }
        constexpr interval_union<B> operator|(const interval<B>& inter) const {
            interval_union<B> m = *this;
            m |= inter;
            return m;
        }
        constexpr interval_union<B> operator|(const interval_union<B>& un) const {
            if (&un == this) return *this;
            interval_union<B> result = *this;
            result |= un;
            return result;
        }


        constexpr interval_union<B>& operator-=(const interval<B>& inter) {
            for (auto it = m_data.begin(); it != m_data.end();) {
                if (!it->intersects(inter)) {
                    it++;
                    continue;
                }
                auto [left, right] = *it - inter;
                it = m_data.erase(it);
                if (!left.empty()) it = std::next(m_data.insert(it, left));
                if (!right.empty()) it = std::next(m_data.insert(it, right));
            }
            return *this;
        }
        constexpr interval_union<B>& operator-=(const interval_union<B>& un) {
            if (&un == this) {
                m_data.clear();
                return *this;
            }
            for (const auto& inter : un) *this -= inter;
            return *this;
        }
        constexpr interval_union<B> operator-(const interval<B>& inter) {
            interval_union<B> result = *this;
            result -= inter;
            return result;
        }
        constexpr interval_union<B> operator-(const interval_union<B>& un) const {
            if (&un == this) return {};
            interval_union<B> result = *this;
            result -= un;
            return result;
        }


        constexpr interval_union<B>& operator&=(const interval<B>& inter) {
            for (auto it = m_data.begin(); it != m_data.end();) {
                auto result = *it & inter;
                it = m_data.erase(it);
                if (!result.empty()) it = std::next(m_data.insert(it, result));
            }
            return *this;
        }
        constexpr interval_union<B>& operator&=(const interval_union<B>& un) {
            if (&un == this) return *this;
            for (const auto& inter : un) *this &= inter;
            return *this;
        }
        constexpr interval_union<B> operator&(const interval<B>& inter) {
            interval_union<B> result = *this;
            result &= inter;
            return result;
        }
        constexpr interval_union<B> operator&(const interval_union<B>& un) const {
            if (&un == this) return *this;
            interval_union<B> result = *this;
            result &= un;
            return result;
        }


        constexpr interval_union<B>& operator>>(B offset) noexcept {
            assert(offset <= std::numeric_limits<B>::max() - end());
            for (auto& el : m_data) {
                el >> offset;
            }
            return *this;
        }
        constexpr interval_union<B>& operator<<(B offset) noexcept {
            assert(offset <= begin() - std::numeric_limits<B>::lowest());
            for (auto& el : m_data) {
                el << offset;
            }
            return *this;
        }
};


template <typename T>
class view_interval {
    private:
        std::span<T> m_data;
        size_t m_begin = 0;
    public:
        using boundary_type = size_t;
        using view_type = T;

        constexpr static view_interval<T> empty_set() noexcept { return view_interval<T>{}; }

        constexpr view_interval() noexcept = default;
        constexpr view_interval(std::span<T> data, size_t begin = 0) : m_begin(begin), m_data(data) {
            assert(m_data.size() <= std::numeric_limits<size_t>::max() - begin());
        }
        template <typename It>
        constexpr view_interval(It begin, It end, size_t set_begin = 0) : m_begin(set_begin), m_data(begin, end) {
            assert(m_data.size() <= std::numeric_limits<size_t>::max() - begin());
        }

        constexpr auto begin() noexcept { return m_data.begin(); }
        constexpr auto begin() const noexcept { return m_data.begin(); }
        constexpr auto cbegin() const noexcept { return m_data.cbegin(); }
        constexpr auto rbegin() noexcept { return m_data.rbegin(); }
        constexpr auto rbegin() const noexcept { return m_data.rbegin(); }
        constexpr auto crbegin() const noexcept { return m_data.crbegin(); }

        constexpr auto end() noexcept { return m_data.end(); }
        constexpr auto end() const noexcept { return m_data.end(); }
        constexpr auto cend() const noexcept { return m_data.cend(); }
        constexpr auto rend() noexcept { return m_data.rend(); }
        constexpr auto rend() const noexcept { return m_data.rend(); }
        constexpr auto crend() const noexcept { return m_data.crend(); }

        constexpr size_t set_begin() const noexcept { return m_begin; }
        constexpr size_t set_end() const noexcept { return m_begin + m_data.size(); }
        constexpr bool first_included() const noexcept { return true; }
        constexpr bool last_included() const noexcept { return false; }
        constexpr bool empty() const noexcept { return m_data.empty(); }
        constexpr size_t length() const noexcept { return m_data.size(); }
        constexpr operator interval<size_t>() const noexcept {
            return interval<size_t>{set_begin(), set_end()};
        }
        constexpr bool equivalent(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return empty() && other.empty();
            return (
                set_begin() == other.set_begin() && set_end() == other.set_end() &&
                first_included() == other.first_included() && last_included() == other.last_included()
            );
        }
        constexpr bool strictly_before(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_end() < other.set_begin()) return true;
            if (set_end() > other.set_begin()) return false;
            return !(last_included() && other.first_included());
        }
        constexpr bool strictly_after(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_begin() > other.set_end()) return true;
            if (set_begin() < other.set_end()) return false;
            return !(first_included() && other.last_included());
        }
        constexpr bool intersects(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            return !(strictly_before(other)) && !(other.strictly_before(interval<size_t>(*this)));
        }
        constexpr bool mergeable(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return true;
            if (intersects(other)) return true;
            return (
                (set_end() == other.set_begin() && (last_included() || other.first_included())) ||
                (set_begin() == other.set_end() && (first_included() || other.last_included()))
            );
        }
        constexpr bool contains(size_t index) const noexcept {
            assert(index != std::numeric_limits<size_t>::max());
            return interval<size_t>(index, index + 1).is_subinterval_of(*this);
        }
        constexpr bool is_subinterval_of(const interval<size_t>& other) const noexcept {
            if (empty()) return true;
            if (other.empty()) return false;
            return (
                (set_begin() > other.set_begin() || (set_begin() == other.set_begin() && other.first_included() >= first_included())) &&
                (set_end() < other.set_end() || (set_end() == other.set_end() && other.last_included() >= last_included()))
            );
        }

        constexpr T& operator[](size_t index) noexcept {
            assert(contains(index));
            return m_data[index];
        }
        constexpr const T& operator[](size_t index) const noexcept {
            assert(contains(index));
            return m_data[index];
        }

        constexpr view_interval<T>& operator>>(size_t offset) noexcept {
            assert(offset <= std::numeric_limits<size_t>::max() - end());
            m_begin += offset;
            return *this;
        }
        constexpr view_interval<T>& operator<<(size_t offset) noexcept {
            assert(offset <= begin());
            m_begin -= offset;
            return *this;
        }
};


template <typename T, typename C = std::vector<T>>
requires std::same_as<T, typename C::value_type>
class data_interval {
    private:
        C m_container;
        size_t m_begin = 0;
    public:
        using boundary_type = size_t;
        using value_type = T;
        using container_type = C;

        constexpr static data_interval<T, C> empty_set() noexcept { return data_interval<T, C>{}; }

        constexpr data_interval() noexcept = default;
        constexpr data_interval(const view_interval<T>& view) : m_container(view.begin(), view.end()) {}
        constexpr data_interval(const C& container, size_t begin = 0) : m_begin(begin), m_container(container) {
            assert(m_container.size() <= std::numeric_limits<size_t>::max() - begin());
        }
        constexpr data_interval(C&& container, size_t begin = 0) : m_begin(begin), m_container(std::move(container)) {
            assert(m_container.size() <= std::numeric_limits<size_t>::max() - begin());
        }

        constexpr operator interval<size_t>() const noexcept {
            return interval<size_t>{set_begin(), set_end()};
        }
        constexpr operator view_interval<T>() const noexcept {
            return view_interval<T>{m_container.begin(), m_container.end(), set_begin()};
        }

        constexpr const C& container() const noexcept { return m_container; }

        constexpr auto begin() noexcept { return m_container.begin(); }
        constexpr auto begin() const noexcept { return m_container.begin(); }
        constexpr auto cbegin() const noexcept { return m_container.cbegin(); }
        constexpr auto rbegin() noexcept { return m_container.rbegin(); }
        constexpr auto rbegin() const noexcept { return m_container.rbegin(); }
        constexpr auto crbegin() const noexcept { return m_container.crbegin(); }

        constexpr auto end() noexcept { return m_container.end(); }
        constexpr auto end() const noexcept { return m_container.end(); }
        constexpr auto cend() const noexcept { return m_container.cend(); }
        constexpr auto rend() noexcept { return m_container.rend(); }
        constexpr auto rend() const noexcept { return m_container.rend(); }
        constexpr auto crend() const noexcept { return m_container.crend(); }

        constexpr size_t set_begin() const noexcept { return m_begin; }
        constexpr size_t set_end() const noexcept { return m_begin + m_container.size(); }
        constexpr bool first_included() const noexcept { return true; }
        constexpr bool last_included() const noexcept { return false; }
        constexpr bool empty() const noexcept { return m_container.empty(); }
        constexpr size_t length() const noexcept { return m_container.size(); }
        constexpr bool equivalent(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return empty() && other.empty();
            return (
                set_begin() == other.set_begin() && set_end() == other.set_end() &&
                first_included() == other.first_included() && last_included() == other.last_included()
            );
        }
        constexpr bool strictly_before(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_end() < other.set_begin()) return true;
            if (set_end() > other.set_begin()) return false;
            return !(last_included() && other.first_included());
        }
        constexpr bool strictly_after(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            if (set_begin() > other.set_end()) return true;
            if (set_begin() < other.set_end()) return false;
            return !(first_included() && other.last_included());
        }
        constexpr bool intersects(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return false;
            return !(strictly_before(other)) && !(other.strictly_before(interval<size_t>(*this)));
        }
        constexpr bool mergeable(const interval<size_t>& other) const noexcept {
            if (empty() || other.empty()) return true;
            if (intersects(other)) return true;
            return (
                (set_end() == other.set_begin() && (last_included() || other.first_included())) ||
                (set_begin() == other.set_end() && (first_included() || other.last_included()))
            );
        }
        constexpr bool contains(size_t index) const noexcept {
            assert(index != std::numeric_limits<size_t>::max());
            return interval<size_t>(index, index + 1).is_subinterval_of(*this);
        }
        constexpr bool is_subinterval_of(const interval<size_t>& other) const noexcept {
            if (empty()) return true;
            if (other.empty()) return false;
            return (
                (set_begin() > other.set_begin() || (set_begin() == other.set_begin() && other.first_included() >= first_included())) &&
                (set_end() < other.set_end() || (set_end() == other.set_end() && other.last_included() >= last_included()))
            );
        }
        
        constexpr T& operator[](size_t index) noexcept {
            assert(contains(index));
            return m_container[index];
        }
        constexpr const T& operator[](size_t index) const noexcept {
            assert(contains(index));
            return m_container[index];
        }

        constexpr data_interval<T, C>& operator>>(size_t offset) noexcept {
            assert(offset <= std::numeric_limits<size_t>::max() - end());
            m_begin += offset;
            return *this;
        }
        constexpr data_interval<T, C>& operator<<(size_t offset) noexcept {
            assert(offset <= begin());
            m_begin -= offset;
            return *this;
        }

        constexpr data_interval<T, C>& operator|=(const view_interval<T>& other) {
            if (other.empty()) return *this;
            if (empty()) {
                *this = other;
                return *this;
            }
            assert(mergeable(other));
            auto new_interval = interval<size_t>{set_begin(), set_end()} | interval<size_t>{other.set_begin(), other.set_end()};
            auto [left, right] = interval<size_t>{set_begin(), set_end()} - interval<size_t>{other.set_begin(), other.set_end()};
            C new_container;
            new_container.resize(new_interval.length());
            if (!left.empty()) {
                std::move(
                    container().begin() + (left.set_begin() - set_begin()),
                    container().begin() + (left.set_end() - set_begin()),
                    new_container.begin() + (left.set_begin() - new_interval.set_begin())
                );
            }
            if (!right.empty()) {
                std::move(
                    container().begin() + (right.set_begin() - set_begin()),
                    container().begin() + (right.set_end() - set_begin()),
                    new_container.begin() + (right.set_begin() - new_interval.set_begin())
                );
            }
            std::copy(other.container().begin(), other.container().end(), new_container.begin() + (other.set_begin() - new_interval.set_begin()));
            m_container = std::move(new_container);
            m_begin = new_interval.set_begin();
            return *this;
        }

        constexpr data_interval<T, C> operator|(const view_interval<T>& other) const {
            data_interval<T, C> inter = *this;
            inter |= other;
            return inter;
        }
        
        constexpr data_interval<T, C>& operator&=(const interval<size_t>& inter) {
            interval<size_t> mask = inter;
            if (!intersects(mask)) {
                *this = {};
                return *this;
            }
            if (!mask.first_included()) mask = {mask.set_begin() + 1, mask.set_end()};
            if (!mask.last_included()) mask = {mask.set_begin(), mask.set_end() + 1};
            auto new_interval = interval<size_t>(*this) & interval<size_t>(mask);
            if (new_interval.empty()) {
                *this = {};
                return *this;
            }
            if (new_interval.set_begin() != set_begin()) {
                std::move(
                    container().begin() + (new_interval.set_begin() - set_begin()),
                    container().begin() + (new_interval.set_end() - set_begin()),
                    m_container.begin()
                );
            }
            m_container.resize(new_interval.length());
            m_begin = new_interval.set_begin();
            return *this;
        }

        constexpr data_interval<T, C> operator&(const interval<size_t>& other) const {
            data_interval<T, C> inter = *this;
            inter &= other;
            return inter;
        }

        constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator^(const view_interval<T>& other) const {
            if (empty()) return {other, *this};
            if (other.empty()) return {*this, other};
            if (!intersects(other)) {
                if (this->strictly_before(other)) return {*this, other};
                return {other, *this};
            }
            auto [left_interval, right_interval] = interval<size_t>(*this) ^ interval<size_t>(other);
            data_interval<T, C> left{left_interval.set_begin(), {}};
            data_interval<T, C> right{right_interval.set_begin(), {}};
            if (!left_interval.empty()) {
                left.container().resize(left_interval.length());
                if (intersects(left_interval)) {
                    std::copy(
                        container().begin() + (left_interval.set_begin() - set_begin()),
                        container().begin() + (left_interval.set_end() - set_begin()),
                        left.m_container.begin()
                    );
                } else {
                    std::copy(
                        other.container().begin() + (left_interval.set_begin() - other.set_begin()),
                        other.container().begin() + (left_interval.set_end() - other.set_begin()),
                        left.m_container().begin()
                    );
                }
            }
            if (!right_interval.empty()) {
                right.container().resize(right_interval.length());
                if (intersects(right_interval)) {
                    std::copy(
                        container().begin() + (right_interval.set_begin() - set_begin()),
                        container().begin() + (right_interval.set_end() - set_begin()),
                        right.m_container.begin()
                    );
                } else {
                    std::copy(
                        other.container().begin() + (right_interval.set_begin() - other.set_begin()),
                        other.container().begin() + (right_interval.set_end() - other.set_begin()),
                        right.m_container.begin()
                    );
                }
            }
            return {left, right};
        }

        constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator-(const interval<size_t>& inter) const {
            interval<size_t> mask = inter;
            if (empty() || mask.empty() || !intersects(mask)) return {*this, {}};
            if (!mask.first_included()) mask = {mask.set_begin() + 1, mask.set_end()};
            if (mask.last_included()) mask = {mask.set_begin(), mask.set_end() + 1};
            auto [left_interval, right_interval] = interval<size_t>(*this) - mask;
            data_interval<T, C> left{{}, left_interval.set_begin()};
            data_interval<T, C> right{{}, right_interval.set_begin()};
            if (!left_interval.empty()) {
                left.m_container.resize(left_interval.length());
                std::copy(
                    container().begin() + (left_interval.set_begin() - set_begin()),
                    container().begin() + (left_interval.set_end() - set_begin()),
                    left.m_container.begin()
                );
            }
            if (!right_interval.empty()) {
                right.m_container.resize(right_interval.length());
                std::copy(
                    container().begin() + (right_interval.set_begin() - set_begin()),
                    container().begin() + (right_interval.set_end() - set_begin()),
                    right.m_container.begin()
                );
            }
            return {left, right};
        }
};


template <typename T, typename C = std::vector<T>>
requires std::same_as<T, typename C::value_type>
class data_interval_union {
    public:
        struct holder {
            private:
                data_interval<T, C> m_data{};
                constexpr data_interval<T, C>& data() noexcept { return m_data; }
                friend class data_interval_union<T, C>;

            public:
                constexpr holder() noexcept = default;
                constexpr holder(const data_interval<T, C>& interval) : m_data(interval) {}
                constexpr holder(data_interval<T, C>&& interval) : m_data(std::move(interval)) {}
                constexpr holder(const C& container, size_t begin = 0) : m_data(container, begin) {}
                constexpr holder(C&& container, size_t begin = 0) : m_data(std::move(container), begin) {}

                constexpr holder(const holder& other) = default;
                #error "vector wants operator=, but it musn't be public"

                constexpr const C& container() const noexcept { return m_data.container(); }

                constexpr auto begin() noexcept { return m_data.begin(); }
                constexpr auto begin() const noexcept { return m_data.end(); }
                constexpr auto cbegin() const noexcept { return m_data.cbegin(); }
                constexpr auto rbegin() noexcept { return m_data.rbegin(); }
                constexpr auto rbegin() const noexcept { return m_data.rbegin(); }
                constexpr auto crbegin() const noexcept { return m_data.crbegin(); }
                constexpr auto end() noexcept { return m_data.end(); }
                constexpr auto end() const noexcept { return m_data.end(); }
                constexpr auto cend() const noexcept { return m_data.cend(); }
                constexpr auto rend() noexcept { return m_data.rend(); }
                constexpr auto rend() const noexcept { return m_data.rend(); }
                constexpr auto crend() const noexcept { return m_data.crend(); }

                constexpr size_t set_begin() const noexcept { return m_data.set_begin(); }
                constexpr size_t set_end() const noexcept { return m_data.set_end(); }
                constexpr bool first_included() const noexcept { return m_data.first_included(); }
                constexpr bool last_included() const noexcept { return m_data.last_included(); }
                constexpr bool empty() const noexcept { return m_data.empty(); }
                constexpr size_t length() const noexcept { return m_data.length(); }
                constexpr operator interval<size_t>() const noexcept { return interval<size_t>(m_data); }
                constexpr bool equivalent(const interval<size_t>& other) const noexcept { return m_data.equivalent(other); }
                constexpr bool strictly_before(const interval<size_t>& other) const noexcept { return m_data.strictly_before(other); }
                constexpr bool strictly_after(const interval<size_t>& other) const noexcept { return m_data.strictly_after(other); }
                constexpr bool intersects(const interval<size_t>& other) const noexcept { return m_data.intersects(other); }
                constexpr bool mergeable(const interval<size_t>& other) const noexcept { return m_data.mergeable(other); }
                constexpr bool contains(size_t index) const noexcept { return m_data.contains(index); }
                constexpr bool is_subinterval_of(const interval<size_t>& other) const noexcept { return m_data.is_subinterval_of(other); }
                constexpr T& operator[](size_t index) noexcept { return m_data[index]; }
                constexpr const T& operator[](size_t index) const noexcept { return m_data[index]; }

                constexpr data_interval<T, C> operator|(const view_interval<T>& other) const { return m_data - other; }
                constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator-(const interval<size_t>& other) const { return m_data - other; }
                constexpr data_interval<T, C> operator&(const interval<T>& other) const { return m_data - other; }
                constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator^(const view_interval<T>& other) const { return m_data - other; }
        };

    private:
        std::vector<holder> m_data;

    public:
        using interval_type = data_interval<T, C>;
        using value_type = holder;

        constexpr data_interval_union<T, C> empty_set() const noexcept { return data_interval_union<T, C>{}; }

        constexpr data_interval_union() noexcept = default;
        constexpr data_interval_union(const data_interval<T, C>& inter) { if (!inter.empty()) m_data.push_back(inter); }
        constexpr data_interval_union(const std::pair<data_interval<T, C>, data_interval<T, C>>& intervals) {
            if (intervals.first.mergeable(intervals.second)) {
                if (!intervals.first.empty() || !intervals.second.empty()) m_data.emplace_back(intervals.first | intervals.second);
            } else {
                assert(intervals.first.strictly_before(intervals.second));
                m_data.reserve(2);
                if (!intervals.first.empty()) m_data.emplace_back(intervals.first);
                if (!intervals.second.empty()) m_data.emplace_back(intervals.second);
            }
        }

        constexpr bool empty() const noexcept { return m_data.empty(); }
        constexpr size_t set_begin() const noexcept {
            assert(!m_data.empty());
            return m_data.front().set_begin();
        }
        constexpr size_t set_end() const noexcept {
            assert(!m_data.empty());
            return m_data.back().set_end();
        }
        constexpr bool first_included() const noexcept {
            assert(!m_data.empty());
            return m_data.front().first_included();
        }
        constexpr bool last_included() const noexcept {
            assert(!m_data.empty());
            return m_data.back().last_included();
        }

        constexpr auto begin() noexcept { return m_data.begin(); }
        constexpr auto begin() const noexcept { return m_data.begin(); }
        constexpr auto cbegin() noexcept { return m_data.cbegin(); }
        constexpr auto rbegin() noexcept { return m_data.rbegin(); }
        constexpr auto rbegin() const noexcept { return m_data.rbegin(); }
        constexpr auto crbegin() noexcept { return m_data.crbegin(); }

        constexpr auto end() noexcept { return m_data.end(); }
        constexpr auto end() const noexcept { return m_data.end(); }
        constexpr auto cend() noexcept { return m_data.cend(); }
        constexpr auto rend() noexcept { return m_data.rend(); }
        constexpr auto rend() const noexcept { return m_data.rend(); }
        constexpr auto crend() noexcept { return m_data.crend(); }

        constexpr operator interval_union<size_t>() const noexcept {
            interval_union<size_t> un;
            for (const auto& inter : *this) {
                un |= interval<size_t>(inter);
            }
            return un;
        }

        constexpr bool intersects(const interval<size_t>& inter) {
            if (inter.empty()) return false;
            for (const auto& el : *this) {
                if (el.intersects(inter)) return true;
            }
            return false;
        }
        constexpr bool contains(size_t index) const noexcept {
            assert(index != std::numeric_limits<size_t>::max());
            return intersects(interval<size_t>(index, index + 1));
        }
        constexpr T& operator[](size_t index) noexcept {
            assert(contains(index));
            for (auto& el : m_data) {
                if (el.contains(index - el.begin())) return el[index - el.begin()];
            }
        }
        constexpr const T& operator[](size_t index) const noexcept {
            assert(contains(index));
            for (auto& el : m_data) {
                if (el.contains(index - el.begin())) return el[index - el.begin()];
            }
        }

        constexpr data_interval_union<T, C>& operator|=(const view_interval<T>& inter) {
            if (inter.empty()) return *this;
            data_interval<T, C> merged = inter;
            auto first = m_data.begin();
            for (; first != m_data.end() && first->strictly_before(merged); first++) {}
            if (first != m_data.begin()) {
                auto prev = std::prev(first);
                if (prev->mergeable(merged)) first = prev;
            }
            auto last = first;
            for (; last != m_data.end() && last->mergeable(merged); last++) merged |= *last;
            auto pos = m_data.erase(first, last);
            m_data.insert(pos, merged);
            return *this;
        }
        constexpr data_interval_union<T, C>& operator|=(const data_interval_union<T, C>& un) {
            if (&un == this) return *this;
            for (const data_interval<T, C>& inter : *this) *this |= inter;
            return *this;
        }
        constexpr data_interval_union<T, C> operator|(const view_interval<T>& inter) const {
            data_interval_union<T, C> m = *this;
            m |= inter;
            return m;
        }
        constexpr data_interval_union<T, C> operator|(const data_interval_union<T, C>& un) const {
            if (&un == this) return *this;
            data_interval_union<T, C> result = *this;
            result |= un;
            return result;
        }


        constexpr data_interval_union<T, C>& operator-=(const interval<size_t>& inter) {
            for (auto it = m_data.begin(); it != m_data.end();) {
                if (!it->intersects(inter)) {
                    it++;
                    continue;
                }
                auto [left, right] = *it - inter;
                it = m_data.erase(it);
                if (!left.empty()) it = std::next(m_data.insert(it, left));
                if (!right.empty()) it = std::next(m_data.insert(it, right));
            }
            return *this;
        }
        constexpr data_interval_union<T, C>& operator-=(const interval_union<size_t>& un) {
            for (const auto& inter : un) *this -= inter;
            return *this;
        }
        constexpr data_interval_union<T, C> operator-(const interval<size_t>& inter) {
            data_interval_union<T, C> result = *this;
            result -= inter;
            return result;
        }
        constexpr data_interval_union<T, C> operator-(const interval_union<size_t>& un) const {
            data_interval_union<T, C> result = *this;
            result -= un;
            return result;
        }


        constexpr data_interval_union<T, C>& operator&=(const interval<size_t>& inter) {
            for (auto it = m_data.begin(); it != m_data.end();) {
                auto result = *it & inter;
                it = m_data.erase(it);
                if (!result.empty()) it = std::next(m_data.insert(it, result));
            }
            return *this;
        }
        constexpr data_interval_union<T, C>& operator&=(const interval_union<size_t>& un) {
            for (const auto& inter : un) *this &= inter;
            return *this;
        }
        constexpr data_interval_union<T, C> operator&(const interval<size_t>& inter) {
            data_interval_union<T, C> result = *this;
            result &= inter;
            return result;
        }
        constexpr data_interval_union<T, C> operator&(const interval_union<size_t>& un) const {
            data_interval_union<T, C> result = *this;
            result &= un;
            return result;
        }

        template <std::random_access_iterator It>
        constexpr void copy_subset(const interval_union<size_t>& un, It dst) {
            if (empty() || un.empty()) return;
            auto src_it = begin();
            auto req_it = un.begin();
            while (src_it != end() && req_it != un.end()) {
                const auto& src = *src_it;
                const auto& req = *req_it;
                if (src.strictly_before(req)) {
                    src_it++;
                } else if (req.strictly_before(interval<size_t>(src))) {
                    req_it++;
                } else {
                    const interval<size_t> intersection = interval<size_t>(src) & req;
                    if (!intersection.empty()) {
                        std::copy(
                            src.container_begin() + (intersection.set_begin() - src.begin()),
                            src.container_begin() + (intersection.set_end() - src.begin()),
                            dst + (intersection.set_begin() - un.set_begin())
                        );
                    }
                    if (src.set_end() < req.set_end()) {
                        src_it++;
                    } else {
                        req_it++;
                    }
                }
            }
        }
        template <std::random_access_iterator It>
        constexpr void copy_subset(const interval<size_t>& inter, It dst) {
            return copy_subset(interval_union<size_t>(inter), dst);
        }


        constexpr data_interval_union<T, C>& operator>>(size_t offset) noexcept {
            assert(offset <= std::numeric_limits<size_t>::max() - end());
            for (auto& el : m_data) {
                el >> offset;
            }
            return *this;
        }
        constexpr data_interval_union<T, C>& operator<<(size_t offset) noexcept {
            assert(offset <= begin());
            for (auto& el : m_data) {
                el << offset;
            }
            return *this;
        }
};


#endif  // INTERVAL_UNION_HPP_
