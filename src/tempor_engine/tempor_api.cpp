
#include "core.hpp"
#include "data_bridge.hpp"
#include "logger.hpp"
#include "tempor_api.hpp"
#include "plugin_core.h"
#include "resource_registry.hpp"
#include "scene_graph.hpp"
#include "window_manager.hpp"
#include "hardware_layer_interface.hpp"
#include "asset_store.hpp"




template <typename T>
struct StandartHandler {
    T operator()(std::function<T() noexcept(false)> func) {
        static_assert(false, "StandartHandler: unsupported type");
    }
};


template <>
struct StandartHandler<TprResult> {
    TprResult operator()(std::function<TprResult() noexcept(false)> func) {

        auto err = gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1);
        TprResult res;

        try {
            res = func();

        } catch (const std::bad_alloc& e) {
            err << "Bad alloc: "<< e.what() << "\n";
            return TPR_BAD_ALLOC;

        } catch (const Exception& e) {
            err << "Centralized exception [" << e.code() << "]: " << e.what() << "\n";
            return TPR_UNKNOWN_ERROR;

        } catch (const std::exception& e) {
            err << "Uncentralized exception: " << e.what() << "\n";
            return TPR_UNKNOWN_ERROR;

        } catch (...) {
            err << "Unknown exception\n";
            return TPR_UNKNOWN_ERROR;
        }

        return res;
    }
};



template <>
struct StandartHandler<void> {
    void operator()(std::function<void() noexcept(false)> func) {

        auto err = gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1);

        try {
            func();

        } catch (const std::bad_alloc& e) {
            err << "Bad alloc: "<< e.what() << "\n";

        } catch (const Exception& e) {
            err << "Centralized exception [" << e.code() << "]: " << e.what() << "\n";

        } catch (const std::exception& e) {
            err << "Uncentralized exception: " << e.what() << "\n";

        } catch (...) {
            err << "Unknown exception\n";
        }
    }
};




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
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.registerComponent(componentSize, componentName, pNewComponentId);
        }

        TprResult acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.acquireComponent(componentName, pComponentId);
        }

        TprResult createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntity* pEntityHandle) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.createEntity(componentIdCount, pComponentIds, pEntityHandle);
        }

        void destroyEntity(const TprEntity* entityHandle) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            sceneGraph.destroyEntity(entityHandle);
        }

        TprResult modifyEntityComponentSet(const TprEntity* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) noexcept {
            return TPR_UNKNOWN_ERROR;
        }

        TprResult copyEntityComponentData(const TprEntity* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.copyEntityComponentData(entityHandle, componentId, start, end, componentData);
        }

        TprResult readEntityComponent8bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.readEntityComponent8bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent16bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.readEntityComponent16bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent32bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.readEntityComponent32bit(entityHandle, componentId, offset, data);
        }

        TprResult readEntityComponent64bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.readEntityComponent64bit(entityHandle, componentId, offset, data);
        }

        TprResult writeEntityComponentData(const TprEntity* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.writeEntityComponentData(entityHandle, componentId, componentData, start, end);
        }

        TprResult writeEntityComponent8bit(const TprEntity* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.writeEntityComponent8bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent16bit(const TprEntity* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.writeEntityComponent16bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent32bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.writeEntityComponent32bit(entityHandle, componentId, data, offset);
        }

        TprResult writeEntityComponent64bit(const TprEntity* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) noexcept {
            SceneGraph& sceneGraph = gGetServiceLocator()->get<SceneGraph>();
            return sceneGraph.writeEntityComponent64bit(entityHandle, componentId, data, offset);
        }

    }

    namespace geo {
        TprResult parseAsset(const TprAssetParseInfo* pInfo, TprAsset* pAsset) noexcept {
            AssetStore& store = gGetServiceLocator()->get<AssetStore>();
            auto exp = store.parseAsset(pInfo);
            if (exp.has_value()) {
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

    namespace vfs {

        TprResult openPathResource(TprContext context, const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pResource) return TPR_INVALID_VALUE;
                if (!path) return TPR_INVALID_VALUE;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.openResource(context, path, flags, alignment);
                if (exp.has_value()) {
                    *pResource = exp.value();
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult openReferenceResource(TprContext context, char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pResource) return TPR_INVALID_VALUE;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.openResource(context, reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags);
                if (exp.has_value()) {
                    *pResource = exp.value();
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult openEmptyResource(TprContext context, uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pResource) return TPR_INVALID_VALUE;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.openResource(context, size, flags, alignment);
                if (exp.has_value()) {
                    *pResource = exp.value();
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult openCapabilityResource(TprContext context, TprResource resource, TprOpenCapabilityResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pResource) return TPR_INVALID_VALUE;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.openResource(context, resource, flags, protectFlags);
                if (exp.has_value()) {
                    *pResource = exp.value();
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept {
            return StandartHandler<TprResult>()([=]() {
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                return resReg.resizeResource(resource, newSize);
            });
        }

        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pSize) return TPR_INVALID_VALUE;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.sizeofResource(resource);
                if (exp.has_value()) {
                    *pSize = exp.value();
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pData) return TPR_UNKNOWN_ERROR;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.getResourceRawDataPointer(resource);
                if (exp.has_value()) {
                    *pData = reinterpret_cast<char*>(exp.value());
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        TprResult getResourceConstPointer(TprResource resource, const char** pData) noexcept {
            return StandartHandler<TprResult>()([=]() {
                if (!pData) return TPR_UNKNOWN_ERROR;
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                auto exp = resReg.getResourceConstPointer(resource);
                if (exp.has_value()) {
                    *pData = reinterpret_cast<const char*>(exp.value());
                    return TPR_SUCCESS;
                } else {
                    return exp.error();
                }
            });
        }

        void closeResource(TprResource resource) noexcept {
            StandartHandler<void>()([=]() {
                ResourceRegistry& resReg = gGetServiceLocator()->get<ResourceRegistry>();
                resReg.closeResource(resource);
            });
        }

    }

    namespace ctx {

        TprResult createRootContext(TprContext* pContext) noexcept {
            return StandartHandler<TprResult>()([=]() {
                ContextManager& ctxMan = gGetServiceLocator()->get<ContextManager>();
                auto exp = ctxMan.createContext();
                if (!exp.has_value()) return exp.error();
                *pContext = exp.value();
                return TPR_SUCCESS;
            });
        }

        TprResult createContext(TprContext parentContext, TprContext* pContext) noexcept {
            return StandartHandler<TprResult>()([=]() {
                ContextManager& ctxMan = gGetServiceLocator()->get<ContextManager>();
                auto exp = ctxMan.createContext(parentContext);
                if (!exp.has_value()) return exp.error();
                *pContext = exp.value();
                return TPR_SUCCESS;
            });
        }

        void destroyContext(TprContext context) noexcept {
            StandartHandler<void>()([=]() {
                ContextManager& ctxMan = gGetServiceLocator()->get<ContextManager>();
                ctxMan.destroyContext(context);
            });
        }

    }

}
