
#ifndef I_GRAPHICS_DEVICE_BACKENDS_VULKAN_LINALG_PACKED_HPP_
#define I_GRAPHICS_DEVICE_BACKENDS_VULKAN_LINALG_PACKED_HPP_

#include <cstddef>

#include <glm/glm.hpp>


template <size_t N, typename T>
struct packed_vec;

template <typename T>
struct packed_vec<2, T> {
    private:
        T m_data[2]{};
    public:
        using value_type = T;
        static constexpr size_t size = 2;

        constexpr packed_vec() noexcept = default;
        constexpr packed_vec(T x, T y) noexcept : m_data{x, y} {}
        constexpr packed_vec(const glm::vec<2, T>& v) : m_data{v[0], v[1]} {}

        constexpr packed_vec& operator=(const packed_vec& other) noexcept = default;
        constexpr packed_vec& operator=(const glm::vec<2, T>& v) { m_data = {v[0], v[1]}; return *this; }
        
        constexpr T& x() noexcept { return m_data[0]; }
        constexpr T& y() noexcept { return m_data[1]; }
        constexpr const T& x() const noexcept { return m_data[0]; }
        constexpr const T& y() const noexcept { return m_data[1]; }
        template <size_t I> requires (I < 2) constexpr T& component() noexcept { return m_data[I]; }
        template <size_t I> requires (I < 2) constexpr const T& component() const noexcept { return m_data[I]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i) noexcept { assert(i < 2); return m_data[i]; }
        constexpr const T& component(size_t i) const noexcept { assert(i < 2); return m_data[i]; }
        constexpr T& operator[](size_t i) noexcept { assert(i < 2); return m_data[i]; }
        constexpr const T& operator[](size_t i) const noexcept { assert(i < 2); return m_data[i]; }
};

template <typename T>
struct packed_vec<3, T> {
    private:
        T m_data[3]{};
    public:
        using value_type = T;
        static constexpr size_t size = 3;

        constexpr packed_vec() noexcept = default;
        constexpr packed_vec(T x, T y, T z) noexcept : m_data{x, y, z} {}
        constexpr packed_vec(const glm::vec<3, T>& v) : m_data{v[0], v[1], v[2]} {}

        constexpr packed_vec& operator=(const packed_vec& other) noexcept = default;
        constexpr packed_vec& operator=(const glm::vec<3, T>& v) { m_data = {v[0], v[1], v[2]}; return *this; }
        
        constexpr T& x() noexcept { return m_data[0]; }
        constexpr T& y() noexcept { return m_data[1]; }
        constexpr T& z() noexcept { return m_data[2]; }
        constexpr const T& x() const noexcept { return m_data[0]; }
        constexpr const T& y() const noexcept { return m_data[1]; }
        constexpr const T& z() const noexcept { return m_data[2]; }
        template <size_t I> requires (I < 3) constexpr T& component() noexcept { return m_data[I]; }
        template <size_t I> requires (I < 3) constexpr const T& component() const noexcept { return m_data[I]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i) noexcept { assert(i < 3); return m_data[i]; }
        constexpr const T& component(size_t i) const noexcept { assert(i < 3); return m_data[i]; }
        constexpr T& operator[](size_t i) noexcept { assert(i < 3); return m_data[i]; }
        constexpr const T& operator[](size_t i) const noexcept { assert(i < 3); return m_data[i]; }
};

template <typename T>
struct packed_vec<4, T> {
    private:
        T m_data[4]{};
    public:
        using value_type = T;
        static constexpr size_t size = 4;

        constexpr packed_vec() noexcept = default;
        constexpr packed_vec(T x, T y, T z, T w) noexcept : m_data{x, y, z, w} {}
        constexpr packed_vec(const glm::vec<4, T>& v) : m_data{v[0], v[1], v[2], v[3]} {}

