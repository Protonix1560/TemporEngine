
#ifndef TENSOR_HPP_
#define TENSOR_HPP_

#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <array>
#include <algorithm>
#include <cassert>
#include <span>


// I'm not sure at all if that code works properly, so it is not used
// However I will leave it here in case it's needed in the future

// Now the engine only uses tensor<2>, so I'd better create and use a matrix class
// It will be easier to debug than this mess


template<class It, class Pred>
requires (std::permutable<It> && std::predicate<Pred, std::iter_difference_t<It>>)
inline It remove_if_indexed(It first, It last, Pred p) {
    It dest = first;
    for(auto it = first, i = std::iter_difference_t<It>{}; it != last; it++, i++) {
        if (!p(i)) {
            if (dest != it) {
                *dest = std::move(*it);
            }
            dest++;
        }
    }
    return dest;
}

template <typename T, size_t N>
requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
inline T product(const std::array<T, N>& nums) {
    if constexpr (N > 0) {
        return [nums]<size_t... Is>(std::index_sequence<Is...>) {
            return (nums[Is] * ...);
        }(std::make_index_sequence<N>());
    } else {
        return T{1};
    }
}

template <typename T, size_t N>
requires (std::is_integral_v<T> || std::is_floating_point_v<T>)
inline T product(std::span<const T, N> nums) {
    if constexpr (N > 0) {
        return [nums]<size_t... Is>(std::index_sequence<Is...>) {
            return (nums[Is] * ...);
        }(std::make_index_sequence<N>());
    } else {
        return T{1};
    }
}

template <typename T>
requires (std::is_integral_v<T>)
inline T mathematical_mod(T a, T m) {
    T r = a % m;
    return (r < 0) ? r + m : r;
}

template <typename T>
requires (std::is_integral_v<T>)
T mathematical_floor_division(T a, T b) {
    T q = a / b;
    T r = a % b;
    if (r != 0 && ((a < 0) != (b < 0))) q--;
    return q;
}


template <size_t N>
inline size_t _step_size(const std::array<size_t, N>& sizes) {
    if constexpr (N > 1) {
        return [sizes]<size_t... Is>(std::index_sequence<Is...>) {
            return (sizes[Is] * ...);
        }(std::make_index_sequence<N - 1>());
    } else {
        return 1;
    }
}

template <size_t N>
inline size_t _step_size(std::span<const size_t, N> sizes) {
    if constexpr (N > 1) {
        return [sizes]<size_t... Is>(std::index_sequence<Is...>) {
            return (sizes[Is] * ...);
        }(std::make_index_sequence<N - 1>());
    } else {
        return 1;
    }
}


template <typename T>
concept tensor_element = !std::is_volatile_v<T>;


template <size_t N, tensor_element T, typename Alloc>
requires (N > 0)
class tensor;

template <size_t N, tensor_element T>
class tensor_view;


template <size_t N, tensor_element T>
requires (N > 0)
class tensor_iterator {
    public:

        using value_type = T;

        tensor_iterator() : m_sizes{}, m_ptr(nullptr), m_begin(nullptr) {}

        template <tensor_element U>
        requires std::same_as<U, std::remove_const_t<T>>
        tensor_iterator(const tensor_iterator<N, U>& it) : m_ptr(it.m_ptr), m_begin(it.m_begin) {}

        template <tensor_element U>
        requires std::same_as<U, std::remove_const_t<T>>
        tensor_iterator<N, T>& operator=(const tensor_iterator<N, U>& it) { m_ptr = it.m_ptr; m_begin = it.m_begin; return *this; }

        T& operator[](ptrdiff_t diff) const requires (N == 1) { return *(*this + diff); }
        T& operator*() const requires (N == 1) { return *m_ptr; }
        T* operator->() const requires (N == 1) { return m_ptr; }
        tensor_view<N - 1, T> operator[](ptrdiff_t diff) const requires (N > 1) { return *(*this + diff); }
        tensor_view<N - 1, T> operator*() const requires (N > 1);

