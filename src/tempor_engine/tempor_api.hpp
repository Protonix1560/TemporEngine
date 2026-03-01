

#ifndef TEMPOR_ENGINE_TEMPOR_API_HPP_
#define TEMPOR_ENGINE_TEMPOR_API_HPP_


#include "plugin_core.h"




namespace tpr_api {
    
    namespace log {
        void log(TprLogLevel logLevel, const char* message) noexcept;
        void logInfo(const char* message) noexcept;
        void logWarn(const char* message) noexcept;
        void logError(const char* message) noexcept;
        void logDebug(const char* message) noexcept;
        void logTrace(const char* message) noexcept;
        void logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept;
        void logInfoStyled(TprLogStyle logStyle, const char* message) noexcept;
        void logWarnStyled(TprLogStyle logStyle, const char* message) noexcept;
        void logErrorStyled(TprLogStyle logStyle, const char* message) noexcept;
        void logDebugStyled(TprLogStyle logStyle, const char* message) noexcept;
        void logTraceStyled(TprLogStyle logStyle, const char* message) noexcept;
    }

    namespace scene {
        TprResult registerComponent(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) noexcept;
        TprResult acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept;
        TprResult createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntity* pEntityHandle) noexcept;
        void destroyEntity(const TprEntity* entityHandle) noexcept;
        TprResult modifyEntityComponentSet(const TprEntity* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) noexcept;
        TprResult copyEntityComponentData(const TprEntity* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept;
        TprResult readEntityComponent8bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept;
        TprResult readEntityComponent16bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept;
        TprResult readEntityComponent32bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept;
        TprResult readEntityComponent64bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept;
        TprResult writeEntityComponentData(const TprEntity* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept;
        TprResult writeEntityComponent8bit(const TprEntity* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent16bit(const TprEntity* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent32bit(const TprEntity* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent64bit(const TprEntity* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) noexcept;
    }

    namespace geo {
        TprResult parseAsset(const TprAssetParseInfo* pInfo, TprAsset* pAsset) noexcept;
    }

    namespace render {
        TprResult growEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t count, TprEntityDrawDesc** start) noexcept;
        TprResult endEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t actualSize) noexcept;
        TprResult sizeofEntityDrawOArray(TprOArrayEntityDrawDesc oarray, uint32_t* size) noexcept;
    }

    namespace vfs {
        TprResult openPathResource(TprContext context, const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept;
        TprResult openReferenceResource(TprContext context, char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept;
        TprResult openEmptyResource(TprContext context, uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept;
        TprResult openCapabilityResource(TprContext context, TprResource resource, TprOpenCapabilityResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept;
        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept;
        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept;
        TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept;
        TprResult getResourceConstPointer(TprResource resource, const char** pData) noexcept;
    }

    namespace wm {
        TprResult openWindow(TprWindow* pHandle, const TprWindowCreateInfo* createInfo) noexcept;
        void closeWindow(TprWindow handle) noexcept;
    }

    namespace ctx {
        TprResult createRootContext(TprContext* pContext) noexcept;
        TprResult createContext(TprContext parentContext, TprContext* pContext) noexcept;
        void destroyContext(TprContext context) noexcept;
    }

}



#endif  // TEMPOR_ENGINE_TEMPOR_API_HPP_