        constexpr packed_vec& operator=(const packed_vec& other) noexcept = default;
        constexpr packed_vec& operator=(const glm::vec<4, T>& v) { m_data = {v[0], v[1], v[2], v[3]}; return *this; }
        
        constexpr T& x() noexcept { return m_data[0]; }
        constexpr T& y() noexcept { return m_data[1]; }
        constexpr T& z() noexcept { return m_data[2]; }
        constexpr T& w() noexcept { return m_data[3]; }
        constexpr const T& x() const noexcept { return m_data[0]; }
        constexpr const T& y() const noexcept { return m_data[1]; }
        constexpr const T& z() const noexcept { return m_data[2]; }
        constexpr const T& w() const noexcept { return m_data[3]; }
        template <size_t I> requires (I < 4) constexpr T& component() noexcept { return m_data[I]; }
        template <size_t I> requires (I < 4) constexpr const T& component() const noexcept { return m_data[I]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i) noexcept { assert(i < 4); return m_data[i]; }
        constexpr const T& component(size_t i) const noexcept { assert(i < 4); return m_data[i]; }
        constexpr T& operator[](size_t i) noexcept { assert(i < 4); return m_data[i]; }
        constexpr const T& operator[](size_t i) const noexcept { assert(i < 4); return m_data[i]; }
};


template <size_t N, size_t M, typename T>
struct packed_mat;

template <typename T>
struct packed_mat<2, 2, T> {
    private:
        T m_data[4]{};
    public:
        using value_type = T;
        static constexpr size_t columns = 2;
        static constexpr size_t rows = 2;
        static constexpr size_t size = 4;

        constexpr packed_mat() noexcept = default;
        constexpr packed_mat(T x0, T y0, T x1, T y1) noexcept
            : m_data{x0, y0, x1, y1} {}
        constexpr packed_mat(const packed_vec<2, T>& c0, const packed_vec<2, T>& c1) noexcept
            : m_data{c0.x, c0.y, c1.x, c1.y} {}
        constexpr packed_mat(const glm::mat<2, 2, T>& m)
            : m_data{m[0][0], m[0][1], m[1][0], m[1][1]} {}

        constexpr packed_mat& operator=(const packed_mat& other) noexcept = default;
        constexpr packed_mat& operator=(const glm::mat<2, 2, T>& m) {
            m_data[0] = m[0][0];
            m_data[1] = m[0][1];
            m_data[2] = m[1][0];
            m_data[3] = m[1][1];
            return *this;
        }
        
        constexpr T& x0() noexcept { return m_data[0]; }
        constexpr T& y0() noexcept { return m_data[1]; }
        constexpr T& x1() noexcept { return m_data[2]; }
        constexpr T& y1() noexcept { return m_data[3]; }
        constexpr const T& x0() const noexcept { return m_data[0]; }
        constexpr const T& y0() const noexcept { return m_data[1]; }
        constexpr const T& x1() const noexcept { return m_data[2]; }
        constexpr const T& y1() const noexcept { return m_data[3]; }
        template <size_t I, size_t J> requires (I < 2 && J < 2) constexpr T& component() noexcept { return m_data[I * rows + J]; }
        template <size_t I, size_t J> requires (I < 2 && J < 2) constexpr const T& component() const noexcept { return m_data[I * rows + J]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i, size_t j) noexcept { assert(i < 2 && j < 2); return m_data[i * rows + j]; }
        constexpr const T& component(size_t i, size_t j) const noexcept { assert(i < 2 && j < 2); return m_data[i * rows + j]; }
};

template <typename T>
struct packed_mat<3, 3, T> {
    private:
        T m_data[9]{};
    public:
        using value_type = T;
        static constexpr size_t columns = 3;
        static constexpr size_t rows = 3;
        static constexpr size_t size = 9;

