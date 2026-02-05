
#include "core.hpp"
#include "data_bridge.hpp"
#include "logger.hpp"
#include "tempor_api.hpp"
#include "plugin_core.h"
#include "resource_registry.hpp"
#include "scene_manager.hpp"
#include "window_manager.hpp"
#include "hardware_layer_interface.hpp"
#include "asset_store.hpp"



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
        TprResult parseAsset(const TprAssetParseInfo* pInfo, TprAsset* pAsset) noexcept {
            AssetStore& store = gGetServiceLocator()->get<AssetStore>();
            auto exp = store.parseAsset(pInfo);
            if (exp.hasValue()) {
                *pAsset = exp.value();
                return TPR_SUCCESS;
            }
            return exp.error();
        }
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

        TprResult openPathResource(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            try {
                if (!pResource) return TPR_INVALID_VALUE;
                if (!path) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(path, flags, alignment);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openRefResource(char* begin, char* end, TprOpenRefResourceFlags flags, TprResource* pResource) noexcept {
            try {
                if (!pResource) return TPR_INVALID_VALUE;
                if (!begin) return TPR_INVALID_VALUE;
                if (!end) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags);

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            try {
                if (!pResource) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(size, flags, alignment);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openCapabilityResource(TprResource resource, TprOpenCapabilityResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept {
            try {
                if (!pResource) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(resource, flags, protectFlags);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openPathResourceLifetimed(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            try {
                if (!lifetime) return TPR_INVALID_VALUE;
                if (!pResource) return TPR_INVALID_VALUE;
                if (!path) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(path, flags, alignment, lifetime);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openRefResourceLifetimed(char* begin, char* end, TprOpenRefResourceFlags flags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            try {
                if (!lifetime) return TPR_INVALID_VALUE;
                if (!pResource) return TPR_INVALID_VALUE;
                if (!begin) return TPR_INVALID_VALUE;
                if (!end) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags, lifetime);

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openEmptyResourceLifetimed(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            try {
                if (!lifetime) return TPR_INVALID_VALUE;
                if (!pResource) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(size, flags, alignment, lifetime);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult openCapabilityResourceLifetimed(TprResource resource, TprOpenCapabilityResourceFlags flags, TprProtectResourceFlags protectFlags, const TprLifetime* lifetime, TprResource* pResource) noexcept {
            try {
                if (!lifetime) return TPR_INVALID_VALUE;
                if (!pResource) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pResource = resReg.openResource(resource, flags, protectFlags, lifetime);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult resetResourceLifetime(TprResource resource, const TprLifetime* lifetime) noexcept {
            try {
                if (!lifetime) return TPR_INVALID_VALUE;
                
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                resReg.resetResourceLifetime(resource, lifetime);

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept {
            try {
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                resReg.resizeResource(resource, newSize);

            } catch (std::bad_alloc) {
                return TPR_BAD_ALLOC;
            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
            try {
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pSize = static_cast<uint64_t>(resReg.sizeofResource(resource));

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult getResourceRawRWDataPointer(TprResource resource, char** pData) noexcept {
            try {
                if (!pData) return TPR_UNKNOWN_ERROR;

                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pData = reinterpret_cast<char*>(resReg.getResourceRawRWDataPointer(resource));

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        TprResult getResourceRawRODataPointer(TprResource resource, const char** pData) noexcept {
            try {
                if (!pData) return TPR_UNKNOWN_ERROR;

                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                *pData = reinterpret_cast<const char*>(resReg.getResourceRawRODataPointer(resource));

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
                return TPR_UNKNOWN_ERROR;
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
                return TPR_UNKNOWN_ERROR;
            }
            return TPR_SUCCESS;
        }

        void closeResource(TprResource resource) noexcept {
            try {
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                resReg.closeResource(resource);

            } catch (const Exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Expected exception [" << e.code() << "]: " << e.what() << "\n";
            } catch (const std::exception& e) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << "Unexpected exception: " << e.what() << "\n";
            } catch (...) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << "Unknown exception\n";
            }
        }

    }

    namespace wm {
        TprResult openWindow(TprWindow* pHandle, const TprWindowCreateInfo* createInfo) noexcept {
            TprResult r;
            r = gGetServiceLocator()->get<WindowManager>().openWindow(pHandle, createInfo);
            if (r < 0) return r;
            r = gGetServiceLocator()->get<HardwareLayer>().registerWindow(*pHandle);
            if (r < 0) return r;
            return TPR_SUCCESS;
        }

        void closeWindow(TprWindow handle) noexcept {
            gGetServiceLocator()->get<HardwareLayer>().unregisterWindow(handle);
            gGetServiceLocator()->get<WindowManager>().closeWindow(handle);
        }
    }

}
