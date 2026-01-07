

#ifndef CORE_HPP_
#define CORE_HPP_


#include <exception>
#include <ostream>
#include <cstdint>
#include <vector>
#include <functional>
#include <memory>
#include <iostream>


using namespace std::string_literals;



#define THREAD_SAFE
#define SIG_SAFE



#define ENV_TEMPOR_CONF_PATH "TEMPOR_CONF_PATH"



enum class ErrCode {
    NoSupportError = -1,
    InternalError = -2,
    IOError = -3,
    FormatError = -4,
    PluginError = -5,
    WrongValueError = -6
};

inline std::ostream& operator<<(std::ostream& os, ErrCode code) {
    return os << static_cast<std::underlying_type_t<ErrCode>>(code);
}



using FlagType = uint32_t;



struct Exception : public std::exception {

    public:

        explicit Exception(ErrCode c, std::string msg = {})
            : errCode(c), message(std::move(msg)) {}

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



enum class HandleType : uint8_t {
    Undefined = 0,
    Window = 1
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



class GlobalServiceLocator {
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


inline const GlobalServiceLocator* gGetServiceLocator() {
    static GlobalServiceLocator sl;
    return &sl;
}


template <typename Base>
struct FactoryListRegistrar {
    explicit FactoryListRegistrar(std::function<std::unique_ptr<Base>()> element) {
        gGetServiceLocator()->registerListFactory(element);
    }
};


#define LOG_RENDERER_NAME "Rndr"
#define LOG_VFS_NAME "VFS"
#define LOG_WINDOW_MANAGER_NAME "WM"
#define LOG_PLUGIN_LAUNCHER_NAME "PL"


#endif  // CORE_HPP_

