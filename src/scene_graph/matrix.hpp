
#ifndef MATRIX_HPP_
#define MATRIX_HPP_


#include <vector>
#include <cassert>


template <typename T>
class matrix {
    private:
        std::vector<T> m_storage;
        size_t m_width;
        size_t m_height;

    public:
        matrix() : m_width(0), m_height(0) {}
        matrix(size_t width, size_t height) : m_storage(width * height), m_width(width), m_height(height) {}

        T* operator[](size_t index) {
            assert(index < m_height);
            return &m_storage[index * m_width];
        }
        const T* operator[](size_t index) const {
            assert(index < m_height);
            return &m_storage[index * m_width];
        }

        void erase_column(size_t index) {
            assert(index < m_width);
            for (size_t i = 0; i < m_height; i++) {
                m_storage.erase(m_storage.begin() + index + i * (m_width - 1));
            }
            m_width--;
        }
        void erase_row(size_t index) {
            assert(index < m_height);
            m_storage.erase(m_storage.begin() + index, m_storage.begin() + index + m_width);
            m_height--;
        }

        void insert_column(size_t index) {
            assert(index <= m_width);
            for (size_t i = 0; i < m_height; i++) {
                m_storage.insert(m_storage.begin() + index + i * (m_width + 1), T{});
            }
            m_width++;
        }
        void insert_row(size_t index) {
            assert(index <= m_height);
            m_storage.insert(m_storage.begin() + index, m_width, T{});
            m_height++;
        }

        T* data() { return m_storage.data(); }
        const T* data() const { return m_storage.data(); }

        bool empty() const { return m_storage.empty(); }

        void reserve(size_t n) { m_storage.reserve(n); }
        void shrink_to_fit() { m_storage.shrink_to_fit(); }

        size_t width() const { return m_width; }
        size_t height() const { return m_height; }

};


#endif  // MATRIX_HPP_