        constexpr packed_mat() noexcept = default;
        constexpr packed_mat(T x0, T y0, T z0, T x1, T y1, T z1, T x2, T y2, T z2) noexcept
            : m_data{x0, y0, z0, x1, y1, z1, x2, y2, z2} {}
        constexpr packed_mat(const packed_vec<3, T>& c0, const packed_vec<3, T>& c1, const packed_vec<3, T>& c2) noexcept
            : m_data{c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z} {}
        constexpr packed_mat(const glm::mat<3, 3, T>& m)
            : m_data{m[0], m[1], m[2]} {}

        constexpr packed_mat& operator=(const packed_mat& other) noexcept = default;
        constexpr packed_mat& operator=(const glm::mat<3, 3, T>& m) {
            m_data[0] = m[0][0];
            m_data[1] = m[0][1];
            m_data[2] = m[0][2];
            m_data[3] = m[1][0];
            m_data[4] = m[1][1];
            m_data[5] = m[1][2];
            m_data[6] = m[2][0];
            m_data[7] = m[2][1];
            m_data[8] = m[2][2];
            return *this;
        }
        
        constexpr T& x0() noexcept { return m_data[0]; }
        constexpr T& y0() noexcept { return m_data[1]; }
        constexpr T& z0() noexcept { return m_data[2]; }
        constexpr T& x1() noexcept { return m_data[3]; }
        constexpr T& y1() noexcept { return m_data[4]; }
        constexpr T& z1() noexcept { return m_data[5]; }
        constexpr T& x2() noexcept { return m_data[6]; }
        constexpr T& y2() noexcept { return m_data[7]; }
        constexpr T& z2() noexcept { return m_data[8]; }
        constexpr const T& x0() const noexcept { return m_data[0]; }
        constexpr const T& y0() const noexcept { return m_data[1]; }
        constexpr const T& z0() const noexcept { return m_data[2]; }
        constexpr const T& x1() const noexcept { return m_data[3]; }
        constexpr const T& y1() const noexcept { return m_data[4]; }
        constexpr const T& z1() const noexcept { return m_data[5]; }
        constexpr const T& x2() const noexcept { return m_data[6]; }
        constexpr const T& y2() const noexcept { return m_data[7]; }
        constexpr const T& z2() const noexcept { return m_data[8]; }
        template <size_t I, size_t J> requires (I < 3 && J < 3) constexpr T& component() noexcept { return m_data[I * rows + J]; }
        template <size_t I, size_t J> requires (I < 3 && J < 3) constexpr const T& component() const noexcept { return m_data[I * rows + J]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i, size_t j) noexcept { assert(i < 3 && j < 3); return m_data[i * rows + j]; }
        constexpr const T& component(size_t i, size_t j) const noexcept { assert(i < 3 && j < 3); return m_data[i * rows + j]; }
};

template <typename T>
struct packed_mat<4, 4, T> {
    private:
        T m_data[16]{};
    public:
        using value_type = T;
        static constexpr size_t columns = 4;
        static constexpr size_t rows = 4;
        static constexpr size_t size = 16;

