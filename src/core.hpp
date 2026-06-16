
#ifndef CORE_HPP_
#define CORE_HPP_

#include <cstdint>
#include <type_traits>
#include <vector>
#include <optional>
#include <format>
#include <array>
#include <variant>

#ifdef HAVE_UNISTD_H
    #include "unistd.h"
#endif



using namespace std::string_literals;


// definitions

#if defined(_WIN32)
    #define WINDOWS
#endif

#if defined(_POSIX_VERSION)
    #define POSIX
#endif

#if defined(__linux__)
    #define LINUX
#endif

#define THREAD_SAFE
#define SIG_SAFE

#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
    #define RESTRICT __restrict
#else
    #define RESTRICT
#endif

#define ENV_TEMPOR_CONF_PATH "TEMPOR_CONF_PATH"



// smol utility things

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename>
inline constexpr bool dependent_false_v = false;

template<typename... A, typename... B>
requires (sizeof...(A) > 0 && sizeof...(B) > 0)
std::variant<A...> variant_cast(const std::variant<B...>& v)
{
    return std::visit([](const auto& x) -> std::variant<A...> {
        return x;
    }, v);
}



// converter from something to string

template <typename T>
class string_converter {
    public:
        static constexpr bool is_convertable() { return false; };
        static std::string convert(const T& value) { static_assert(false, "string_converter: not convertable"); return {}; }
};

template <>
class string_converter<const char*> {
    public:
        static constexpr bool is_convertable() { return true; };
        static std::string convert(const char* value) { return std::string(value); }
};

template <typename T>
requires (std::is_integral_v<T>)
class string_converter<T> {
    public:
        static constexpr bool is_convertable() { return true; };
        static std::string convert(T value) { return std::to_string(value); }
};

template <typename T>
requires (std::is_integral_v<std::underlying_type_t<T>>)
class string_converter<T> {
    public:
        static constexpr bool is_convertable() { return true; };
        static std::string convert(T value) { return std::to_string(to_underlying(value)); }
};



// consteval string
// a string type that supports compile-time operations and that doesn't allocate any memory

template <typename CharT, size_t N>
class basic_consteval_string {

    public:

        consteval basic_consteval_string() : m_str{} {}

        consteval basic_consteval_string(const CharT (&str)[N]) : m_str{} {
            for (size_t i = 0; i < N; i++) {
                m_str[i] = str[i];
            }
        }

        template <size_t M>
        consteval basic_consteval_string<CharT, N + M - 1UL> operator+(const basic_consteval_string<CharT, M>& other) const {
            basic_consteval_string<CharT, N + M - 1UL> concat{};
            for (size_t i = 0; i < N - 1; i++) {
                concat.m_str[i] = m_str[i];
            }
            for (size_t i = 0; i < M - 1; i++) {
                concat.m_str[i + N - 1] = other.m_str[i];
            }
            // concat[N + M - 1] = '\0';  // concat is already nulled-out
            return concat;
        }

        template <size_t M>
        consteval basic_consteval_string<CharT, N + M - 1UL> operator+(const CharT (&str)[M]) const {
            return this->operator+(basic_consteval_string<CharT, M>(str));
        }

        consteval CharT& operator[](size_t index) {
            if (index >= N) throw "consteval_string: Index out of range";
            return m_str[index];
        }
        consteval const CharT& operator[](size_t index) const {
            if (index >= N) throw "consteval_string: Index out of range";
            return m_str[index];
        }
        
        constexpr const CharT* c_str() const { return m_str.data(); }
        constexpr operator const CharT*() const { return m_str.data(); }

        std::basic_string<CharT> string() const { return std::basic_string<CharT>(m_str.data()); }

        std::basic_string<CharT> operator+(std::basic_string<CharT> str) const { return string() + str; }

        template <size_t C>
        requires (C < N)
        consteval basic_consteval_string<CharT, C + 1UL> clamped() const {
            basic_consteval_string<CharT, C + 1UL> clamped{};
            for (size_t i = 0; i < C; i++) {
                clamped.m_str[i] = m_str[i];
            }
            clamped.m_str[C] = '\0';
            return clamped;
        }

        consteval std::basic_string_view<CharT> view() const {
            return std::basic_string_view<CharT>(m_str, N - 1);
        }

        template <typename... Args>
        std::basic_string<CharT> format(Args&&... args) const {
            constexpr auto str = m_str;
            return std::format(str.data(), std::forward<Args>(args)...);
        }
        
    private:
        std::array<CharT, N> m_str;
        template <typename, size_t> friend class basic_consteval_string;
    
};

template <typename CharT, size_t N, size_t M>
consteval auto operator+(const CharT (&const_str)[N], const basic_consteval_string<CharT, M>& comptime_str) {
    return basic_consteval_string<CharT, N>(const_str) + comptime_str;
}

template <size_t N>
using consteval_string = basic_consteval_string<char, N>;


template <size_t N>
struct nttp {
    char data[N];

