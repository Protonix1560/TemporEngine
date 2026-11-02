
#ifndef INTERVAL_UNION_HPP_
#define INTERVAL_UNION_HPP_

#include <algorithm>
#include <concepts>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>
#include <span>
#include <cassert>
#include <functional>

#include <iostream>


namespace mat {

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
            constexpr bool subinterval_of(const interval<B>& other) const noexcept {
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

            constexpr interval<B>& merge_with(const interval<B>& other) noexcept {
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
            constexpr interval<B> merged(const interval<B>& other) const noexcept { 
                interval<B> result = *this;
                result.merge_with(other);
                return result;
            }
            constexpr interval<B>& operator|=(const interval<B>& other) noexcept { return merge_with(other); }
            constexpr interval<B> operator|(const interval<B>& other) const noexcept { return merged(other); }
            

            constexpr interval<B>& intersect_with(const interval<B>& other) noexcept {
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
            constexpr interval<B> intersected(const interval<B>& other) const noexcept { 
                interval<B> result = *this;
                result.intersect_with(other);
                return result;
            }
            constexpr interval<B>& operator&=(const interval<B>& other) noexcept { return intersect_with(other); }
            constexpr interval<B> operator&(const interval<B>& other) const noexcept { return intersected(other); }


            constexpr std::pair<interval<B>, interval<B>> exclusive_unioned(const interval<B>& other) const noexcept {
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
            constexpr std::pair<interval<B>, interval<B>> operator^(const interval<B>& other) const noexcept { return exclusive_unioned(other); }


            constexpr std::pair<interval<B>, interval<B>> subtracted(const interval<B>& other) const noexcept {
                if (empty() || other.empty() || !intersects(other)) return {*this, {}};
                const interval<B> i = *this & other;
                interval<B> left{};
                interval<B> right{};
                if (set_begin() < i.set_begin()) left = {set_begin(), i.set_begin(), first_included(), !i.first_included()};
                if (i.set_end() < set_end()) right = {i.set_end(), set_end(), !i.last_included(), last_included()};
                return {left, right};
            }
            constexpr std::pair<interval<B>, interval<B>> operator-(const interval<B>& other) const noexcept { return subtracted(other); }
    };


    template <typename B>
    requires std::is_arithmetic_v<B>
    class interval_union {
        private:
            std::vector<interval<B>> m_data;

        public:
            using interval_type = interval<B>;
            using iterator = std::vector<interval<B>>::iterator;
            using const_iterator = std::vector<interval<B>>::const_iterator;

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

            constexpr interval<B>& front() noexcept { return m_data.front(); }
            constexpr const interval<B>& front() const noexcept { return m_data.front(); }
            constexpr interval<B>& back() noexcept { return m_data.back(); }
            constexpr const interval<B>& back() const noexcept { return m_data.back(); }

            constexpr auto erase(const_iterator it) { return m_data.erase(it); }
            constexpr auto erase(const_iterator begin, const_iterator end) { return m_data.erase(begin, end); }
            constexpr void clear() { m_data.clear(); }

            constexpr interval_union<B>& operator>>(B offset) noexcept {
                assert(offset <= std::numeric_limits<B>::max() - end());
                for (auto& el : m_data) el >> offset;
                return *this;
            }
            constexpr interval_union<B>& operator<<(B offset) noexcept {
                assert(offset <= begin() - std::numeric_limits<B>::lowest());
                for (auto& el : m_data) el << offset;
                return *this;
            }


            constexpr interval_union<B>& merge_with(const interval<B>& inter) {
                if (inter.empty()) return *this;
                interval<B> merged = inter;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->mergeable(merged) && !it->strictly_after(merged)) it++;
                while (it != m_data.end() && it->mergeable(merged)) {
                    merged.merge_with(*it);
                    it = m_data.erase(it);
                }
                m_data.insert(it, merged);
                return *this;
            }
            constexpr interval_union<B>& merge_with(const interval_union<B>& un) {
                if (&un == this) return *this;
                for (const auto& inter : *this) {
                    merge_with(inter);
                }
                return *this;
            }
            constexpr interval_union<B> merged(const interval<B>& inter) const {
                interval_union<B> result = *this;
                result.merge_with(inter);
                return result;
            }
            constexpr interval_union<B> merged(const interval_union<B>& un) const {
                if (&un == this) return *this;
                interval_union<B> result = *this;
                result.merge_with(un);
                return result;
            }
            constexpr interval_union<B>& operator|=(const interval<B>& inter) { return merge_with(inter); }
            constexpr interval_union<B>& operator|=(const interval_union<B>& un) { return merge_with(un); }
            constexpr interval_union<B> operator|(const interval<B>& un) const { return merged(un); }
            constexpr interval_union<B> operator|(const interval_union<B>& un) const { return merged(un); }


            constexpr interval_union<B>& intersect_with(const interval<size_t>& inter) {
                for (auto it = m_data.begin(); it != m_data.end();) {
                    auto result = *it & inter;
                    it = m_data.erase(it);
                    if (!result.empty()) it = std::next(m_data.insert(it, result));
                }
                return *this;
            }
            constexpr interval_union<B>& intersect_with(const interval_union<size_t>& un) {
                for (const auto& inter : un) intersect_with(inter);
                return *this;
            }
            constexpr interval_union<B> intersected(const interval<size_t>& inter) {
                interval_union<B> result = *this;
                result.intersect_with(inter);
                return result;
            }
            constexpr interval_union<B> intersected(const interval_union<size_t>& un) const {
                interval_union<B> result = *this;
                result.intersect_with(un);
                return result;
            }
            constexpr interval_union<B>& operator&=(const interval<size_t>& inter) { return intersect_with(inter); }
            constexpr interval_union<B>& operator&=(const interval_union<size_t>& un) { return intersect_with(un); }
            constexpr interval_union<B> operator&(const interval<size_t>& inter) const { return intersected(inter); }
            constexpr interval_union<B> operator&(const interval_union<size_t>& un) const { return intersected(un); }


            constexpr interval_union<B>& exclusive_union_with(const interval<B>& inter) {
                if (inter.empty()) return *this;
                interval<B> remain = inter;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->intersects(remain) && !it->strictly_after(remain)) it++;
                while (it != m_data.end() && remain.empty()) {
                    auto el = interval<size_t>(*it);
                    auto [left, right] = remain.exclusive_unioned(it);
                    it = m_data.erase(it);
                    if (!left.empty()) it = m_data.insert(it, left);
                    if (!right.empty() && !right.strictly_after(el)) {
                        it = m_data.insert(it, right);
                        break;
                    }
                    remain = std::move(right);
                }
                m_data.insert(it, std::move(remain));
                return *this;
            }
            constexpr interval_union<B>& exclusive_union_with(const interval_union<B>& un) {
                if (&un == this) {
                    *this = {};
                    return *this;
                }
                for (const auto& el : un) exclusive_union_with(interval<B>(el));
                return *this;
            }
            constexpr interval_union<B> exclusive_unioned(const interval<B>& inter) const {
                interval_union<B> result = *this;
                result.exclusive_union_with(inter);
                return result;
            }
            constexpr interval_union<B> exclusive_unioned(const interval_union<B>& inter) const {
                interval_union<B> result = *this;
                result.exclusive_union_with(inter);
                return result;
            }
            constexpr interval_union<B>& operator^=(const interval<B>& inter) { return exclusive_union_with(inter); }
            constexpr interval_union<B>& operator^=(const interval_union<B>& un) { return exclusive_union_with(un); }
            constexpr interval_union<B>& operator^(const interval<B>& inter) const { return exclusive_unioned(inter); }
            constexpr interval_union<B>& operator^(const interval_union<B>& un) const { return exclusive_unioned(un); }


            constexpr interval_union<B>& subtract_with(const interval<size_t>& inter) {
                if (inter.empty()) return *this;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->intersects(inter) && !it->strictly_after(inter)) it++;
                while (it != m_data.end() && it->intersects(inter)) {
                    auto [left, right] = it->subtracted(inter);
                    it = m_data.erase(it);
                    if (!left.empty()) it = std::next(m_data.insert(it, left));
                    if (!right.empty()) it = std::next(m_data.insert(it, right));
                }
                return *this;
            }
            constexpr interval_union<B>& subtract_with(const interval_union<size_t>& un) {
                if (&un == this) {
                    *this = {};
                    return *this;
                }
                for (const auto& inter : un) subtract_with(inter);
                return *this;
            }
            constexpr interval_union<B> subtracted(const interval<size_t>& inter) const {
                interval_union<B> result = *this;
                result.subtract_with(inter);
                return result;
            }
            constexpr interval_union<B> subtracted(const interval_union<size_t>& un) const {
                interval_union<B> result = *this;
                result.subtract_with(un);
                return result;
            }
            constexpr interval_union<B>& operator-=(const interval<size_t>& inter) { return subtract_with(inter); }
            constexpr interval_union<B>& operator-=(const interval_union<size_t>& un) { return subtract_with(un); }
            constexpr interval_union<B> operator-(const interval<size_t>& inter) const { return subtracted(inter); }
            constexpr interval_union<B> operator-(const interval_union<size_t>& un) const { return subtracted(un); }
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

            constexpr interval<size_t> set() const noexcept { return interval<size_t>(set_begin(), set_end()); }
            constexpr operator interval<size_t>() const noexcept { return set(); }
            constexpr operator view_interval<const T>() const noexcept { return view_interval<const T>(m_data.begin(), m_data.end()); }

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

            constexpr T& front() noexcept { return m_data.front(); }
            constexpr const T& front() const noexcept { return m_data.front(); }
            constexpr T& back() noexcept { return m_data.back(); }
            constexpr const T& back() const noexcept { return m_data.back(); }

            constexpr T* data() noexcept { return m_data.data(); }
            constexpr T* data() const noexcept { return m_data.data(); }

            constexpr T& operator[](size_t index) noexcept {
                assert(contains(index));
                return m_data[index - set_begin()];
            }
            constexpr const T& operator[](size_t index) const noexcept {
                assert(contains(index));
                return m_data[index - set_begin()];
            }

            constexpr size_t set_begin() const noexcept { return m_begin; }
            constexpr size_t set_end() const noexcept { return m_begin + m_data.size(); }
            constexpr bool first_included() const noexcept { return true; }
            constexpr bool last_included() const noexcept { return false; }
            constexpr bool empty() const noexcept { return m_data.empty(); }
            constexpr size_t length() const noexcept { return m_data.size(); }
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
                return !(strictly_before(other)) && !(other.strictly_before(set()));
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
                return interval<size_t>(index, index + 1).subinterval_of(*this);
            }
            constexpr bool subinterval_of(const interval<size_t>& other) const noexcept {
                if (empty()) return true;
                if (other.empty()) return false;
                return (
                    (set_begin() > other.set_begin() || (set_begin() == other.set_begin() && other.first_included() >= first_included())) &&
                    (set_end() < other.set_end() || (set_end() == other.set_end() && other.last_included() >= last_included()))
                );
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


    template <typename T>
    struct comb_new_t {
        T operator()(const T& a, const T& b) { return b; }
        T operator()(const T& a, T& b) { return std::move(b); }
    };
    template <typename T>
    comb_new_t<T> comb_new{};
    
    template <typename T>
    struct comb_keep_t {
        T operator()(const T& a, const T& b) { return a; }
        T operator()(T& a, const T& b) { return std::move(a); }
    };
    template <typename T>
    comb_keep_t<T> comb_keep{};

    
    template <typename T, typename C = std::vector<T>>
    requires std::same_as<T, typename C::value_type>
    class data_interval {
        private:
            C m_container;
            size_t m_begin = 0;

            constexpr static interval<size_t> normalise(const interval<size_t>& inter) {
                interval<size_t> result = inter;
                if (!result.first_included()) result = {inter.set_begin() + 1, inter.set_end(), true, result.last_included()};
                if (result.last_included()) result = {inter.set_begin(), inter.set_end() + 1};
                return result;
            }

        public:
            using boundary_type = size_t;
            using value_type = T;
            using container_type = C;

            constexpr static data_interval<T, C> empty_set() noexcept { return data_interval<T, C>{}; }

            constexpr data_interval() noexcept = default;
            constexpr data_interval(const view_interval<const T>& view) : m_container(view.begin(), view.end()), m_begin(view.set_begin()) {}
            constexpr data_interval(const C& container, size_t begin = 0) : m_begin(begin), m_container(container) {
                assert(m_container.size() <= std::numeric_limits<size_t>::max() - begin());
            }
            constexpr data_interval(C&& container, size_t begin = 0) : m_begin(begin), m_container(std::move(container)) {
                assert(m_container.size() <= std::numeric_limits<size_t>::max() - begin());
            }
            constexpr data_interval(std::initializer_list<T> list, size_t begin = 0) : m_begin(begin), m_container(list) {
                assert(m_container.size() <= std::numeric_limits<size_t>::max() - begin());
            }

            constexpr interval<size_t> set() const noexcept { return interval<size_t>(set_begin(), set_end()); }
            constexpr operator interval<size_t>() const noexcept { return set(); }
            constexpr view_interval<T> view() noexcept { return view_interval<T>{m_container.begin(), m_container.end(), set_begin()}; }
            constexpr operator view_interval<T>() noexcept { return view(); }
            constexpr view_interval<const T> view() const noexcept { return view_interval<const T>{m_container.begin(), m_container.end(), set_begin()}; }
            constexpr view_interval<const T> cview() const noexcept { return view_interval<const T>{m_container.begin(), m_container.end(), set_begin()}; }
            constexpr operator view_interval<const T>() const noexcept { return cview(); }

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

            constexpr T& front() noexcept { return m_container.front(); }
            constexpr const T& front() const noexcept { return m_container.front(); }
            constexpr T& back() noexcept { return m_container.back(); }
            constexpr const T& back() const noexcept { return m_container.back(); }

            constexpr T* data() noexcept { return m_container.data(); }
            constexpr const T* data() const noexcept { return m_container.data(); }
            
            constexpr T& operator[](size_t index) noexcept {
                assert(contains(index));
                return m_container[index - set_begin()];
            }
            constexpr const T& operator[](size_t index) const noexcept {
                assert(contains(index));
                return m_container[index - set_begin()];
            }

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
                return !(strictly_before(other)) && !(other.strictly_before(set()));
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
                return interval<size_t>(index, index + 1).subinterval_of(*this);
            }
            constexpr bool subinterval_of(const interval<size_t>& other) const noexcept {
                if (empty()) return true;
                if (other.empty()) return false;
                return (
                    (set_begin() > other.set_begin() || (set_begin() == other.set_begin() && other.first_included() >= first_included())) &&
                    (set_end() < other.set_end() || (set_end() == other.set_end() && other.last_included() >= last_included()))
                );
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


            template <typename Comb> requires std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval<T, C>& merge_with(const view_interval<const T>& other, Comb&& comb) {
                if (other.empty()) return *this;
                if (empty()) {
                    *this = other;
                    return *this;
                }
                assert(mergeable(other));
                auto new_interval = set() | other.set();
                C new_container;
                new_container.resize(new_interval.length());
                {
                    auto [left, right] = set() - other.set();
                    if (!left.empty()) std::move(
                        begin() + (left.set_begin() - set_begin()),
                        begin() + (left.set_end() - set_begin()),
                        new_container.begin() + (left.set_begin() - new_interval.set_begin())
                    );
                    if (!right.empty()) std::move(
                        begin() + (right.set_begin() - set_begin()),
                        begin() + (right.set_end() - set_begin()),
                        new_container.begin() + (right.set_begin() - new_interval.set_begin())
                    );
                }
                {
                    auto [left, right] = other.set() - set();
                    if (!left.empty()) std::copy(
                        other.begin() + (left.set_begin() - other.set_begin()),
                        other.begin() + (left.set_end() - other.set_begin()),
                        new_container.begin() + (left.set_begin() - new_interval.set_begin())
                    );
                    if (!right.empty()) std::copy(
                        other.begin() + (right.set_begin() - other.set_begin()),
                        other.begin() + (right.set_end() - other.set_begin()),
                        new_container.begin() + (right.set_begin() - new_interval.set_begin())
                    );
                }
                {
                    auto inter = set() & other.set();
                    for (size_t i = inter.set_begin(); i < inter.set_end(); i++) {
                        new_container[i - new_interval.set_begin()] = std::invoke(comb, (*this)[i], other[i]);
                    }
                }
                m_container = std::move(new_container);
                m_begin = new_interval.set_begin();
                return *this;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, T&>
            constexpr data_interval<T, C>& merge_with(data_interval<T, S>&& other, Comb&& comb) {
                if (other.empty()) return *this;
                if (empty()) {
                    *this = other;
                    return *this;
                }
                assert(mergeable(other));
                auto new_interval = set() | other.set();
                C new_container;
                new_container.resize(new_interval.length());
                {
                    auto [left, right] = set() - other.set();
                    if (!left.empty()) std::move(
                        begin() + (left.set_begin() - set_begin()),
                        begin() + (left.set_end() - set_begin()),
                        new_container.begin() + (left.set_begin() - new_interval.set_begin())
                    );
                    if (!right.empty()) std::move(
                        begin() + (right.set_begin() - set_begin()),
                        begin() + (right.set_end() - set_begin()),
                        new_container.begin() + (right.set_begin() - new_interval.set_begin())
                    );
                }
                {
                    auto [left, right] = other.set() - set();
                    if (!left.empty()) std::move(
                        other.begin() + (left.set_begin() - other.set_begin()),
                        other.begin() + (left.set_end() - other.set_begin()),
                        new_container.begin() + (left.set_begin() - new_interval.set_begin())
                    );
                    if (!right.empty()) std::move(
                        other.begin() + (right.set_begin() - other.set_begin()),
                        other.begin() + (right.set_end() - other.set_begin()),
                        new_container.begin() + (right.set_begin() - new_interval.set_begin())
                    );
                }
                {
                    auto inter = set() & other.set();
                    for (size_t i = inter.set_begin(); i < inter.set_end(); i++) {
                        new_container[i - new_interval.set_begin()] = std::invoke(comb, (*this)[i], other[i]);
                    }
                }
                m_container = std::move(new_container);
                m_begin = new_interval.set_begin();
                return *this;
            }
            constexpr data_interval<T, C>& merge_with(const view_interval<const T>& other) { return merge_with(other, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval<T, C>& merge_with(data_interval<T, S>&& other) { return merge_with(std::move(other), comb_new<T>); }
            constexpr data_interval<T, C>& operator|=(const view_interval<const T>& other) { return merge_with(other); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval<T, C>& operator|=(data_interval<T, S>&& other) { return merge_with(std::move(other)); }

            template <typename Comb> requires std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval<T, C> merged(const view_interval<const T>& other, Comb&& comb) const {
                data_interval<T, C> result = *this;
                result.merge_with(other, std::forward<Comb>(comb));
                return result;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, T&>
            constexpr data_interval<T, C> merged(data_interval<T, S>&& other, Comb&& comb) const {
                data_interval<T, C> result = *this;
                result.merge_with(std::move(other), std::forward<Comb>(comb));
                return result;
            }
            constexpr data_interval<T, C> merged(const view_interval<const T>& other) const { return merged(other, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval<T, C> merged(data_interval<T, S>&& other) const { return merged(std::move(other), comb_new<T>); }
            constexpr data_interval<T, C> operator|(const view_interval<const T>& other) const { return merged(other); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval<T, C> operator|(data_interval<T, S>&& other) const { return merged(std::move(other)); }
        
            
            constexpr data_interval<T, C>& intersect_with(const interval<size_t>& other) {
                auto mask = normalise(other);
                if (!intersects(mask)) {
                    *this = {};
                    return *this;
                }
                auto new_interval = set() & interval<size_t>(mask);
                if (new_interval.set_begin() != set_begin()) {
                    std::move(
                        begin() + (new_interval.set_begin() - set_begin()),
                        begin() + (new_interval.set_end() - set_begin()),
                        m_container.begin()
                    );
                }
                m_container.resize(new_interval.length());
                m_begin = new_interval.set_begin();
                return *this;
            }
            constexpr data_interval<T, C>& operator&=(const interval<size_t>& other) { return intersect_with(other); }

            constexpr data_interval<T, C> intersected(const interval<size_t>& other) const {
                data_interval<T, C> result = *this;
                result.intersect_with(other);
                return result;
            }
            constexpr data_interval<T, C> operator&(const interval<size_t>& other) { return intersected(other); }


            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> exclusive_union_with(const view_interval<const T>& other) {
                if (empty()) return {other, *this};
                if (other.empty()) return {*this, other};
                if (!intersects(other)) {
                    if (this->strictly_before(other)) return {*this, other};
                    return {other, *this};
                }
                auto [left_interval, right_interval] = set() ^ other.set();
                data_interval<T, C> left{left_interval.set_begin(), {}};
                data_interval<T, C> right{right_interval.set_begin(), {}};
                if (!left_interval.empty()) {
                    left->resize(left_interval.length());
                    if (intersects(left_interval)) {
                        std::move(
                            begin() + (left_interval.set_begin() - set_begin()),
                            begin() + (left_interval.set_end() - set_begin()),
                            left.m_container.begin()
                        );
                    } else {
                        std::copy(
                            other.begin() + (left_interval.set_begin() - other.set_begin()),
                            other.begin() + (left_interval.set_end() - other.set_begin()),
                            left.m_container.begin()
                        );
                    }
                }
                if (!right_interval.empty()) {
                    right->resize(right_interval.length());
                    if (intersects(right_interval)) {
                        std::move(
                            begin() + (right_interval.set_begin() - set_begin()),
                            begin() + (right_interval.set_end() - set_begin()),
                            right.m_container.begin()
                        );
                    } else {
                        std::copy(
                            other.begin() + (right_interval.set_begin() - other.set_begin()),
                            other.begin() + (right_interval.set_end() - other.set_begin()),
                            right.m_container.begin()
                        );
                    }
                }
                return {left, right};
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> exclusive_union_with(data_interval<T, S>&& other) {
                if (empty()) return {other, *this};
                if (other.empty()) return {*this, other};
                if (!intersects(other)) {
                    if (this->strictly_before(other)) return {*this, other};
                    return {other, *this};
                }
                auto [left_interval, right_interval] = set() ^ other.set();
                data_interval<T, C> left{left_interval.set_begin(), {}};
                data_interval<T, C> right{right_interval.set_begin(), {}};
                if (!left_interval.empty()) {
                    left->resize(left_interval.length());
                    if (intersects(left_interval)) {
                        std::move(
                            begin() + (left_interval.set_begin() - set_begin()),
                            begin() + (left_interval.set_end() - set_begin()),
                            left.m_container.begin()
                        );
                    } else {
                        std::move(
                            other.begin() + (left_interval.set_begin() - other.set_begin()),
                            other.begin() + (left_interval.set_end() - other.set_begin()),
                            left.m_container.begin()
                        );
                    }
                }
                if (!right_interval.empty()) {
                    right->resize(right_interval.length());
                    if (intersects(right_interval)) {
                        std::move(
                            begin() + (right_interval.set_begin() - set_begin()),
                            begin() + (right_interval.set_end() - set_begin()),
                            right.m_container.begin()
                        );
                    } else {
                        std::move(
                            other.begin() + (right_interval.set_begin() - other.set_begin()),
                            other.begin() + (right_interval.set_end() - other.set_begin()),
                            right.m_container.begin()
                        );
                    }
                }
                return {left, right};
            }
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> exclusive_unioned(const view_interval<const T>& other) const {
                if (empty()) return {other, *this};
                if (other.empty()) return {*this, other};
                if (!intersects(other)) {
                    if (this->strictly_before(other)) return {*this, other};
                    return {other, *this};
                }
                auto [left_interval, right_interval] = set() ^ other.set();
                data_interval<T, C> left{left_interval.set_begin(), {}};
                data_interval<T, C> right{right_interval.set_begin(), {}};
                if (!left_interval.empty()) {
                    left->resize(left_interval.length());
                    if (intersects(left_interval)) {
                        std::copy(
                            begin() + (left_interval.set_begin() - set_begin()),
                            begin() + (left_interval.set_end() - set_begin()),
                            left.m_container.begin()
                        );
                    } else {
                        std::copy(
                            other.begin() + (left_interval.set_begin() - other.set_begin()),
                            other.begin() + (left_interval.set_end() - other.set_begin()),
                            left.m_container.begin()
                        );
                    }
                }
                if (!right_interval.empty()) {
                    right->resize(right_interval.length());
                    if (intersects(right_interval)) {
                        std::copy(
                            begin() + (right_interval.set_begin() - set_begin()),
                            begin() + (right_interval.set_end() - set_begin()),
                            right.m_container.begin()
                        );
                    } else {
                        std::copy(
                            other.begin() + (right_interval.set_begin() - other.set_begin()),
                            other.begin() + (right_interval.set_end() - other.set_begin()),
                            right.m_container.begin()
                        );
                    }
                }
                return {left, right};
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> exclusive_unioned(data_interval<T, S>&& other) const {
                if (empty()) return {other, *this};
                if (other.empty()) return {*this, other};
                if (!intersects(other)) {
                    if (this->strictly_before(other)) return {*this, other};
                    return {other, *this};
                }
                auto [left_interval, right_interval] = set() ^ other.set();
                data_interval<T, C> left{left_interval.set_begin(), {}};
                data_interval<T, C> right{right_interval.set_begin(), {}};
                if (!left_interval.empty()) {
                    left->resize(left_interval.length());
                    if (intersects(left_interval)) {
                        std::copy(
                            begin() + (left_interval.set_begin() - set_begin()),
                            begin() + (left_interval.set_end() - set_begin()),
                            left.m_container.begin()
                        );
                    } else {
                        std::move(
                            other.begin() + (left_interval.set_begin() - other.set_begin()),
                            other.begin() + (left_interval.set_end() - other.set_begin()),
                            left.m_container.begin()
                        );
                    }
                }
                if (!right_interval.empty()) {
                    right->resize(right_interval.length());
                    if (intersects(right_interval)) {
                        std::copy(
                            begin() + (right_interval.set_begin() - set_begin()),
                            begin() + (right_interval.set_end() - set_begin()),
                            right.m_container.begin()
                        );
                    } else {
                        std::move(
                            other.begin() + (right_interval.set_begin() - other.set_begin()),
                            other.begin() + (right_interval.set_end() - other.set_begin()),
                            right.m_container.begin()
                        );
                    }
                }
                return {left, right};
            }
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator^(const view_interval<const T>& other) const { return exclusive_unioned(other); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator^(data_interval<T, S>&& other) const { return exclusive_unioned(std::move(other)); }


            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> subtract_with(const interval<size_t>& other) {
                interval<size_t> mask = normalise(other);
                if (empty() || mask.empty() || !intersects(mask)) return {*this, {}};
                auto [left_interval, right_interval] = set() - mask;
                data_interval<T, C> left{{}, left_interval.set_begin()};
                data_interval<T, C> right{{}, right_interval.set_begin()};
                if (!left_interval.empty()) {
                    left.m_container.resize(left_interval.length());
                    std::move(
                        container().begin() + (left_interval.set_begin() - set_begin()),
                        container().begin() + (left_interval.set_end() - set_begin()),
                        left.m_container.begin()
                    );
                }
                if (!right_interval.empty()) {
                    right.m_container.resize(right_interval.length());
                    std::move(
                        container().begin() + (right_interval.set_begin() - set_begin()),
                        container().begin() + (right_interval.set_end() - set_begin()),
                        right.m_container.begin()
                    );
                }
                return {left, right};
            }
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> subtracted(const interval<size_t>& other) const {
                interval<size_t> mask = normalise(other);
                if (empty() || mask.empty() || !intersects(mask)) return {*this, {}};
                auto [left_interval, right_interval] = set() - mask;
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
            constexpr std::pair<data_interval<T, C>, data_interval<T, C>> operator-(const interval<size_t>& other) const { return subtracted(other); }
    };


    template <typename T, typename C = std::vector<T>>
    requires std::same_as<T, typename C::value_type>
    class data_interval_union {
        public:
            struct holder {
                private:
                    data_interval<T, C> m_data{};
                    constexpr data_interval<T, C>& data() noexcept { return m_data; }
                    constexpr holder() = default;
                    constexpr holder(data_interval<T, C>&& data) : m_data(data) {}
                    constexpr holder(const data_interval<T, C>& data) : m_data(data) {}
                    friend class data_interval_union<T, C>;
                public:
                    constexpr holder(const holder& other) = default;
                    constexpr holder& operator=(const holder& other) = default;
                    constexpr const data_interval<T, C>& data() const noexcept { return m_data; }
                    constexpr view_interval<T> view() noexcept { return view_interval<T>(m_data); }
                    constexpr view_interval<const T> view() const noexcept { return view_interval<const T>(m_data); }
                    constexpr view_interval<const T> cview() const noexcept { return view_interval<const T>(m_data); }
            };

        private:
            std::vector<holder> m_data;


        public:
            using interval_type = data_interval<T, C>;
            using value_type = holder;
            using iterator = std::vector<holder>::iterator;
            using const_iterator = std::vector<holder>::const_iterator;

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

            constexpr interval_union<size_t> set() const noexcept {
                interval_union<size_t> un;
                for (const auto& inter : *this) {
                    un |= interval<size_t>(inter.data());
                }
                return un;
            }
            constexpr operator interval_union<size_t>() const noexcept { return set(); }

            constexpr bool empty() const noexcept { return m_data.empty(); }
            constexpr size_t set_begin() const noexcept {
                assert(!m_data.empty());
                return m_data.front().data().set_begin();
            }
            constexpr size_t set_end() const noexcept {
                assert(!m_data.empty());
                return m_data.back().data().set_end();
            }
            constexpr bool first_included() const noexcept {
                assert(!m_data.empty());
                return m_data.front().data().first_included();
            }
            constexpr bool last_included() const noexcept {
                assert(!m_data.empty());
                return m_data.back().data().last_included();
            }

            constexpr auto begin() noexcept { return m_data.begin(); }
            constexpr auto begin() const noexcept { return m_data.cbegin(); }
            constexpr auto cbegin() const noexcept { return m_data.cbegin(); }
            constexpr auto rbegin() noexcept { return m_data.rbegin(); }
            constexpr auto rbegin() const noexcept { return m_data.crbegin(); }
            constexpr auto crbegin() const noexcept { return m_data.crbegin(); }

            constexpr auto end() noexcept { return m_data.end(); }
            constexpr auto end() const noexcept { return m_data.cend(); }
            constexpr auto cend() const noexcept { return m_data.cend(); }
            constexpr auto rend() noexcept { return m_data.rend(); }
            constexpr auto rend() const noexcept { return m_data.crend(); }
            constexpr auto crend() const noexcept { return m_data.crend(); }

            constexpr holder& front() noexcept { return m_data.front(); }
            constexpr const holder& front() const noexcept { return m_data.front(); }
            constexpr holder& back() noexcept { return m_data.back(); }
            constexpr const holder& back() const noexcept { return m_data.back(); }

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

            constexpr auto erase(const_iterator it) { return m_data.erase(it); }
            constexpr auto erase(const_iterator begin, const_iterator end) { return m_data.erase(begin, end); }
            constexpr void clear() { m_data.clear(); }

            constexpr data_interval_union<T, C>& operator>>(size_t offset) noexcept {
                assert(offset <= std::numeric_limits<size_t>::max() - end());
                for (auto& el : m_data) el >> offset;
                return *this;
            }
            constexpr data_interval_union<T, C>& operator<<(size_t offset) noexcept {
                assert(offset <= begin());
                for (auto& el : m_data) el << offset;
                return *this;
            }


            template <typename Comb> requires std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C>& merge_with(const view_interval<const T>& inter, Comb&& comb) {
                if (inter.empty()) return *this;
                data_interval<T, C> merged = inter;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->data().mergeable(merged) && !it->data().strictly_after(merged)) it++;
                while (it != m_data.end() && it->data().mergeable(merged)) {
                    it->data().merge_with(merged, std::forward<Comb>(comb));
                    merged = std::move(it->data());
                    it = m_data.erase(it);
                }
                m_data.insert(it, std::move(merged));
                return *this;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C>& merge_with(data_interval<T, S>&& inter, Comb&& comb) {
                if (inter.empty()) return *this;
                data_interval<T, C> merged = std::move(inter);
                auto it = m_data.begin();
                while (it != m_data.end() && !it->data().mergeable(merged) && !it->data().strictly_after(merged)) it++;
                while (it != m_data.end() && it->data().mergeable(merged)) {
                    merged = it->data().merge_with(std::move(merged), std::forward<Comb>(comb));
                    it = m_data.erase(it);
                }
                m_data.insert(it, std::move(merged));
                return *this;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C>& merge_with(const data_interval_union<T, S>& un, Comb&& comb) {
                if (&un == this) return *this;
                for (const auto& inter : *this) {
                    merge_with(view_interval<const T>(inter.data()), std::forward<Comb>(comb));
                }
                return *this;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C>& merge_with(data_interval_union<T, S>&& un, Comb&& comb) {
                if (&un == this) return *this;
                auto it = un.begin();
                while (it != un.end()) {
                    merge_with(std::move(it->data()), std::forward<Comb>(comb));
                    it = un.erase(it);
                }
                return *this;
            }
            constexpr data_interval_union<T, C>& merge_with(const view_interval<const T>& inter) { return merge_with(inter, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& merge_with(data_interval<T, S>&& inter) { return merge_with(std::move(inter), comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& merge_with(const data_interval_union<T, S>& un) { return merge_with(un, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& merge_with(data_interval_union<T, S>&& un) { return merge_with(std::move(un), comb_new<T>); }
            constexpr data_interval_union<T, C>& operator|=(const view_interval<const T>& inter) { return merge_with(inter); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator|=(data_interval<T, S>&& inter) { return merge_with(std::move(inter)); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator|=(const data_interval_union<T, S>& un) { return merge_with(un); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator|=(data_interval_union<T, S>&& un) { return merge_with(std::move(un)); }

            template <typename Comb> requires std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C> merged(const view_interval<const T>& inter, Comb&& comb) const {
                data_interval_union<T, C> result = *this;
                result.merge_with(inter, std::forward<Comb>(comb));
                return result;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C> merged(data_interval<T, S>&& inter, Comb&& comb) const {
                data_interval_union<T, C> result = *this;
                result.merge_with(std::move(inter), std::forward<Comb>(comb));
                return result;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C> merged(const data_interval_union<T, S>& un, Comb&& comb) const {
                if (&un == this) return *this;
                data_interval_union<T, C> result = *this;
                result.merge_with(un, std::forward<Comb>(comb));
                return result;
            }
            template <typename S, typename Comb> requires std::same_as<T, typename S::value_type> && std::is_invocable_r_v<T, Comb, T&, const T&>
            constexpr data_interval_union<T, C> merged(data_interval_union<T, S>&& un, Comb&& comb) const {
                if (&un == this) return *this;
                data_interval_union<T, C> result = *this;
                result.merge_with(std::move(un), std::forward<Comb>(comb));
                return result;
            }
            constexpr data_interval_union<T, C> merged(const view_interval<const T>& inter) const { return merged(inter, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> merged(data_interval<T, S>&& inter) const { return merged(std::move(inter), comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> merged(const data_interval_union<T, S>& un) const { return merged(un, comb_new<T>); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> merged(data_interval_union<T, S>&& un) const { return merged(std::move(un), comb_new<T>); }
            constexpr data_interval_union<T, C> operator|(const view_interval<const T>& inter) const { return merged(inter); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> operator|(data_interval<T, S>&& inter) const { return merged(std::move(inter)); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> operator|(const data_interval_union<T, S>& un) const { return merged(un); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> operator|(data_interval_union<T, S>&& un) const { return merged(std::move(un)); }


            constexpr data_interval_union<T, C>& intersect_with(const interval<size_t>& inter) {
                for (auto it = m_data.begin(); it != m_data.end();) {
                    auto result = it->data() & inter;
                    it = m_data.erase(it);
                    if (!result.empty()) it = std::next(m_data.insert(it, result));
                }
                return *this;
            }
            constexpr data_interval_union<T, C>& intersect_with(const interval_union<size_t>& un) {
                for (const auto& inter : un) intersect_with(inter);
                return *this;
            }
            constexpr data_interval_union<T, C> intersected(const interval<size_t>& inter) {
                data_interval_union<T, C> result = *this;
                result.intersect_with(inter);
                return result;
            }
            constexpr data_interval_union<T, C> intersected(const interval_union<size_t>& un) const {
                data_interval_union<T, C> result = *this;
                result.intersect_with(un);
                return result;
            }
            constexpr data_interval_union<T, C>& operator&=(const interval<size_t>& inter) { return intersect_with(inter); }
            constexpr data_interval_union<T, C>& operator&=(const interval_union<size_t>& un) { return intersect_with(un); }
            constexpr data_interval_union<T, C> operator&(const interval<size_t>& inter) const { return intersected(inter); }
            constexpr data_interval_union<T, C> operator&(const interval_union<size_t>& un) const { return intersected(un); }


            constexpr data_interval_union<T, C>& exclusive_union_with(const view_interval<const T>& inter) {
                if (inter.empty()) return *this;
                data_interval<T, C> remain = inter;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->data().intersects(remain) && !it->data().strictly_after(remain)) it++;
                while (it != m_data.end() && remain.empty()) {
                    auto el = interval<size_t>(*it);
                    auto [left, right] = remain.exclusive_union_with(std::move(it->data()));
                    it = m_data.erase(it);
                    if (!left.empty()) it = m_data.insert(it, left);
                    if (!right.empty() && !right.strictly_after(el)) {
                        it = m_data.insert(it, right);
                        break;
                    }
                    remain = std::move(right);
                }
                m_data.insert(it, std::move(remain));
                return *this;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& exclusive_union_with(data_interval<T, S>&& inter) {
                if (inter.empty()) return *this;
                data_interval<T, C> remain = std::move(inter);
                auto it = m_data.begin();
                while (it != m_data.end() && !it->data().intersects(remain) && !it->data().strictly_after(remain)) it++;
                while (it != m_data.end() && remain.empty()) {
                    auto el = interval<size_t>(*it);
                    auto [left, right] = remain.exclusive_union_with(std::move(it->data()));
                    it = m_data.erase(it);
                    if (!left.empty()) it = m_data.insert(it, left);
                    if (!right.empty() && !right.strictly_after(el)) {
                        it = m_data.insert(it, right);
                        break;
                    }
                    remain = std::move(right);
                }
                m_data.insert(it, std::move(remain));
                return *this;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& exclusive_union_with(const data_interval_union<T, S>& un) {
                if (&un == this) {
                    *this = {};
                    return *this;
                }
                for (const auto& el : un) exclusive_union_with(view_interval<const T>(el));
                return *this;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& exclusive_union_with(data_interval_union<T, S>&& un) {
                if (&un == this) {
                    *this = {};
                    return *this;
                }
                for (auto& el : un) exclusive_union_with(std::move(el));
                return *this;
            }
            constexpr data_interval_union<T, C> exclusive_unioned(const view_interval<const T>& inter) const {
                data_interval_union<T, C> result = *this;
                result.exclusive_union_with(inter);
                return result;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> exclusive_unioned(data_interval<T, S>&& inter) const {
                data_interval_union<T, C> result = *this;
                result.exclusive_union_with(std::move(inter));
                return result;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> exclusive_unioned(const data_interval_union<T, S>& inter) const {
                data_interval_union<T, C> result = *this;
                result.exclusive_union_with(inter);
                return result;
            }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C> exclusive_unioned(data_interval_union<T, S>&& inter) const {
                data_interval_union<T, C> result = *this;
                result.exclusive_union_with(std::move(inter));
                return result;
            }
            constexpr data_interval_union<T, C>& operator^=(const view_interval<const T>& inter) { return exclusive_union_with(inter); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^=(data_interval<T, S>&& inter) { return exclusive_union_with(std::move(inter)); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^=(const data_interval_union<T, S>& un) { return exclusive_union_with(un); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^=(data_interval_union<T, S>&& un) { return exclusive_union_with(std::move(un)); }
            constexpr data_interval_union<T, C>& operator^(const view_interval<const T>& inter) const { return exclusive_unioned(inter); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^(data_interval<T, S>&& inter) const { return exclusive_unioned(std::move(inter)); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^(const data_interval_union<T, S>& un) const { return exclusive_unioned(un); }
            template <typename S> requires std::same_as<T, typename S::value_type>
            constexpr data_interval_union<T, C>& operator^(data_interval_union<T, S>&& un) const { return exclusive_unioned(std::move(un)); }



            constexpr data_interval_union<T, C>& subtract_with(const interval<size_t>& inter) {
                if (inter.empty()) return *this;
                auto it = m_data.begin();
                while (it != m_data.end() && !it->data().intersects(inter) && !it->data().strictly_after(inter)) it++;
                while (it != m_data.end() && it->data().intersects(inter)) {
                    auto [left, right] = it->data().subtracted(inter);
                    it = m_data.erase(it);
                    if (!left.empty()) it = std::next(m_data.insert(it, left));
                    if (!right.empty()) it = std::next(m_data.insert(it, right));
                }
                return *this;
            }
            constexpr data_interval_union<T, C>& subtract_with(const interval_union<size_t>& un) {
                if (&un == this) {
                    *this = {};
                    return *this;
                }
                for (const auto& inter : un) subtract_with(inter);
                return *this;
            }
            constexpr data_interval_union<T, C> subtracted(const interval<size_t>& inter) const {
                data_interval_union<T, C> result = *this;
                result.subtract_with(inter);
                return result;
            }
            constexpr data_interval_union<T, C> subtracted(const interval_union<size_t>& un) const {
                data_interval_union<T, C> result = *this;
                result.subtract_with(un);
                return result;
            }
            constexpr data_interval_union<T, C>& operator-=(const interval<size_t>& inter) { return subtract_with(inter); }
            constexpr data_interval_union<T, C>& operator-=(const interval_union<size_t>& un) { return subtract_with(un); }
            constexpr data_interval_union<T, C> operator-(const interval<size_t>& inter) const { return subtracted(inter); }
            constexpr data_interval_union<T, C> operator-(const interval_union<size_t>& un) const { return subtracted(un); }


            template <std::random_access_iterator It>
            constexpr void copy_subset(const interval_union<size_t>& un, It dst) const {
                if (empty() || un.empty()) return;
                auto src_it = begin();
                auto req_it = un.begin();
                while (src_it != end() && req_it != un.end()) {
                    const auto& src = src_it->data();
                    const auto& req = *req_it;
                    if (src.strictly_before(req)) {
                        src_it++;
                    } else if (req.strictly_before(interval<size_t>(src))) {
                        req_it++;
                    } else {
                        const interval<size_t> intersection = interval<size_t>(src) & req;
                        if (!intersection.empty()) {
                            std::copy(
                                src.begin() + (intersection.set_begin() - src.set_begin()),
                                src.begin() + (intersection.set_end() - src.set_begin()),
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
            constexpr void copy_subset(const interval<size_t>& inter, It dst) const {
                copy_subset(interval_union<size_t>(inter), dst);
            }
    };

}


#endif  // INTERVAL_UNION_HPP_
