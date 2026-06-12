
#ifndef SCENE_GRAPH_SPARSE_SET_HPP_
#define SCENE_GRAPH_SPARSE_SET_HPP_

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>
#include <type_traits>
#include <vector>
#include <cstdint>



template <typename SizeT, typename T>
requires std::is_integral_v<SizeT>
class basic_sparse_set {
    
    private:
        std::vector<SizeT> m_sparse;
        std::vector<T> m_dense;
        std::vector<SizeT> m_indices;

    public:
        static constexpr SizeT null_offset = std::numeric_limits<SizeT>::max();

        basic_sparse_set() = default;

        basic_sparse_set(SizeT n) : m_sparse(n), m_dense(n), m_indices(n) {
            for (SizeT i = 0; i < n; i++) {
                m_sparse[i] = i;
                m_indices[i] = i;
            }
        }

        template <typename It>
        requires (std::input_iterator<It> && !std::random_access_iterator<It>)
        basic_sparse_set(It first, It last) {
            for (auto it = first, i = SizeT{}; it != last; it++, i++) {
                m_dense.push_back(*it);
                m_sparse.push_back(i);
                m_indices.push_back(i);
            }
        }

        template <typename It>
        requires (std::random_access_iterator<It>)
        basic_sparse_set(It first, It last) : m_dense(first, last), m_sparse(last - first), m_indices(last - first) {
            for (SizeT i = 0; i < (last - first); i++) {
                m_sparse[i] = i;
                m_indices[i] = i;
            }
        }

        template <typename... Args>
        requires std::is_constructible_v<T, Args...>
        SizeT emplace(Args&&... args) {
            m_dense.emplace_back(std::forward(args)...);
            SizeT sparse_size = m_sparse.size();
            SizeT indices_size = m_indices.size();
            m_sparse.push_back(indices_size);
            m_indices.push_back(sparse_size);
            return sparse_size;
        }

        SizeT insert(const T& value) {
            m_dense.push_back(value);
            SizeT sparse_size = m_sparse.size();
            SizeT indices_size = m_indices.size();
            m_sparse.push_back(indices_size);
            m_indices.push_back(sparse_size);
            return sparse_size;
        }

        SizeT index(SizeT offset) {
            assert(offset < m_indices.size());
            return m_indices[offset];
        }

        SizeT offset(SizeT index) {
            assert(index < m_sparse.size());
            return m_sparse[index];
        }

        void reserve(SizeT n) {
            m_dense.reserve(n);
            m_indices.reserve(n);
            if (n > m_indices.size()) {
                m_sparse.reserve(n - m_indices.size() + m_sparse.size());
            }
        }

        SizeT index_erase(SizeT index) {
            assert(index < m_sparse.size());
            SizeT offset = m_sparse[index];
            if (offset != m_indices.size() - 1) {
                m_dense[offset] = std::move(m_dense.back());
                m_indices[offset] = m_indices.back();
            }
            m_sparse[index] = null_offset;
            m_dense.pop_back();
            m_indices.pop_back();
            return offset;
        }

        SizeT offset_erase(SizeT offset) {
            assert(offset < m_indices.size());
            SizeT index = m_indices[offset];
            if (offset != m_indices.size() - 1) {
                m_dense[offset] = std::move(m_dense.back());
                m_indices[offset] = m_indices.back();
            }
            m_sparse[index] = null_offset;
            m_dense.pop_back();
            m_indices.pop_back();
            return index;
        }

        SizeT dense_size() { return m_dense.size(); }
        SizeT sparse_size() { return m_sparse.size(); }

        T& operator[](SizeT offset) {
            assert(offset < m_dense.size());
            return m_dense[offset];
        }

        std::vector<T>::iterator begin() { return m_dense.begin(); }
        std::vector<T>::const_iterator cbegin() const { return m_dense.cbegin(); }
        std::vector<T>::const_iterator begin() const { return cbegin(); }
        std::vector<T>::reverse_iterator rbegin() { return m_dense.rbegin(); }
        std::vector<T>::const_reverse_iterator crbegin() const { return m_dense.crbegin(); }
        std::vector<T>::const_reverse_iterator rbegin() const { return crbegin(); }

        std::vector<T>::iterator end() { return m_dense.end(); }
        std::vector<T>::const_iterator cend() const { return m_dense.cend(); }
        std::vector<T>::const_iterator end() const { return cend(); }
        std::vector<T>::reverse_iterator rend() { return m_dense.rend(); }
        std::vector<T>::const_reverse_iterator crend() const { return m_dense.crend(); }
        std::vector<T>::const_reverse_iterator rend() const { return crend(); }

        template <typename SizeU>
        requires std::is_integral_v<SizeU>
        bool operator==(const basic_sparse_set<SizeU, T>& other) {
            return (m_dense.size() == other.dense_size()) && std::equal(other.begin(), other.end(), m_dense.begin());
        }

        template <typename SizeU>
        requires std::is_integral_v<SizeU>
        bool operator!=(const basic_sparse_set<SizeU, T>& other) {
            return (m_dense.size() != other.dense_size()) || !std::equal(other.begin(), other.end(), m_dense.begin());
        }

};


template <typename T>
using sparse_set = basic_sparse_set<uint32_t, T>;


#endif  // SCENE_GRAPH_SPARSE_SET_HPP_
