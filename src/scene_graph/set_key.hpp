
#ifndef SCENE_GRAPH_IMMUTABLE_SOA_SET_HPP_
#define SCENE_GRAPH_IMMUTABLE_SOA_SET_HPP_


#include <cstddef>
#include <cstring>
#include <type_traits>
#include <set>
#include "hash.hpp"



template <typename T>
class set_key {
    private:
        std::set<T> m_set;

    public:
        set_key() = default;

        template <typename It>
        set_key(It first, It last) : m_set(first, last) {}

        template <typename U>
        requires std::is_convertible_v<U, T>
        set_key(const set_key<U>& other) {
            for (const auto& el : other) {
                m_set.insert(el);
            }
        }

        void insert(const T& value) {
            m_set.insert(value);
        }

        template <typename... Args>
        void emplace(Args&&... args) { m_set.emplace(std::forward(args)...); }

        template <typename U>
        requires requires(const U& u, const T& t) {
            t == u;
        }
        void erase(const U& value) {
            for (auto it = m_set.begin(); it != m_set.end();) {
                if (*it == value) {
                    it = m_set.erase(it);
                } else {
                    it++;
                }
            }
        }

        size_t size() const { return m_set.size(); }
        bool empty() const { return m_set.empty(); }

        bool operator==(const set_key<T>& other) const { return m_set == other.m_set; }
        bool operator!=(const set_key<T>& other) const { return m_set != other.m_set; }

        std::set<T>::iterator begin() { return m_set.begin(); }
        std::set<T>::const_iterator cbegin() const { return m_set.cbegin(); }
        std::set<T>::const_iterator begin() const { return m_set.cbegin(); }

        std::set<T>::iterator end() { return m_set.end(); }
        std::set<T>::const_iterator cend() const { return m_set.cend(); }
        std::set<T>::const_iterator end() const { return m_set.cend(); }
};

template <typename T>
struct std::hash<set_key<T>> {
    size_t operator()(const set_key<T>& set) const {
        return sequence_hash(set.begin(), set.end());
    }
};


#endif  // SCENE_GRAPH_IMMUTABLE_SOA_SET_HPP_
