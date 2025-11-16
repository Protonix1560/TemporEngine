

#ifndef CORE_HPP_
#define CORE_HPP_



#include <exception>
#include <ostream>

using namespace std::string_literals;



enum class ErrCode {
    NoSupportError = -1,
    InternalError = -2,
    IOError = -3,
    FormatError = -4,
    PluginError = -5
};

inline std::ostream& operator<<(std::ostream& os, ErrCode code) {
    return os << static_cast<std::underlying_type_t<ErrCode>>(code);
}



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



enum class BackendRealization {
    VULKAN,
};



struct Settings {
    bool vulkanBackend_debug_useKhronosValidationLayer;
};



class GlobalServiceLocator {
    public:
        template<typename T>
        static void provide(T* service) {
            getSlot<T>() = service;
        }

        template<typename T>
        static T& get() {
            auto* s = getSlot<T>();
            if (!s) throw Exception(ErrCode::InternalError, "Internal service is not provided");
            return *s;
        }

    private:
        template<typename T>
        static T*& getSlot() {
            static T* instance = nullptr;
            return instance;
        }
};



inline const GlobalServiceLocator* swapServiceLocator(const GlobalServiceLocator* pServiceLocator = nullptr) {
    static const GlobalServiceLocator* sl = nullptr;
    if (pServiceLocator) sl = pServiceLocator;
    return sl;
}



#endif  // CORE_HPP_

