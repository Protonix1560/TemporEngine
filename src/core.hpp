

#ifndef CORE_HPP_
#define CORE_HPP_


#include <exception>
#include <ostream>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>
#include <cstring>



using namespace std::string_literals;



#define THREAD_SAFE
#define SIG_SAFE



#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || defined(__INTEL_COMPILER)
    #define RESTRICT __restrict
#else
    #define RESTRICT
#endif



#define ENV_TEMPOR_CONF_PATH "TEMPOR_CONF_PATH"



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



using FlagType = uint32_t;



template <size_t N>
class ConstexprString {

    public:

        constexpr ConstexprString() : mStr{} {}

        template <size_t M>
        constexpr ConstexprString(const char (&str)[M]) {
            static_assert(M == N, "ConstexprString: ConstexprString size doesn't match const char* size");
            if (str[M - 1] == '\0') {
                throw "ConstexprString: Str is not null-terminated";
            }
            for (size_t i = 0; i < M; i++) {
                mStr[i] = str[i];
            }
        }

        template <size_t M>
        constexpr ConstexprString<N + M - 1UL> operator+(const ConstexprString<M>& other) const {
            ConstexprString<N + M - 1UL> concat{};
            for (size_t i = 0; i < N - 1; i++) {
                concat[i] = mStr[i];
            }
            for (size_t i = 0; i < M; i++) {
                concat[i + N - 1] = other.mStr[i];
            }
            return concat;
        }

        template <size_t M>
        constexpr ConstexprString<N + M - 1UL> operator+(const char (&str)[M]) const {
            ConstexprString<N + M - 1UL> concat{};
            for (size_t i = 0; i < N - 1; i++) {
                concat[i] = mStr[i];
            }
            for (size_t i = 0; i < M; i++) {
                concat[i + N - 1] = str[i];
            }
            return concat;
        }

        constexpr char& operator[](size_t index) { return index < N ? mStr[index] : throw "ConstexprString: Index out of range"; }
        constexpr const char& operator[](size_t index) const { return index < N ? mStr[index] : throw "ConstexprString: Index out of range"; }
        constexpr const char* cStr() const { return mStr; }
        constexpr operator const char*() const { return mStr; }

        std::string string() const { return std::string(mStr); }

        template <typename T, typename = std::enable_if_t<!std::is_array_v<T> && std::is_constructible_v<std::string, T>>>
        std::string operator+(T str) const {
            return string() + std::string(str);
        }

        std::string operator+(std::string str) const { return string() + str; }
        
    private:
        char mStr[N];
    
};


struct Exception : public std::exception {

    public:

        explicit Exception(ErrCode c, std::string msg = {})
            : errCode(c), message(std::move(msg)) {}

        template <size_t N>
        explicit Exception(ErrCode c, ConstexprString<N> msg = {})
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



struct Settings {
    bool vulkanBackendDebugUseKhronosValidationLayer;
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
            if (!s) throw Exception(ErrCode::InternalError, "Internal service is not provided");
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



enum class HandleType : uint8_t {
    Undefined = 0,
    Window = 1,
    Resource = 2,
    Asset = 3
};


template <typename T>
inline constexpr uint32_t getBasicHandleIndex(T handle) {
    return (handle._d >> 32) & 0xFFFFFFFF;
}

template <typename T>
inline constexpr uint32_t getBasicHandleGeneration(T handle) {
    return (handle._d >> 8) & 0xFFFFFF;
}

template <typename T>
inline constexpr HandleType getBasicHandleType(T handle) {
    return static_cast<HandleType>(handle._d & 0xFF);
}

template <typename T>
inline constexpr void setBasicHandleIndex(T* handle, uint32_t value) {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFFFF) << 32);
    handle->_d |= static_cast<uint64_t>(value) << 32;
}

template <typename T>
inline constexpr void setBasicHandleGeneration(T* handle, uint32_t value) {
    handle->_d &= ~(static_cast<uint64_t>(0xFFFFFF) << 8);
    handle->_d |= static_cast<uint64_t>(value & 0xFFFFFF) << 8;
}

template <typename T>
inline constexpr void setBasicHandleType(T* handle, HandleType value) {
    handle->_d &= ~static_cast<uint64_t>(0xFF);
    handle->_d |= static_cast<uint64_t>(value);
}

template <typename T>
inline constexpr T constructBasicHandle(uint32_t index, uint32_t generation, HandleType type) {
    return {
        ._d = (static_cast<uint64_t>(index) << 32) | 
              (static_cast<uint64_t>(generation & 0xFFFFFF) << 8) |
              (static_cast<uint64_t>(type))
    };
}