    consteval nttp(const char (&s)[N]) {
        for (size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }
};

template <nttp Str>
constexpr auto operator""_ces() {
    return basic_consteval_string<char, sizeof(Str.data)>(Str.data);;
}



// constexpr type name without RTTI
// DANGER!! it is static, therefore global

#define TYPE_NAME_UNKNOWN "UNKNOWN"
#define TYPE_NAME_UNKNOWN_SHORT "UNKN"
#define TYPE_NAME_SHORT_SIZE 4

template <typename T> struct type_name {
    static constexpr consteval_string<sizeof(TYPE_NAME_UNKNOWN)> value{TYPE_NAME_UNKNOWN};
    static constexpr consteval_string<sizeof(TYPE_NAME_UNKNOWN_SHORT)> value_short{TYPE_NAME_UNKNOWN_SHORT};
};

#define REGISTER_TYPE_NAME(T)                                                                                                    \
    template <>                                                                                                                  \
    struct type_name<T> {                                                                                                        \
        public:                                                                                                                  \
            static constexpr consteval_string<sizeof(#T)> value{#T};                                                             \
            static constexpr consteval_string<5> value_short{consteval_string<sizeof(#T)>(#T).clamped<TYPE_NAME_SHORT_SIZE>()};  \
    };

#define REGISTER_TYPE_NAME_S(T, S)                                        \
    template <>                                                           \
    struct type_name<T> {                                                 \
        public:                                                           \
            static constexpr consteval_string<sizeof(#T)> value{#T};      \
            static constexpr consteval_string<sizeof(S)> value_short{S};  \
    };

template <typename T>
inline constexpr auto type_name_v = type_name<T>::value;

template <typename T>
inline constexpr auto type_name_v_s = type_name<T>::value_short;




// static registry
// DANGER!! it is static, therefore global

template <typename T, size_t N>
requires (N == 0 || N == 1)
class static_registry;


template <typename T>
class static_registry<T, 1> {
    public:
        struct registrar {
            registrar(T obj) {
                static_registry::instance().regist(obj);
            }
        };

        static static_registry& instance() {
            static static_registry reg;
            return reg;
        }
        void regist(T&& obj) {
            m_obj.reset();
            m_obj.emplace(std::forward(obj));
        }
        bool has() {
            return m_obj.has_value();
        }
        T& get() {
            if (!m_obj.has_value()) throw std::runtime_error("static_registry: no value");
            return m_obj.value();
        }
    private:
        std::optional<T> m_obj;
};


template <typename T>
class static_registry<T, 0> {
    private:
        std::vector<T> mObjs;
        
    public:
        struct registrar {
            registrar(T obj) {
                static_registry::instance().regist(obj);
            }
        };

        using iterator = typename decltype(mObjs)::iterator;

        static static_registry& instance() {
            static static_registry reg;
            return reg;
        }
        void regist(T obj) {
            mObjs.push_back(obj);
        }
        iterator begin() {
            return mObjs.begin();
        }
        iterator end() {
            return mObjs.end();
        }
};



// opaque handles utilities

enum class handle_type : uint8_t {
    undefined = 0,
    window = 1,
    resource = 2,
    action = 3,
    component = 4,
    mesh = 5,
    setting = 6,
    depth_domain = 7,
    render_target = 8,
    object_image = 9,
    component_chunk = 10,
    job = 11
};

template <typename T>
inline constexpr uint32_t get_basic_handle_index(T handle) noexcept {
    return (handle._d >> 32) & 0xFFFFFFFF;
}
template <typename T>
inline constexpr uint32_t get_basic_handle_generation(T handle) noexcept {
    return (handle._d >> 8) & 0xFFFFFF;
}
template <typename T>
inline constexpr handle_type get_basic_handle_type(T handle) noexcept {
    return static_cast<handle_type>(handle._d & 0xFF);
}
template <typename T>
inline constexpr void set_basic_handle_index(T* handle, uint32_t index) noexcept {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFFFF) << 32);
    handle->_d |= static_cast<uint64_t>(index) << 32;
}
template <typename T>
inline constexpr void set_basic_handle_generation(T* handle, uint32_t generation) noexcept {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFF) << 8);
    handle->_d |= static_cast<uint64_t>(generation & 0xFFFFFF) << 8;
}
template <typename T>
inline constexpr void set_basic_handle_type(T* handle, handle_type type) noexcept {
    handle->_d &= ~static_cast<uint64_t>(0xFF);
    handle->_d |= static_cast<uint64_t>(type);
}
template <typename T>
inline constexpr T construct_basic_handle(uint32_t index, uint32_t generation, handle_type type) noexcept {
    return {
        (static_cast<uint64_t>(index) << 32) | 
        (static_cast<uint64_t>(generation & 0xFFFFFF) << 8) |
        (static_cast<uint64_t>(type))
    };
}



// manual std::expected

template <typename E>
class unexpected {
    public:
        explicit unexpected(E error) : m_error(std::move(error)) {}
        E& error() & { return m_error; }
        const E& error() const & { return m_error; }
        E&& error() && { return std::move(m_error); }
    private:
        E m_error;
};

template <typename T, typename E>
class expected {
    public:
        expected(T value) : m_has_value(true) {
            new (&m_data.value) T(std::move(value));
        }
        expected(unexpected<E> error) : m_has_value(false) {
            new (&m_data.error) E(std::move(error.error()));
        }
        template <typename U>
        requires std::is_constructible_v<U, E>
        expected(unexpected<U> error) : m_has_value(false) {
            new (&m_data.error) E(std::move(static_cast<E>(error.error())));
        }
        expected(expected<T, E>&& other) : m_has_value(other.has_value()) {
            if (m_has_value) new (&m_data.value) T(std::move(other.value()));
            else new (&m_data.error) E(std::move(other.error()));
        }
        expected(const expected<T, E>& other) : m_has_value(other.has_value()) {
            if (m_has_value) new (&m_data.value) T(other.value());
            else new (&m_data.error) E(other.error());
        }
        expected& operator=(expected&& other) {
            if (&other == this) return *this;
            if (m_has_value && other.m_has_value) {
                m_data.value = std::move(other.m_data.value);
            } else if (!m_has_value && !other.m_has_value) {
                m_data.error = std::move(other.m_data.error);
            } else {
                if (m_has_value) m_data.value.~T();
                else m_data.error.~E();
                m_has_value = other.m_has_value;
                if (m_has_value) new (&m_data.value) T(std::move(other.m_data.value));
                else new (&m_data.error) E(std::move(other.m_data.error));
            }
            return *this;
        }
        ~expected() {
            if (m_has_value) m_data.value.~T();
            else m_data.error.~E();
        }

