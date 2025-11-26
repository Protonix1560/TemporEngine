
#include "core.hpp"
#include "logger.hpp"
#include "plugin.h"
#include "tempor.hpp"



void tpr_api::log(TprLogLevel logLevel, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.log(logLevel) << message;
}

void tpr_api::logInfo(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.info() << message;
}

void tpr_api::logWarn(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.warn() << message;
}

void tpr_api::logError(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.error() << message;
}

void tpr_api::logDebug(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.debug() << message;
}

void tpr_api::logTrace(const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.trace() << message;
}



void tpr_api::logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.log(logLevel, logStyle) << message;
}

void tpr_api::logInfoStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.info(logStyle) << message;
}

void tpr_api::logWarnStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.warn(logStyle) << message;
}

void tpr_api::logErrorStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.error(logStyle) << message;
}

void tpr_api::logDebugStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.debug(logStyle) << message;
}

void tpr_api::logTraceStyled(TprLogStyle logStyle, const char *message) noexcept {
    Logger& logger = swapServiceLocator()->get<Logger>();
    logger.trace(logStyle) << message;
}



TprResult tpr_api::declareComponent(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.declareComponent(componentSize, componentName, pNewComponentId);
}

TprResult tpr_api::acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.acquireComponent(componentName, pComponentId);
}

TprResult tpr_api::createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, uint32_t* pEntityId) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.createEntity(componentIdCount, pComponentIds, pEntityId);
}

TprResult tpr_api::destroyEntity(uint32_t entityId) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.destroyEntity(entityId);
}

TprResult tpr_api::modifyEntityComponentSet(uint32_t entityId, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) noexcept {
    return TPR_SUCCESS;
}

TprResult tpr_api::copyEntityComponentData(uint32_t entityId, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.copyEntityComponentData(entityId, componentId, start, end, componentData);
}

TprResult tpr_api::readEntityComponent8bit(uint32_t entityId, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.readEntityComponent8bit(entityId, componentId, offset, data);
}

TprResult tpr_api::readEntityComponent16bit(uint32_t entityId, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.readEntityComponent16bit(entityId, componentId, offset, data);
}

TprResult tpr_api::readEntityComponent32bit(uint32_t entityId, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.readEntityComponent32bit(entityId, componentId, offset, data);
}

TprResult tpr_api::readEntityComponent64bit(uint32_t entityId, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.readEntityComponent64bit(entityId, componentId, offset, data);
}

TprResult tpr_api::writeEntityComponentData(uint32_t entityId, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.writeEntityComponentData(entityId, componentId, componentData, start, end);
}

TprResult tpr_api::writeEntityComponent8bit(uint32_t entityId, uint32_t componentId, uint8_t data, uint32_t offset) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.writeEntityComponent8bit(entityId, componentId, data, offset);
}

TprResult tpr_api::writeEntityComponent16bit(uint32_t entityId, uint32_t componentId, uint16_t data, uint32_t offset) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.writeEntityComponent16bit(entityId, componentId, data, offset);
}

TprResult tpr_api::writeEntityComponent32bit(uint32_t entityId, uint32_t componentId, uint32_t data, uint32_t offset) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.writeEntityComponent32bit(entityId, componentId, data, offset);
}

TprResult tpr_api::writeEntityComponent64bit(uint32_t entityId, uint32_t componentId, uint64_t data, uint32_t offset) noexcept {
    SceneManager& sceneManager = swapServiceLocator()->get<SceneManager>();
    return sceneManager.writeEntityComponent64bit(entityId, componentId, data, offset);
}



