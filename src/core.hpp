
#ifndef CORE_HPP_
#define CORE_HPP_

#include <cstdint>
#include <source_location>
#include <type_traits>
#include <vector>
#include <optional>
#include <format>
#include <array>
#include <variant>
#include <utility>
#include <functional>

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
std::variant<A...> variant_cast(const std::variant<B...>& v) {
    return std::visit([](const auto& x) -> std::variant<A...> {
        return x;
    }, v);
}

template<typename A, typename... B>
requires (sizeof...(B) > 0)
A variant_cast_to(const std::variant<B...>& v) {
    return std::visit([](const auto& x) -> A {
        return static_cast<A>(x);
    }, v);
}

template <std::integral T, std::integral Min, std::integral Max, std::integral Def>
T bounded_or(T value, Min min, Max max, Def def) {
    using common = std::common_type_t<T, Min, Max, Def>;
    static_assert(std::is_same_v<common, T> || std::is_same_v<T, std::common_type_t<T, common>>, "bounded_or: type conversion would lose information");
    if (static_cast<common>(value) > static_cast<common>(max) || 
        static_cast<common>(value) < static_cast<common>(min)) {
        return static_cast<T>(def);
    }
    return value;
}

constexpr std::string_view relative_path(std::string_view file, std::string_view root) {
    if (file.starts_with(root)) {
        file.remove_prefix(root.size());
        while (!file.empty() && (file.front() == '/' || file.front() == '\\')) {
            file.remove_prefix(1);
        }
    }
    return file;
}

constexpr std::string_view current_file(std::source_location loc = std::source_location::current(), std::string_view root = SOURCE_ROOT_DIR) {
    return relative_path(loc.file_name(), root);
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



// opaque handles

enum class handle_type : uint8_t {
    undefined = 0,
    window = 1,
    file = 2,
    action = 3,
    component = 4,
    mesh = 5,
    setting = 6,
    depth_domain = 7,
    render_target = 8,
    render_target_set = 9,
    entity_image = 10,
    component_chunk = 11,
    job = 12
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

template <typename E>
unexpected(E error) -> unexpected<E>;

template <typename T, typename E>
class expected {
    public:
        using value_type = T;
        using error_type = E;

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

        operator bool() const { return has_value(); }
        
        template<typename F>
        requires std::same_as<expected<typename std::invoke_result_t<F, const T&>::value_type, E>, std::invoke_result_t<F, const T&>>
        auto and_then(F&& f) {
            if (m_has_value) {
                return std::invoke(f, **this);
            } else {
                return std::invoke_result_t<F, T>(unexpected(m_data.error));
            }
        }

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

class expected_void {};

template <typename E>
class expected<void, E> {
    public:
        using value_type = void;
        using error_type = E;

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

        operator bool() const { return has_value(); }
        
        template<typename F>
        requires std::same_as<expected<typename std::invoke_result_t<F>::value_type, E>, std::invoke_result_t<F>>
        auto and_then(F&& f) {
            if (m_has_value) {
                return std::invoke(f, **this);
            } else {
                return std::invoke_result_t<F>(unexpected(m_data.error));
            }
        }

    private:
        union data {
            E error;
            data() {}
            ~data() {}
        } m_data;
        bool m_has_value;
};


// manual std::scope_exit

template<typename F>
class scope_exit {
    public:
        explicit scope_exit(F&& f) : m_f(std::forward<F>(f)) {}
        scope_exit(const scope_exit&) = delete;
        scope_exit& operator=(const scope_exit&) = delete;
        scope_exit(scope_exit&& other) noexcept : m_f(std::move(other.m_f)), m_active(std::exchange(other.m_active, false)) {}

        ~scope_exit() noexcept { if (m_active) m_f(); }
        void release() noexcept { m_active = false; }

    private:
        F m_f;
        bool m_active = true;
};

template<typename F>
scope_exit(F) -> scope_exit<F>;


#endif  // CORE_HPP_

