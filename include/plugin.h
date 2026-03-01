
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_


#include "plugin_core.h"
#include <stdint.h>



#ifdef __cplusplus
    #define NOEXCEPT noexcept(true)
#else
    #define NOEXCEPT
#endif



typedef struct TprEngineAPI {

    struct Log {

        // log functions, allow to log the same way the engine logs (i. e. to the same log file or to stdout and with the same formatting)
        // plugin should use log functions to log
        void(*log)(TprLogLevel logLevel, const char* message) NOEXCEPT;
        void(*info)(const char* message) NOEXCEPT;
        void(*warn)(const char* message) NOEXCEPT;
        void(*error)(const char* message) NOEXCEPT;
        void(*debug)(const char* message) NOEXCEPT;
        void(*trace)(const char* message) NOEXCEPT;

        void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) NOEXCEPT;
        void(*infoStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT;
        void(*warnStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT;
        void(*errorStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT;
        void(*debugStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT;
        void(*traceStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT;

    } *log;

    struct Scene {

        /// @brief declares a new component id.
        /// @details new id is a valid id and can be passed to other functions.
        TprResult(*registerComponent)(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) NOEXCEPT;

        TprResult(*acquireComponent)(const char* componentName, uint32_t* pComponentId) NOEXCEPT;

        /// @brief creates an entity.
        /// @param pEntityId pointer to uint32_t where created entity id will be stored.
        /// @param componentIdsCount count of components that created entity must have.
        /// @param pComponentIds array with component ids that created entity must have. All ids must be valid ids created using declareComponentId.
        /// @returns TPR_SUCCESS on success
        TprResult(*createEntity)(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntity* pEntityHandle) NOEXCEPT;

        void(*destroyEntity)(const TprEntity* entityHandle) NOEXCEPT;

        TprResult(*modifyEntityComponentSet)(const TprEntity* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) NOEXCEPT;

        TprResult(*copyEntityComponentData)(const TprEntity* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) NOEXCEPT;
        TprResult(*readEntityComponent8bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) NOEXCEPT;
        TprResult(*readEntityComponent16bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) NOEXCEPT;
        TprResult(*readEntityComponent32bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) NOEXCEPT;
        TprResult(*readEntityComponent64bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) NOEXCEPT;

        TprResult(*writeEntityComponentData)(const TprEntity* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) NOEXCEPT;
        TprResult(*writeEntityComponent8bit)(const TprEntity* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) NOEXCEPT;
        TprResult(*writeEntityComponent16bit)(const TprEntity* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) NOEXCEPT;
        TprResult(*writeEntityComponent32bit)(const TprEntity* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) NOEXCEPT;
        TprResult(*writeEntityComponent64bit)(const TprEntity* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) NOEXCEPT;

    } *scene;

    struct Geo {

        TprResult(*parseAsset)(const TprAssetParseInfo* pParseInfo, TprAsset* pAsset) NOEXCEPT;

    } *geo;

    struct VFS {

        TprResult(*openPathResource)(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT;
        TprResult(*openReferenceResource)(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) NOEXCEPT;
        TprResult(*openEmptyResource)(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT;
        TprResult(*openCapabilityResource)(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) NOEXCEPT;

        TprResult(*resizeResource)(TprResource resource, uint64_t newSize) NOEXCEPT;
        TprResult(*sizeofResource)(TprResource resource, uint64_t* pSize) NOEXCEPT;
        TprResult(*getResourceRawDataPointer)(TprResource resource, char** pData) NOEXCEPT;
        TprResult(*getResourceConstPointer)(TprResource resource, const char** pData) NOEXCEPT;

        void(*closeResource)(TprResource resource) NOEXCEPT;

    } *vfs;

    struct WM {

        TprResult(*openWindow)(TprWindow* pHandle, const TprWindowCreateInfo* pCreateInfo) NOEXCEPT;
        void(*closeWindow)(TprWindow handle) NOEXCEPT;

    } *wm;

} TprEngineAPI;



#ifdef __cplusplus
extern "C" {
#endif



// ----------- required hooks -----------
// Theese hooks must be present in every plagin as a valid symbol

/// @brief initializes the plugin. Required.
/// @param ctx a pointer to a pointer to plugin's own data.
/// @returns exit code.
/// @details is called by the engine the first.
/// *ctx should be set to a pointer to plugin's data (e. g. to a struct where all of the data is stored) or NULL if there is none.
/// In case of negative exit code tprHookShutdown will not be called
int32_t tprHookInit(void** ctx, const TprEngineAPI* engineApi) NOEXCEPT;

/// @brief shutdowns the plugin. Required.
/// @details is called by the engine at the end of it's lifecycle or if any previous plugin's hook before returned negative exit code
void tprHookShutdown(void* ctx) NOEXCEPT;

/// @brief returns required by plugin hooks. Required.
/// @param hooks a pointer to an array given by engine.
/// @returns number of hooks.
/// @details is called by the engine twice:
/// 1st time with *hooks set to NULL to obtain the number of hooks;
/// 2nd time with *hooks pointing to an array of size, obtained with the first call.
/// Returned hooks must not have any required hook listed
uint32_t tprHookGetNeededHooks(void* ctx, TprHook* hooks) NOEXCEPT;


// ----------- optional hooks -----------
// Theese hooks are optional.
// However, all hooks passed in tprHookGetNeededHooks must be present as a valid symbol

/// @brief is being called at the start of every frame.
/// @returns exit code
int32_t tprHookUpdatePerFrame(void* ctx) NOEXCEPT;

/// @brief is being called every frame right before rendering started
int32_t tprHookRenderBegin(void* ctx) NOEXCEPT;

/// @brief gives ids of every entity to be drawn
void tprHookGetEntityDrawDescriptors(void* ctx, TprOArrayEntityDrawDesc oarray) NOEXCEPT;



#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