        constexpr packed_mat() noexcept = default;
        constexpr packed_mat(T x0, T y0, T z0, T w0, T x1, T y1, T z1, T w1, T x2, T y2, T z2, T w2, T x3, T y3, T z3, T w3) noexcept
            : m_data{x0, y0, z0, w0, x1, y1, z1, w1, x2, y2, z2, w2, x3, y3, z3, w3} {}
        constexpr packed_mat(const packed_vec<4, T>& c0, const packed_vec<4, T>& c1, const packed_vec<4, T>& c2, const packed_vec<4, T>& c3) noexcept
            : m_data{c0.x, c0.y, c0.z, c0.w, c1.x, c1.y, c1.z, c1.w, c2.x, c2.y, c2.z, c2.w, c3.x, c3.y, c3.z, c3.w} {}
        constexpr packed_mat(const glm::mat<4, 4, T>& m)
            : m_data{m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3], m[3][0], m[3][1], m[3][2], m[3][3]} {}

        constexpr packed_mat& operator=(const packed_mat& other) noexcept = default;
        constexpr packed_mat& operator=(const glm::mat<4, 4, T>& m) {
            m_data[0] = m[0][0];
            m_data[1] = m[0][1];
            m_data[2] = m[0][2];
            m_data[3] = m[0][3];
            m_data[4] = m[1][0];
            m_data[5] = m[1][1];
            m_data[6] = m[1][2];
            m_data[7] = m[1][3];
            m_data[8] = m[2][0];
            m_data[9] = m[2][1];
            m_data[10] = m[2][2];
            m_data[11] = m[2][3];
            m_data[12] = m[3][0];
            m_data[13] = m[3][1];
            m_data[14] = m[3][2];
            m_data[15] = m[3][3];
            return *this;
        }
        
        constexpr T& x0() noexcept { return m_data[0]; }
        constexpr T& y0() noexcept { return m_data[1]; }
        constexpr T& z0() noexcept { return m_data[2]; }
        constexpr T& w0() noexcept { return m_data[3]; }
        constexpr T& x1() noexcept { return m_data[4]; }
        constexpr T& y1() noexcept { return m_data[5]; }
        constexpr T& z1() noexcept { return m_data[6]; }
        constexpr T& w1() noexcept { return m_data[7]; }
        constexpr T& x2() noexcept { return m_data[8]; }
        constexpr T& y2() noexcept { return m_data[9]; }
        constexpr T& z2() noexcept { return m_data[10]; }
        constexpr T& w2() noexcept { return m_data[11]; }
        constexpr T& x3() noexcept { return m_data[12]; }
        constexpr T& y3() noexcept { return m_data[13]; }
        constexpr T& z3() noexcept { return m_data[14]; }
        constexpr T& w3() noexcept { return m_data[15]; }
        constexpr const T& x0() const noexcept { return m_data[0]; }
        constexpr const T& y0() const noexcept { return m_data[1]; }
        constexpr const T& z0() const noexcept { return m_data[2]; }
        constexpr const T& w0() const noexcept { return m_data[3]; }
        constexpr const T& x1() const noexcept { return m_data[4]; }
        constexpr const T& y1() const noexcept { return m_data[5]; }
        constexpr const T& z1() const noexcept { return m_data[6]; }
        constexpr const T& w1() const noexcept { return m_data[7]; }
        constexpr const T& x2() const noexcept { return m_data[8]; }
        constexpr const T& y2() const noexcept { return m_data[9]; }
        constexpr const T& z2() const noexcept { return m_data[10]; }
        constexpr const T& w2() const noexcept { return m_data[11]; }
        constexpr const T& x3() const noexcept { return m_data[12]; }
        constexpr const T& y3() const noexcept { return m_data[13]; }
        constexpr const T& z3() const noexcept { return m_data[14]; }
        constexpr const T& w3() const noexcept { return m_data[15]; }
        template <size_t I, size_t J> requires (I < 4 && J < 4) constexpr T& component() noexcept { return m_data[I * rows + J]; }
        template <size_t I, size_t J> requires (I < 4 && J < 4) constexpr const T& component() const noexcept { return m_data[I * rows + J]; }
        constexpr T* data() noexcept { return m_data; }
        constexpr const T* data() const noexcept { return m_data; }
        constexpr T& component(size_t i, size_t j) noexcept { assert(i < 4 && j < 4); return m_data[i * rows + j]; }
        constexpr const T& component(size_t i, size_t j) const noexcept { assert(i < 4 && j < 4); return m_data[i * rows + j]; }
};


using packed_vec2 = packed_vec<2, float>;
using packed_vec3 = packed_vec<3, float>;
using packed_vec4 = packed_vec<4, float>;

using packed_mat2 = packed_mat<2, 2, float>;
using packed_mat3 = packed_mat<3, 3, float>;
using packed_mat4 = packed_mat<4, 4, float>;


#endif  // I_GRAPHICS_DEVICE_BACKENDS_VULKAN_LINALG_PACKED_HPP_
