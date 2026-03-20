

#ifndef CORE_HPP_
#define CORE_HPP_


// #include <exception>
// #include <ostream>
#include <cstdint>
#include <type_traits>
#include <vector>
// #include <functional>
// #include <memory>
#include <optional>
#include <format>
#include <array>

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

template <typename... Ts> struct overload : Ts... { using Ts::operator()...; };
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

template <typename E>
constexpr typename std::underlying_type<E>::type to_underlying(E e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}

template <typename> inline constexpr bool dependent_false_v = false;



// converter from something to string

template <typename T, typename Enable = void>
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
class string_converter<T, typename std::enable_if_t<std::is_integral_v<T>>> {
    public:
        static constexpr bool is_convertable() { return true; };
        static std::string convert(T value) { return std::to_string(value); }
};

template <typename T>
class string_converter<T, typename std::enable_if_t<std::is_integral_v<std::underlying_type_t<T>>>> {
    public:
        static constexpr bool is_convertable() { return true; };
        static std::string convert(T value) { return std::to_string(static_cast<std::underlying_type_t<T>>(value)); }
};



// consteval string
// a string type that supports compile-time operations and that doesn't allocate any memory

template <size_t N>
class consteval_string {

    public:

        consteval consteval_string() : m_str{} {}

        consteval consteval_string(const char (&str)[N]) : m_str{} {
            for (size_t i = 0; i < N; i++) {
                m_str[i] = str[i];
            }
        }

        template <size_t M>
        consteval consteval_string<N + M - 1UL> operator+(const consteval_string<M>& other) const {
            consteval_string<N + M - 1UL> concat{};
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
        consteval consteval_string<N + M - 1UL> operator+(const char (&str)[M]) const {
            return this->operator+(consteval_string<M>(str));
        }

        consteval char& operator[](size_t index) {
            if (index >= N) throw "consteval_string: Index out of range";
            return m_str[index];
        }
        consteval const char& operator[](size_t index) const {
            if (index >= N) throw "consteval_string: Index out of range";
            return m_str[index];
        }
        
        constexpr const char* c_str() const { return m_str.data(); }
        constexpr operator const char*() const { return m_str.data(); }

        std::string string() const { return std::string(m_str.data()); }

        std::string operator+(std::string str) const { return string() + str; }

        template <size_t C>
        requires (C < N)
        consteval consteval_string<C + 1UL> clamped() const {
            consteval_string<C + 1UL> clamped{};
            for (size_t i = 0; i < C; i++) {
                clamped.m_str[i] = m_str[i];
            }
            clamped.m_str[C] = '\0';
            return clamped;
        }

        consteval std::string_view view() const {
            return std::string_view(m_str, N - 1);
        }

        template <typename... Args>
        std::string format(Args&&... args) const {
            constexpr auto str = m_str;
            return std::format(str.data(), std::forward<Args>(args)...);
        }
        
    private:
        std::array<char, N> m_str;
        template <size_t> friend class consteval_string;
    
};

template <size_t N, size_t M>
consteval auto operator+(const char (&const_str)[N], const consteval_string<M>& comptime_str) {
    return consteval_string<N>(const_str) + comptime_str;
}



// constexpr type name without RTTI
// DANGER!! it is static, therefore global

#define TYPE_NAME_UNKNOWN "UNKNOWN"
#define TYPE_NAME_UNKNOWN_SHORT "UNKN"

template <typename T> struct type_name {
    static constexpr consteval_string<sizeof(TYPE_NAME_UNKNOWN)> value{TYPE_NAME_UNKNOWN};
    static constexpr consteval_string<sizeof(TYPE_NAME_UNKNOWN_SHORT)> value_short{TYPE_NAME_UNKNOWN_SHORT};
};

#define REGISTER_TYPE_NAME(T)                                                                                 \
    template <>                                                                                               \
    struct type_name<T> {                                                                                     \
        public:                                                                                               \
            static constexpr consteval_string<sizeof(#T)> value{#T};                                          \
            static constexpr consteval_string<5> value_short{consteval_string<sizeof(#T)>(#T).clamped<4>()};  \
    };

#define REGISTER_TYPE_NAME_S(T, S)                                                                            \
    template <>                                                                                               \
    struct type_name<T> {                                                                                     \
        public:                                                                                               \
            static constexpr consteval_string<sizeof(#T)> value{#T};                                          \
            static constexpr consteval_string<sizeof(S)> value_short{S};                                      \
    };

template <typename T>
using type_name_v = typename type_name<T>::value;




// static registry
// DANGER!! it is static, therefore global

template <typename T, size_t N, typename = std::enable_if_t<N == 0 || N == 1>>
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
            m_obj.emplace(obj);
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
    asset = 3,
    action = 4
};


template <typename T>
inline constexpr uint32_t get_basic_handle_index(T handle) {
    return (handle._d >> 32) & 0xFFFFFFFF;
}

template <typename T>
inline constexpr uint32_t get_basic_handle_generation(T handle) {
    return (handle._d >> 8) & 0xFFFFFF;
}

template <typename T>
inline constexpr handle_type get_basic_handle_type(T handle) {
    return static_cast<handle_type>(handle._d & 0xFF);
}

template <typename T>
inline constexpr void set_basic_handle_index(T* handle, uint32_t value) {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFFFF) << 32);
    handle->_d |= static_cast<uint64_t>(value) << 32;
}

template <typename T>
inline constexpr void set_basic_handle_generation(T* handle, uint32_t value) {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFF) << 8);
    handle->_d |= static_cast<uint64_t>(value & 0xFFFFFF) << 8;
}

template <typename T>
inline constexpr void set_basic_handle_type(T* handle, handle_type value) {
    handle->_d &= ~static_cast<uint64_t>(0xFF);
    handle->_d |= static_cast<uint64_t>(value);
}

template <typename T>
inline constexpr T construct_basic_handle(uint32_t index, uint32_t generation, handle_type type) {
    return {
        ._d = (static_cast<uint64_t>(index) << 32) | 
              (static_cast<uint64_t>(generation & 0xFFFFFF) << 8) |
              (static_cast<uint64_t>(type))
    };
}



// log names

consteval auto logPHWLName() {
    const char name[] = "PHWL";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxPHWL() {
    return logPHWLName() + ": ";
}

consteval auto logHWMOName() {
    const char name[] = "HWMO";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxHWMO() {
    return logHWMOName() + ": ";
}

consteval auto logRRegName() {
    const char name[] = "RReg";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxRReg() {
    return logRRegName() + ": ";
}

consteval auto logPlLdName() {
    const char name[] = "PlLd";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxPlLd() {
    return logPlLdName() + ": ";
}

consteval auto logAStrName() {
    const char name[] = "AStr";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxAStr() {
    return logAStrName() + ": ";
}

consteval auto logWinMName() {
    const char name[] = "WinM";
    return consteval_string<std::size(name)>(name);
}
consteval auto logPrxWinM() {
    return logWinMName() + ": ";
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
        template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, E>>>
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