        tensor_iterator<N, T>& operator++() { m_ptr += _step_size(m_sizes); return *this; }
        tensor_iterator<N, T>& operator--() { m_ptr -= _step_size(m_sizes); return *this; }

        tensor_iterator<N, T> operator++(int) { tensor_iterator<N, T> old = *this; ++(*this); return old; }
        tensor_iterator<N, T> operator--(int) { tensor_iterator<N, T> old = *this; --(*this); return old; }

        tensor_iterator<N, T> operator+(ptrdiff_t diff) const { return tensor_iterator<N, T>(std::span(m_sizes), m_ptr + diff * _step_size(m_sizes), m_begin); }
        tensor_iterator<N, T> operator-(ptrdiff_t diff) const { return tensor_iterator<N, T>(std::span(m_sizes), m_ptr - diff * _step_size(m_sizes), m_begin); }
        ptrdiff_t operator-(tensor_iterator<N, const T> it) const { return (m_ptr - it.m_ptr) / _step_size(m_sizes); }

        tensor_iterator<N, T>& operator+=(ptrdiff_t diff) { m_ptr += diff * _step_size(m_sizes); return *this; }
        tensor_iterator<N, T>& operator-=(ptrdiff_t diff) { m_ptr -= diff * _step_size(m_sizes); return *this; }

        template <typename U> std::strong_ordering operator<=>(tensor_iterator<N, U> it) const { return m_ptr <=> it.m_ptr; }
        template <typename U> bool operator==(tensor_iterator<N, U> it) const { return m_ptr == it.m_ptr; }
        template <typename U> bool operator!=(tensor_iterator<N, U> it) const { return m_ptr != it.m_ptr; }

    private:
        tensor_iterator(std::span<const size_t, N> sizes, T* ptr, T* begin) : m_ptr(ptr), m_begin(begin) {
            std::ranges::copy(sizes, m_sizes.begin());
        }

        std::array<size_t, N> m_sizes;
        T* m_ptr;
        T* m_begin;

        friend struct tensor_iterator<N, const T>;
        friend struct tensor_iterator<N, std::remove_const_t<T>>;
        template <size_t M, tensor_element, typename> requires (M > 0) friend class tensor;
        friend class tensor_view<N, T>;
};

template <size_t N, typename T>
tensor_iterator<N, T> operator+(ptrdiff_t diff, tensor_iterator<N, T> it) {
    return it + diff;
}


template <size_t N, tensor_element T>
requires (N > 0)
class tensor_view<N, T> {

    public:
        using iterator = tensor_iterator<N, T>;
        using const_iterator = tensor_iterator<N, const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr size_t dimension() const { return N; }
        const std::array<size_t, N>& sizes() const { return m_sizes; }

        iterator begin() { return iterator(std::span<const size_t, N>(m_sizes), mp_data, mp_data); }
        const_iterator cbegin() const { return const_iterator(std::span<const size_t, N>(m_sizes), mp_data, mp_data); }
        const_iterator begin() const { return cbegin(); }

