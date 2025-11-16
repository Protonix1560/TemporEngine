
#include "core.hpp"
#include "logger.hpp"
#include "tempor.hpp"



void tprapi::log(TprLogLevel logLevel, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.log(logLevel) << message;
}

void tprapi::logInfo(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.info() << message;
}

void tprapi::logWarn(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.warn() << message;
}

void tprapi::logError(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.error() << message;
}

void tprapi::logDebug(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.debug() << message;
}

void tprapi::logTrace(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.trace() << message;
}



void tprapi::logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.log(logLevel, logStyle) << message;
}

void tprapi::logInfoStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.info(logStyle) << message;
}

void tprapi::logWarnStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.warn(logStyle) << message;
}

void tprapi::logErrorStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.error(logStyle) << message;
}

void tprapi::logDebugStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.debug(logStyle) << message;
}

void tprapi::logTraceStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.trace(logStyle) << message;
}


