#pragma once

#include <string>
#include <exception>
#include <ostream>

using namespace std::string_literals;



enum class ErrCode {
    NoSupportError = -1,
    InternalError = -2,
    IOError = -3,
    FormatError = -4
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

