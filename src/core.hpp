

#ifndef CORE_HPP_
#define CORE_HPP_


#include <exception>
#include <ostream>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <functional>
#include <memory>
#include <optional>



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



// recursive constexpr byte-by-byte copy function

template <size_t I, size_t N, size_t O = 0UL>
constexpr void constexpr_copy(const char* strsrc, char* strdst) {
    static_assert(O <= I, "constexpr_copy: O > I");
    if constexpr (I < N) {
        strdst[I] = strsrc[I - O];
        constexpr_copy<I + 1, N, O>(strsrc, strdst);
    }
}



// comptime string
// a string type that supports compile-time operations and that doesn't allocate any memory

template <size_t N>
class comptime_string {

    public:

        constexpr comptime_string() : m_str{} {}

        constexpr comptime_string(const char (&str)[N]) : m_str{} {
            constexpr_copy<0, N>(str, m_str);
        }

        template <size_t M>
        constexpr comptime_string<N + M - 1UL> operator+(const comptime_string<M>& other) const {
            comptime_string<N + M - 1UL> concat{};
            constexpr_copy<0, N - 1>(c_str(), concat.m_str);
            constexpr_copy<N - 1, M + N - 1, N - 1>(other.m_str, concat.m_str);
            return concat;
        }

        template <size_t M>
        constexpr comptime_string<N + M - 1UL> operator+(const char (&str)[M]) const {
            return *this + comptime_string<M>(str);
        }

        constexpr char& operator[](size_t index) { return index < N ? m_str[index] : throw "expr_string: Index out of range"; }
        constexpr const char& operator[](size_t index) const { return index < N ? m_str[index] : throw "expr_string: Index out of range"; }
        constexpr const char* c_str() const { return m_str; }
        constexpr operator const char*() const { return m_str; }

        std::string string() const { return std::string(m_str); }

        template <typename T, typename = std::enable_if_t<!std::is_array_v<T> && std::is_constructible_v<std::string, T>>>
        std::string operator+(T str) const {
            return string() + std::string(str);
        }

        std::string operator+(std::string str) const { return string() + str; }
        
    private:
        char m_str[N];
        template <size_t> friend class comptime_string;
    
};

template <size_t N, size_t M>
constexpr auto operator+(const char (&conststr)[N], const comptime_string<M>& constexprstr) {
    return comptime_string<N>(conststr) + constexprstr;
}



// constexpr type name without RTTI
// DANGER!! it is static, therefore global

#define TYPE_NAME_UNKNOWN "UNKNOWN"
#define TYPE_NAME_UNKNOWN_SHORT "UNKN"

template <typename T> struct type_name {
    static constexpr comptime_string<sizeof(TYPE_NAME_UNKNOWN)> value = {TYPE_NAME_UNKNOWN};
    static constexpr comptime_string<sizeof(TYPE_NAME_UNKNOWN_SHORT)> value_short = {TYPE_NAME_UNKNOWN_SHORT};
};

#define REGISTER_TYPE_NAME(T)                                                \
    template <>                                                              \
    struct type_name<T> {                                                    \
        public:                                                              \
            static constexpr comptime_string<sizeof(#T)> value{#T};          \
            static constexpr comptime_string<sizeof(#T)> value_short{#T};    \
    };

#define REGISTER_TYPE_NAME_S(T, S)                                           \
    template <>                                                              \
    struct type_name<T> {                                                    \
        public:                                                              \
            static constexpr comptime_string<sizeof(#T)> value{#T};          \
            static constexpr comptime_string<sizeof(S)> value_short{S};      \
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
    asset = 3
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

constexpr auto logHWLRName() {
    const char name[] = "HWLr";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxHWLR() {
    return logHWLRName() + ": ";
}

constexpr auto logHWMOName() {
    const char name[] = "HWMO";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxHWMO() {
    return logHWMOName() + ": ";
}

constexpr auto logRRegName() {
    const char name[] = "RReg";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxRReg() {
    return logRRegName() + ": ";
}

constexpr auto logPlLnName() {
    const char name[] = "PlLn";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxPlLn() {
    return logPlLnName() + ": ";
}

constexpr auto logAStrName() {
    const char name[] = "AStr";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxAStr() {
    return logAStrName() + ": ";
}

constexpr auto logWinMName() {
    const char name[] = "WinM";
    return comptime_string<std::size(name)>(name);
}
constexpr auto logPrxWinM() {
    return logWinMName() + ": ";
}



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



/*

        TRASH FIELD  TRASH FIELD  TRASH FIELD  TRASH FIELD
        TRASH FIELD  TRASH FIELD  TRASH FIELD  TRASH FIELD

*/



enum class ErrCode {
    NoSupportError = -1,
    InternalError = -2,
    IOError = -3,
    FormatError = -4,
    PluginError = -5,
    WrongValueError = -6,
    AccessError = -7
};



inline std::ostream& operator<<(std::ostream& os, ErrCode code) {
    return os << static_cast<std::underlying_type_t<ErrCode>>(code);
}



struct Exception : public std::exception {

    public:

        explicit Exception(ErrCode c, std::string msg = {})
            : errCode(c), message(std::move(msg)) {}

        template <size_t N>
        explicit Exception(ErrCode c, comptime_string<N> msg = {})
            : errCode(c), message(msg) {}

        const char* what() const noexcept override {
            if (message.empty()) {
                return defaultMessage();
            }
            return message.c_str();
        }

        ErrCode code() const noexcept {
            return errCode;
        }
    
    private:
        const char* defaultMessage() const noexcept {
            switch (errCode) {
                default: return "unknown error";
            }
        }

        std::string message;
        ErrCode errCode;

};



class ServiceLocator {
    public:
        template<typename T>
        static void provide(T* service) {
            getSlot<T>() = service;
        }

        template<typename T>
        static T& get() {
            T* s = getSlot<T>();
            if (!s) throw Exception(ErrCode::InternalError, "Service " + type_name<T>::value + " is not provided");
            return *s;
        }

        template<typename T>
        static T* getPtr() {
            T* s = getSlot<T>();
            return s;
        }

        template <typename Base>
        static void registerListFactory(std::function<std::unique_ptr<Base>()> factory) {
            getFactoryList<Base>().push_back(std::move(factory));
        }

        template <typename Base>
        static const std::vector<std::function<std::unique_ptr<Base>()>>& getListFactories() {
            return getFactoryList<Base>();
        }

    private:
        template<typename T>
        static T*& getSlot() {
            static T* instance = nullptr;
            return instance;
        }

        template <typename Base>
        static std::vector<std::function<std::unique_ptr<Base>()>>& getFactoryList() {
            static std::vector<std::function<std::unique_ptr<Base>()>> list;
            return list;
        }
};


inline const ServiceLocator* gGetServiceLocator() {
    static ServiceLocator sl;
    return &sl;
}


template <typename Base>
struct FactoryListRegistrar {
    explicit FactoryListRegistrar(std::function<std::unique_ptr<Base>()> element) {
        gGetServiceLocator()->registerListFactory(element);
    }
};




#endif  // CORE_HPP_