        reverse_iterator rbegin() { return end(); }
        const_reverse_iterator crbegin() const { return cend(); }
        const_reverse_iterator rbegin() const { return crbegin(); }

        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, T> mode_m_begin() {
            return tensor_iterator<M, T>(std::span<const size_t, M>(m_sizes.data(), M), mp_data, mp_data);
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_cbegin() const {
            return tensor_iterator<M, const T>(std::span<const size_t, M>(m_sizes.data(), M), mp_data, mp_data);
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_begin() const { return mode_m_cbegin<M>(); }

        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, T>> mode_m_rbegin() { return mode_m_end<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_crbegin() const { return mode_m_cend<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_rbegin() const { return mode_m_crbegin<M>(); }

        iterator end() { return iterator(std::span<const size_t, N>(m_sizes), mp_data + product(m_sizes), mp_data); }
        const_iterator cend() const { return const_iterator(std::span<const size_t, N>(m_sizes), mp_data + product(m_sizes), mp_data); }
        const_iterator end() const { return cend(); }

        reverse_iterator rend() { return begin(); }
        const_reverse_iterator crend() const { return cbegin(); }
        const_reverse_iterator rend() const { return crend(); }

        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, T> mode_m_end() {
            return tensor_iterator<M, T>(
                std::span<const size_t, M>(m_sizes.data(), M), mp_data + product(m_sizes),
                mp_data + product(m_sizes) - product(std::span<const size_t, M>(m_sizes.data(), M))
            );
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_cend() const {
            return tensor_iterator<M, const T>(
                std::span<const size_t, M>(m_sizes.data(), M), mp_data + product(m_sizes),
                mp_data + product(m_sizes) - product(std::span<const size_t, M>(m_sizes.data(), M))
            );
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_end() const { return mode_m_cend<M>(); }

        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, T>> mode_m_rend() { return mode_m_begin<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_crend() const { return mode_m_cbegin<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_rend() const { return mode_m_crend<M>(); }

        tensor_view<N - 1, T> operator[](size_t index) requires (N > 1) {
            assert(index < m_sizes.back());
            return tensor_view<N - 1, T>(std::span<const size_t, N - 1>(m_sizes.data(), N - 1), mp_data + _step_size(m_sizes) * index);
        }

        T& operator[](size_t index) requires (N == 1) {
            assert(index < m_sizes.back());
            return *(this->mp_data + index);
        }

        tensor_view<N - 1, T> front() requires (N > 1) { assert(m_sizes.back() > 0); return (*this)[0]; }
        T& front() requires (N == 1) { assert(m_sizes.back() > 0); return (*this)[0]; }

        tensor_view<N - 1, T> back() requires (N > 1) { assert(m_sizes.back() > 0); return (*this)[m_sizes.back() - 1]; }
        T& back() requires (N == 1) { assert(m_sizes.back() > 0); return (*this)[m_sizes.back() - 1]; }

        friend void swap(tensor_view<N, T> a, tensor_view<N, T> b) {
            assert(a.m_sizes == b.m_sizes);
            size_t p = product(a.m_sizes);
            for (size_t i = 0; i < p; i++) {
                std::swap(a.mp_data[i], b.mp_data[i]);
            }
        }

    private:
        tensor_view(std::span<const size_t, N> sizes, T* p_data) : mp_data(p_data) {
            std::ranges::copy(sizes, m_sizes.begin());
        }
        std::array<size_t, N> m_sizes;
        T* mp_data;

        template <size_t M, tensor_element, typename> requires (M > 0) friend class tensor;
        friend class tensor_iterator<N + 1, T>;
};


template <size_t N, tensor_element T>
requires (N > 0)
tensor_view<N - 1, T> tensor_iterator<N, T>::operator*() const requires (N > 1) {
    return tensor_view<N - 1, T>(std::span<const size_t, N - 1>(m_sizes.data(), N - 1), m_ptr);
}


template <size_t N, tensor_element T, typename Alloc = std::allocator<T>>
requires (N > 0)
class tensor {

    private:
        std::vector<T, Alloc> m_storage;
        std::array<size_t, N> m_sizes;

    public:
        using iterator = tensor_iterator<N, T>;
        using const_iterator = tensor_iterator<N, const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        tensor() : m_sizes() {}

        template <typename... Sizes>
        requires (sizeof...(Sizes) == N)
        tensor(Sizes... sizes) {
            [this, sizes...]<size_t... Is>(std::index_sequence<Is...>) {
                ((m_sizes[Is] = sizes), ...);
            }(std::make_index_sequence<N>());
            m_storage.resize(product(m_sizes));
        }

        tensor(std::initializer_list<size_t> sizes) {
            assert(sizes.size() == N);
            std::ranges::copy(sizes, m_sizes.begin());
            m_storage.resize(product(m_sizes));
        }

        template <typename It>
        requires std::input_iterator<It>
        tensor(It begin, It end) {
            for (auto it = begin, i = 0; i < N; it++, i++) {
                m_sizes[i] = *it;
            }
            m_storage.resize(product(m_sizes));
        }

        template <typename... Sizes>
        requires (sizeof...(Sizes) == N)
        void resize(Sizes... sizes) {
            [this, sizes...]<size_t... Is>(std::index_sequence<Is...>) {
                ([this, size = std::get<Is>(std::make_tuple(sizes...))](){
                    if (m_sizes[Is] < size) {
                        size_t b = size - m_sizes[Is];
                        for (size_t i = 0; i < b; i++) push_back<Is + 1>();
                    } else if (m_sizes[Is] > size) {
                        size_t b = m_sizes[Is] - size;
                        for (size_t i = 0; i < b; i++) erase_back<Is + 1>();
                    }
                }(), ...);
            }(std::make_index_sequence<N>());
        }

        template <typename U, typename... Sizes>
        requires (sizeof...(Sizes) == N && std::is_convertible_v<U, T>)
        void assign(const U& value, Sizes... sizes) {
            resize(sizes...);
            std::fill(m_storage.begin(), m_storage.end(), value);
        }

        template <typename U, typename... Sizes>
        requires (sizeof...(Sizes) == N && std::is_convertible_v<U, T>)
        void assign_new(const U& value, Sizes... sizes) {
            [this, value, sizes...]<size_t... Is>(std::index_sequence<Is...>) {
                ([this, value, size = std::get<Is>(std::make_tuple(sizes...))](){
                    if (m_sizes[Is] < size) {
                        size_t b = size - m_sizes[Is];
                        for (size_t i = 0; i < b; i++) push_back<Is + 1>(value);
                    } else if (m_sizes[Is] > size) {
                        size_t b = m_sizes[Is] - size;
                        for (size_t i = 0; i < b; i++) erase_back<Is + 1>();
                    }
                }(), ...);
            }(std::make_index_sequence<N>());
        }

        tensor_view<N - 1, T> operator[](size_t index) requires (N > 1) {
            assert(index < m_sizes.back());
            return tensor_view<N - 1, T>(std::span<const size_t, N - 1>(m_sizes.data(), N - 1), m_storage.data() + _step_size(m_sizes) * index);
        }

        T& operator[](size_t index) requires (N == 1) {
            assert(index < this->m_sizes.back());
            return this->m_storage[index];
        }

        tensor_view<N - 1, T> front() requires (N > 1) { assert(m_sizes.back() > 0); return (*this)[0]; }
        T& front() requires (N == 1) { assert(m_sizes.back() > 0); return (*this)[0]; }

        tensor_view<N - 1, T> back() requires (N > 1) { assert(m_sizes.back() > 0); return (*this)[m_sizes.back() - 1]; }
        T& back() requires (N == 1) { assert(m_sizes.back() > 0); return (*this)[m_sizes.back() - 1]; }

        iterator begin() { return iterator(std::span<const size_t, N>(m_sizes), m_storage.data(), m_storage.data()); }
        const_iterator cbegin() const { return const_iterator(std::span<const size_t, N>(m_sizes), m_storage.data(), m_storage.data()); }
        const_iterator begin() const { return cbegin(); }

        reverse_iterator rbegin() { return end(); }
        const_reverse_iterator crbegin() const { return cend(); }
        const_reverse_iterator rbegin() const { return crbegin(); }

        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, T> mode_m_begin() {
            return tensor_iterator<M, T>(std::span<const size_t, M>(m_sizes.data(), M), m_storage.data(), m_storage.data());
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_cbegin() const {
            return tensor_iterator<M, const T>(std::span<const size_t, M>(m_sizes.data(), M), m_storage.data(), m_storage.data());
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_begin() const { return mode_m_cbegin<M>(); }

        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, T>> mode_m_rbegin() { return mode_m_end<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_crbegin() const { return mode_m_cend<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_rbegin() const { return mode_m_crbegin<M>(); }

        iterator end() { return iterator(std::span<const size_t, N>(m_sizes), m_storage.data() + product(m_sizes), m_storage.data()); }
        const_iterator cend() const { return const_iterator(std::span<const size_t, N>(m_sizes), m_storage.data() + product(m_sizes), m_storage.data()); }
        const_iterator end() const { return cend(); }

        reverse_iterator rend() { return begin(); }
        const_reverse_iterator crend() const { return cbegin(); }
        const_reverse_iterator rend() const { return crend(); }

        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, T> mode_m_end() {
            return tensor_iterator<M, T>(
                std::span<const size_t, M>(m_sizes.data(), M), m_storage.data() + product(m_sizes),
                m_storage.data() + product(m_sizes) - product(std::span<const size_t, M>(m_sizes.data(), M))
            );
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_cend() const {
            return tensor_iterator<M, const T>(
                std::span<const size_t, M>(m_sizes.data(), M), m_storage.data() + product(m_sizes),
                m_storage.data() + product(m_sizes) - product(std::span<const size_t, M>(m_sizes.data(), M))
            );
        }
        template <size_t M> requires (M <= N && M > 0)
        tensor_iterator<M, const T> mode_m_end() const { return mode_m_cend<M>(); }

        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, T>> mode_m_rend() { return mode_m_begin<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_crend() const { return mode_m_cbegin<M>(); }
        template <size_t M> requires (M <= N && M > 0)
        std::reverse_iterator<tensor_iterator<M, const T>> mode_m_rend() const { return mode_m_crend<M>(); }

        template <size_t M>
        requires (M <= N && M > 0)
        inline void erase_hyperplane(tensor_iterator<M, T> pos) {
            ptrdiff_t offset = pos.m_ptr - m_storage.data();
            ptrdiff_t step = _step_size(std::span<const size_t, M>(m_sizes.data(), M));
            ptrdiff_t mode_m_size = m_sizes[M - 1];
            m_storage.erase(remove_if_indexed(m_storage.begin(), m_storage.end(), [offset, step, mode_m_size](ptrdiff_t i) {
                return mathematical_mod(mathematical_floor_division(i - offset, step), mode_m_size) == 0;
            }), m_storage.end());
            m_sizes[M - 1]--;
        }

        template <size_t M = N> requires (M <= N && M > 0)
        tensor_iterator<M, T> erase_front() { erase_hyperplane(mode_m_begin<M>()); return mode_m_begin<M>(); }

        template <size_t M = N> requires (M <= N && M > 0)
        tensor_iterator<M, T> erase_back() { erase_hyperplane(--mode_m_end<M>()); return mode_m_end<M>(); }

        template <size_t M = N> requires (M <= N && M > 0)
        void pop_front() { erase_hyperplane(mode_m_begin<M>()); }

        template <size_t M = N> requires (M <= N && M > 0)
        void pop_back() { erase_hyperplane(--mode_m_end<M>()); }

        template <size_t M>
        requires (M > 0)
        inline void insert_hyperplane(tensor_iterator<M, T> pos) {
            ptrdiff_t step = product(std::span<const size_t, M>(m_sizes.data(), M));
            ptrdiff_t batch_size = product(std::span<const size_t, M - 1>(m_sizes.data() + (N - M), M - 1));
            ptrdiff_t old_size = m_storage.size();

            ptrdiff_t add = 0;
            if constexpr (M > 1) {
                add = [this]<size_t... Is>(std::index_sequence<Is...>) {
                    return (m_sizes[Is + (Is >= M - 1 ? 1 : 0)] * ...);
                }(std::make_index_sequence<M - 1>());
            } else {
                add = 1;
            }

            m_storage.resize(old_size + add);
            if (step != 0) {
                for (ptrdiff_t i = old_size - 1; i >= 0; i--) {
                    ptrdiff_t div = mathematical_floor_division(i, step);
                    ptrdiff_t batch_pos = step * div + (pos.m_ptr - pos.m_begin);
                    ptrdiff_t new_i = static_cast<ptrdiff_t>(i >= batch_pos) * batch_size + div + i;
                    m_storage[new_i] = std::move(m_storage[i]);
                }
            }
            m_sizes[M - 1]++;
        }

        template <size_t M, typename U>
        requires (M <= N && M > 0 && std::is_convertible_v<U, T>)
        void insert_hyperplane(tensor_iterator<M, T> pos, const U& value) {
            insert_hyperplane(pos);
            ptrdiff_t step = product(std::span<const size_t, M>(m_sizes.data(), M));
            ptrdiff_t batch_size = product(std::span<const size_t, M - 1>(m_sizes.data() + (N - M), M - 1));
            for (ptrdiff_t i = 0; i < m_storage.size() / step; i++) {
                for (ptrdiff_t j = 0; j < batch_size; j++) {
                    ptrdiff_t k = j + i * step + (pos.m_ptr - pos.m_begin);
                    m_storage[k] = value;
                }
            }
        }

        template <size_t M, typename... Args>
        requires (M <= N && M > 0 && std::is_constructible_v<T, Args...>)
        void emplace_hyperplane(tensor_iterator<M, T> pos, Args&&... args) {
            insert_hyperplane(pos);
            ptrdiff_t step = product(std::span<const size_t, M>(m_sizes.data(), M));
            ptrdiff_t batch_size = product(std::span<const size_t, M - 1>(m_sizes.data() + (N - M), M - 1));
            for (ptrdiff_t i = 0; i < m_storage.size() / step; i++) {
                for (ptrdiff_t j = 0; j < batch_size; j++) {
                    ptrdiff_t k = j + i * step + (pos.m_ptr - pos.m_begin);
                    m_storage[k] = T{std::forward(args)...};
                }
            }
        }

        template <size_t M = N> requires (M <= N && M > 0)
        void push_front() { insert_hyperplane(mode_m_begin<M>()); }
        template <size_t M = N, typename U> requires (M <= N && M > 0 && std::is_convertible_v<U, T>)
        void push_front(const U& value) { insert_hyperplane(mode_m_begin<M>(), value); }
        template <size_t M = N, typename... Args> requires (M <= N && M > 0 && std::is_constructible_v<T, Args...>)
        void emplace_front(Args&&... args) { insert_hyperplane(mode_m_begin<M>(), std::forward(args)...); return *mode_m_begin<M>(); }

        template <size_t M = N> requires (M <= N && M > 0)
        void push_back() { insert_hyperplane(mode_m_end<M>()); }
        template <size_t M = N, typename U> requires (M <= N && M > 0 && std::is_convertible_v<U, T>)
        void push_back(const U& value) { insert_hyperplane(mode_m_end<M>(), value); }
        template <size_t M = N, typename... Args> requires (M <= N && M > 0 && std::is_constructible_v<T, Args...>)
        void emplace_back(Args&&... args) { insert_hyperplane(mode_m_end<M>(), std::forward(args)...); }

        T* data() { return m_storage.data(); }
        constexpr size_t dimension() const { return N; }
        const std::array<size_t, N>& sizes() const { return m_sizes; }
        size_t capacity() const { return m_storage.capacity(); }
        bool empty() const { return m_storage.empty(); }
        void reserve(size_t n) { m_storage.reserve(n); }
        void shrink_to_fit() { m_storage.shrink_to_fit(); }
};


template <tensor_element T, typename Alloc = std::allocator<T>, typename... Sizes>
inline tensor<sizeof...(Sizes), T, Alloc> make_tensor(Sizes... sizes) {
    return tensor<sizeof...(Sizes), T, Alloc>(sizes...);
}


#endif  // TENSOR_HPP_