constexpr auto logHWLRName() {
    const char name[] = "HWLR";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxHWLR() {
    return logHWLRName() + ": ";
}

constexpr auto logHWMOName() {
    const char name[] = "HWMO";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxHWMO() {
    return logHWMOName() + ": ";
}

constexpr auto logRRegName() {
    const char name[] = "RReg";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxRReg() {
    return logRRegName() + ": ";
}

constexpr auto logPlLnName() {
    const char name[] = "PlLn";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxPlLn() {
    return logPlLnName() + ": ";
}

constexpr auto logAStrName() {
    const char name[] = "AStr";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxAStr() {
    return logAStrName() + ": ";
}

constexpr auto logWinMName() {
    const char name[] = "WinM";
    return ConstexprString<std::size(name)>(name);
}
constexpr auto logPrxWinM() {
    return logWinMName() + ": ";
}



// template <size_t N>
// constexpr auto makeArray(const char (&str)[N]) { return str; }

// #define LOG_HARDWARE_LAYER_INTERFACE_NAME "HWLI"
// constexpr auto logHWLI() { return makeArray("HWLI"); }
// inline constexpr const char[] logPrxHWLI() { return LOG_HARDWARE_LAYER_INTERFACE_NAME ": "; }



// for std::variant
template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;


// manual std::expected

template <typename E>
class Unexpected {
    public:
        explicit Unexpected(E error) : mError(std::move(error)) {}
        E& error() & { return mError; }
        const E& error() const & { return mError; }
        E&& error() && { return std::move(mError); }
    private:
        E mError;
};

template <typename T, typename E>
class Expected {
    public:
        Expected(T value) : mHasValue(true) {
            new (&mData.value) T(std::move(value));
        }
        Expected(Unexpected<E> error) : mHasValue(false) {
            new (&mData.error) E(std::move(error.error()));
        }
        Expected(Expected<T, E>&& other) : mHasValue(other.hasValue()) {
            if (mHasValue) new (&mData.value) T(std::move(other.value()));
            else new (&mData.error) E(std::move(other.error()));
        }
        Expected(const Expected<T, E>& other) : mHasValue(other.hasValue()) {
            if (mHasValue) new (&mData.value) T(other.value());
            else new (&mData.error) E(other.error());
        }
        Expected& operator=(Expected&& other) {
            if (&other == this) return *this;
            if (mHasValue && other.mHasValue) {
                mData.value = std::move(other.mData.value);
            } else if (!mHasValue && !other.mHasValue) {
                mData.error = std::move(other.mData.error);
            } else {
                if (mHasValue) mData.value.~T();
                else mData.error.~E();
                mHasValue = other.mHasValue;
                if (mHasValue) new (&mData.value) T(std::move(other.mData.value));
                else new (&mData.error) E(std::move(other.mData.error));
            }
            return *this;
        }
        ~Expected() {
            if (mHasValue) mData.value.~T();
            else mData.error.~E();
        }

        bool hasValue() const noexcept { return mHasValue; }

        T& value() & {
            if (!mHasValue) throw std::runtime_error("Expected: no value");
            return mData.value;
        }
        E& error() & {
            if (mHasValue) throw std::runtime_error("Expected: no error");
            return mData.error;
        }
        T&& value() && {
            if (!mHasValue) throw std::runtime_error("Expected: no value");
            return std::move(mData.value);
        }
        E&& error() && {
            if (mHasValue) throw std::runtime_error("Expected: no error");
            return std::move(mData.error);
        }
        const T& value() const & {
            if (!mHasValue) throw std::runtime_error("Expected: no value");
            return mData.value;
        }
        const E& error() const & {
            if (mHasValue) throw std::runtime_error("Expected: no error");
            return mData.error;
        }
        T& operator*() { return value(); }
        T* operator->() { return &value(); }

    private:
        union Data {
            T value;
            E error;
            Data() {}
            ~Data() {}
        } mData;
        bool mHasValue;
};



inline void* memcpyStride(void* RESTRICT dest, const void* RESTRICT src, size_t n, size_t elSize, size_t srcStride, size_t dstStride = 0) {
    const char* s = reinterpret_cast<const char*>(src);
    char* d = reinterpret_cast<char*>(dest);
    for (size_t i = 0; i < n; i++) {
        std::memcpy(d, s, elSize);
        s += (srcStride != 0) ? srcStride : elSize;
        d += (dstStride != 0) ? dstStride : elSize;
    }
    return dest;
}



#endif  // CORE_HPP_

