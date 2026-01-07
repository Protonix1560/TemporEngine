
#include "core.hpp"
#include "data_bridge.hpp"
#include "logger.hpp"
#include "tempor_api.hpp"
#include "plugin_core.h"
#include "scene_manager.hpp"
#include "window_manager.hpp"
#include "irenderer.hpp"



namespace tpr_api {

    namespace log {

        void log(TprLogLevel logLevel, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.log(logLevel) << message;
        }

        void logInfo(const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.info() << message;
        }

        void logWarn(const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.warn() << message;
        }

        void logError(const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.error() << message;
        }

        void logDebug(const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.debug() << message;
        }

        void logTrace(const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.trace() << message;
        }

        void logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.log(logLevel, logStyle) << message;
        }

        void logInfoStyled(TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.info(logStyle) << message;
        }

        void logWarnStyled(TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.warn(logStyle) << message;
        }

        void logErrorStyled(TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.error(logStyle) << message;
        }

        void logDebugStyled(TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.debug(logStyle) << message;
        }

        void logTraceStyled(TprLogStyle logStyle, const char *message) noexcept {
            Logger& logger = gGetServiceLocator()->get<Logger>();
            logger.trace(logStyle) << message;
        }

    }

    namespace scene {

        TprResult registerComponent(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.registerComponent(componentSize, componentName, pNewComponentId);
        }

        TprResult acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.acquireComponent(componentName, pComponentId);
        }

        TprResult createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntityHandle* pEntityHandle) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.createEntity(componentIdCount, pComponentIds, pEntityHandle);
        }

        void destroyEntity(const TprEntityHandle* entityHandle) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            sceneManager.destroyEntity(entityHandle);
        }

        TprResult modifyEntityComponentSet(const TprEntityHandle* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult copyEntityComponentData(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.copyEntityComponentData(entityHandle, componentId, start, end, componentData);
        }

        TprResult readEntityComponent8bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.readEntityComponent8bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent16bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.readEntityComponent16bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent32bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.readEntityComponent32bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent64bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.readEntityComponent64bit(entityHandle, componentId, offset, data);
        }

        TprResult writeEntityComponentData(const TprEntityHandle* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.writeEntityComponentData(entityHandle, componentId, componentData, start, end);
        }

        TprResult writeEntityComponent8bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.writeEntityComponent8bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent16bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.writeEntityComponent16bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent32bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.writeEntityComponent32bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent64bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) noexcept {
            SceneManager& sceneManager = gGetServiceLocator()->get<SceneManager>();
            return sceneManager.writeEntityComponent64bit(entityHandle, componentId, data, offset);
        }

    }

    namespace geo {

    }

    namespace render {
        TprResult growEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t count, TprEntityDrawDesc** start) noexcept {
            DataBridge& bridge = gGetServiceLocator()->get<DataBridge>();
            return bridge.growOArray(oarray, count, reinterpret_cast<void**>(start));
        }

        TprResult endEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t actualSize) noexcept {
            DataBridge& bridge = gGetServiceLocator()->get<DataBridge>();
            return bridge.endOArray(oarray, actualSize);
        }

        TprResult sizeofEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t* size) noexcept {
            DataBridge& bridge = gGetServiceLocator()->get<DataBridge>();
            return bridge.sizeofOArray(oarray, size);
        }
    }

    namespace vfs {

        TprResult openResouceByPath(const char* path, TprOpenResourceFlags flags, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceByBuffer(const char* begin, const char* end, TprOpenResourceFlags flags, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceEmpty(uint64_t size, TprOpenResourceFlags flags, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceZeroed(uint64_t size, TprOpenResourceFlags flags, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResouceByPathLifetimed(const char* path, TprOpenResourceFlags flags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceByBufferLifetimed(const char* begin, const char* end, TprOpenResourceFlags flags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceEmptyLifetimed(uint64_t size, TprOpenResourceFlags flags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult openResourceZeroedLifetimed(uint64_t size, TprOpenResourceFlags flags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult resetResourceLifetime(TprResource resource, const TprLifetime* lifetime) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        void closeResource(TprResource resource) noexcept {

        }

    }

    namespace wm {
        TprResult openWindow(TprWindow* pHandle, const TprWindowCreateInfo* createInfo) noexcept {
            TprResult r;
            r = gGetServiceLocator()->get<WindowManager>().openWindow(pHandle, createInfo);
            if (r < 0) return r;
            r = gGetServiceLocator()->get<IRenderer>().registerWindow(*pHandle);
            if (r < 0) return r;
            return TPR_SUCCESS;
        }

        void closeWindow(TprWindow handle) noexcept {
            gGetServiceLocator()->get<IRenderer>().unregisterWindow(handle);
            gGetServiceLocator()->get<WindowManager>().closeWindow(handle);
        }
    }

}
