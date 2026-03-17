
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_


#include "plugin_core.h"
#include <stdint.h>



#if defined(__cplusplus) && __cplusplus >= 201703L
    #define NOEXCEPT_T noexcept
#else
    #define NOEXCEPT_T
#endif

#if defined(__cplusplus)
    #define NOEXCEPT noexcept
#else
    #define NOEXCEPT
#endif



typedef struct TprEngineAPI {

    struct Log {

        // log functions, allow to log the same way the engine logs (i. e. to the same log file or to stdout and with the same formatting)
        // plugin should use log functions to log
        void(*log)(TprLogLevel logLevel, const char* message) NOEXCEPT_T;
        void(*info)(const char* message) NOEXCEPT_T;
        void(*warn)(const char* message) NOEXCEPT_T;
        void(*error)(const char* message) NOEXCEPT_T;
        void(*debug)(const char* message) NOEXCEPT_T;
        void(*trace)(const char* message) NOEXCEPT_T;

        void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) NOEXCEPT_T;
        void(*infoStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_T;
        void(*warnStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_T;
        void(*errorStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_T;
        void(*debugStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_T;
        void(*traceStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_T;

    } *log;

    struct Scene {

        /// @brief declares a new component id.
        /// @details new id is a valid id and can be passed to other functions.
        TprResult(*registerComponent)(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) NOEXCEPT_T;

        TprResult(*acquireComponent)(const char* componentName, uint32_t* pComponentId) NOEXCEPT_T;

        /// @brief creates an entity.
        /// @param pEntityId pointer to uint32_t where created entity id will be stored.
        /// @param componentIdsCount count of components that created entity must have.
        /// @param pComponentIds array with component ids that created entity must have. All ids must be valid ids created using declareComponentId.
        /// @returns TPR_SUCCESS on success
        TprResult(*createEntity)(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntity* pEntityHandle) NOEXCEPT_T;

        void(*destroyEntity)(const TprEntity* entityHandle) NOEXCEPT_T;

        TprResult(*modifyEntityComponentSet)(const TprEntity* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) NOEXCEPT_T;

        TprResult(*copyEntityComponentData)(const TprEntity* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) NOEXCEPT_T;
        TprResult(*readEntityComponent8bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent16bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent32bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent64bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) NOEXCEPT_T;

        TprResult(*writeEntityComponentData)(const TprEntity* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) NOEXCEPT_T;
        TprResult(*writeEntityComponent8bit)(const TprEntity* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent16bit)(const TprEntity* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent32bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent64bit)(const TprEntity* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) NOEXCEPT_T;

    } *scene;

    struct Geo {

        TprResult(*parseAsset)(const TprAssetParseInfo* pParseInfo, TprAsset* pAsset) NOEXCEPT_T;

    } *geo;

    struct VFS {

        TprResult(*openPathResource)(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT_T;
        TprResult(*openReferenceResource)(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) NOEXCEPT_T;
        TprResult(*openEmptyResource)(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT_T;
        TprResult(*openCapabilityResource)(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) NOEXCEPT_T;

        TprResult(*resizeResource)(TprResource resource, uint64_t newSize) NOEXCEPT_T;
        TprResult(*sizeofResource)(TprResource resource, uint64_t* pSize) NOEXCEPT_T;
        TprResult(*getResourceRawDataPointer)(TprResource resource, char** pData) NOEXCEPT_T;
        TprResult(*getResourceConstPointer)(TprResource resource, const char** pData) NOEXCEPT_T;

        void(*closeResource)(TprResource resource) NOEXCEPT_T;

    } *vfs;

    struct WM {

        TprResult(*openWindow)(TprWindow* pHandle, const TprWindowCreateInfo* pCreateInfo) NOEXCEPT_T;
        void(*closeWindow)(TprWindow handle) NOEXCEPT_T;

    } *wm;

} TprEngineAPI;



typedef struct TprPluginCallbacks {

    int32_t(*init)(void** ctx, const TprEngineAPI* pEngineAPI) NOEXCEPT_T;
    void(*preShutdown)(void* ctx) NOEXCEPT_T;
    void(*shutdown)(void* ctx) NOEXCEPT_T;

} TprPluginCallbacks;



#ifdef __cplusplus
extern "C" {
#endif


int32_t getPluginCallbacks(TprPluginCallbacks* pCallbacks) NOEXCEPT;


#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

