
#include <iostream>



enum class LogLevel {
    Info, Error, Warning
};



enum class LogStyle {
    Normal
};



class Logger;



struct Log {

    public:
        template <typename T>
        Log& operator<<(T msg) {
            if (logLevel == LogLevel::Error) {
                std::cerr << msg;
            } else {
                std::cout << msg;
            }
            return *this;
        }

    private:
        LogLevel logLevel;
        LogStyle logStyle;

        Log(LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal)
            : logLevel(logLevel), logStyle(logStyle) {}

        friend class Logger;

};



class Logger {

    public:

        template <typename T>
        Log operator<<(T msg) {
            return Log() << msg;
        }

        Log log(LogLevel logLevel = LogLevel::Info, LogStyle logStyle = LogStyle::Normal) {
            return Log(logLevel, logStyle);
        }

};