        bool has_value() const noexcept { return m_has_value; }

        T& value() & {
            if (!m_has_value) {
                std::string err = "expected: no value";
                if constexpr (string_converter<E>::is_convertable()) {
                    err += " (error "s + string_converter<E>::convert(error()) + ")"s;
                }
                throw std::runtime_error(err);
            }
            return m_data.value;
        }
        E& error() & {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return m_data.error;
        }
        T&& value() && {
            if (!m_has_value) {
                std::string err = "expected: no value";
                if constexpr (string_converter<E>::is_convertable()) {
                    err += " (error "s + string_converter<E>::convert(error()) + ")"s;
                }
                throw std::runtime_error(err);
            }
            return std::move(m_data.value);
        }
        E&& error() && {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return std::move(m_data.error);
        }
        const T& value() const & {
            if (!m_has_value) {
                std::string err = "expected: no value";
                if constexpr (string_converter<E>::is_convertable()) {
                    err += " (error "s + string_converter<E>::convert(error()) + ")"s;
                }
                throw std::runtime_error(err);
            }
            return m_data.value;
        }
        const E& error() const & {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return m_data.error;
        }
        T& operator*() { return value(); }
        T* operator->() { return &value(); }

        template <typename U>
        requires std::is_convertible_v<U, T>
        T value_or(U value) {
            if (!m_has_value) return value;
            return m_data.value;
        }

    private:
        union data {
            T value;
            E error;
            data() {}
            ~data() {}
        } m_data;
        bool m_has_value;
};


class expected_void {

};


template <typename E>
class expected<void, E> {
    public:
        expected() : m_has_value(true) {}
        expected(unexpected<E> error) : m_has_value(false) {
            new (&m_data.error) E(std::move(error.error()));
        }
        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, E>>>
        expected(unexpected<U> error) : m_has_value(false) {
            new (&m_data.error) E(std::move(static_cast<E>(error.error())));
        }
        expected(expected_void&& v) : m_has_value(true) {}
        expected(expected<void, E>&& other) : m_has_value(other.has_value()) {
            if (!m_has_value) new (&m_data.error) E(std::move(other.error()));
        }
        expected(const expected<void, E>& other) : m_has_value(other.has_value()) {
            if (!m_has_value) new (&m_data.error) E(other.error());
        }
        expected& operator=(expected&& other) {
            if (&other == this) return *this;
            if (m_has_value && other.m_has_value) {
            } else if (!m_has_value && !other.m_has_value) {
                m_data.error = std::move(other.m_data.error);
            } else {
                if (!m_has_value) m_data.error.~E();
                m_has_value = other.m_has_value;
                if (!m_has_value) new (&m_data.error) E(std::move(other.m_data.error));
            }
            return *this;
        }
        ~expected() {
            if (!m_has_value) m_data.error.~E();
        }

        bool has_value() const noexcept { return m_has_value; }

        void value() const {
            if (!m_has_value) {
                std::string err = "expected: no value";
                if constexpr (string_converter<E>::is_convertable()) {
                    err += " (error "s + string_converter<E>::convert(error()) + ")"s;
                }
                throw std::runtime_error(err);
            }
        }
        E& error() & {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return m_data.error;
        }
        E&& error() && {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return std::move(m_data.error);
        }
        const E& error() const & {
            if (m_has_value) throw std::runtime_error("expected: no error");
            return m_data.error;
        }

    private:
        union data {
            E error;
            data() {}
            ~data() {}
        } m_data;
        bool m_has_value;
};



#endif  // CORE_HPP_

