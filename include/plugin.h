
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_


#include "plugin_core.h"
#include <stdint.h>



#ifdef __cplusplus
    #define __noexcept noexcept(true)
#else
    #define __noexcept
#endif



typedef struct TprEngineAPI {

    // log functions, allow to log the same way the engine logs (i. e. to the same log file or to stdout and with the same formatting)
    // plugin should use log functions to log
    void(*log)(TprLogLevel logLevel, const char* message) __noexcept;
    void(*logInfo)(const char* message) __noexcept;
    void(*logWarn)(const char* message) __noexcept;
    void(*logError)(const char* message) __noexcept;
    void(*logDebug)(const char* message) __noexcept;
    void(*logTrace)(const char* message) __noexcept;

    void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) __noexcept;
    void(*logInfoStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logWarnStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logErrorStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logDebugStyled)(TprLogStyle logStyle, const char* message) __noexcept;
    void(*logTraceStyled)(TprLogStyle logStyle, const char* message) __noexcept;

    struct {

        /// @brief declares a new component id.
        /// @details new id is a valid id and can be passed to other functions.
        TprResult(*registerComponent)(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) __noexcept;

        TprResult(*acquireComponent)(const char* componentName, uint32_t* pComponentId) __noexcept;

        /// @brief creates an entity.
        /// @param pEntityId pointer to uint32_t where created entity id will be stored.
        /// @param componentIdsCount count of components that created entity must have.
        /// @param pComponentIds array with component ids that created entity must have. All ids must be valid ids created using declareComponentId.
        /// @returns TPR_SUCCESS on success
        TprResult(*createEntity)(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntityHandle* pEntityHandle) __noexcept;

        void(*destroyEntity)(const TprEntityHandle* entityHandle) __noexcept;

        TprResult(*modifyEntityComponentSet)(const TprEntityHandle* entityHandle, uint32_t newComponentIdCount, const uint32_t* pNewComponentIds) __noexcept;

        TprResult(*copyEntityComponentData)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) __noexcept;
        TprResult(*readEntityComponent8bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) __noexcept;
        TprResult(*readEntityComponent16bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) __noexcept;
        TprResult(*readEntityComponent32bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) __noexcept;
        TprResult(*readEntityComponent64bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) __noexcept;

        TprResult(*writeEntityComponentData)(const TprEntityHandle* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) __noexcept;
        TprResult(*writeEntityComponent8bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) __noexcept;
        TprResult(*writeEntityComponent16bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) __noexcept;
        TprResult(*writeEntityComponent32bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) __noexcept;
        TprResult(*writeEntityComponent64bit)(const TprEntityHandle* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) __noexcept;

    } scene;

    struct {

        TprResult(*parseAsset)(const TprAssetParseInfo* pParseInfo, TprAsset* pAsset) __noexcept;

    } geo;

    struct {

        TprResult(*growEntityDrawOArray)(TprOArrayEntityDrawDesc oarray, uint32_t count, TprEntityDrawDesc** pStart) __noexcept;
        TprResult(*endEntityDrawOArray)(TprOArrayEntityDrawDesc oarray, uint32_t actualSize) __noexcept;
        TprResult(*sizeofEntityDrawOArray)(TprOArrayEntityDrawDesc oarray, uint32_t* pSize) __noexcept;

    } render;

    struct {

        TprResult(*openPathResource)(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) __noexcept;
        TprResult(*openRefResource)(char* begin, char* end, TprOpenRefResourceFlags flags, TprResource* pResource) __noexcept;
        TprResult(*openEmptyResource)(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) __noexcept;
        TprResult(*openCapabilityResource)(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) __noexcept;

        TprResult(*openPathResourceLifetimed)(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, const TprLifetime* pLifetime, TprResource* pResource) __noexcept;
        TprResult(*openRefResourceLifetimed)(char* begin, char* end, TprOpenRefResourceFlags flags, const TprLifetime* pLifetime, TprResource* pResource) __noexcept;
        TprResult(*openEmptyResourceLifetimed)(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, const TprLifetime* pLifetime, TprResource* pResource) __noexcept;
        TprResult(*openCapabilityResourceLifetimed)(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, const TprLifetime* pLifetime, TprResource* pResource) __noexcept;

        TprResult(*resetResourceLifetime)(TprResource resource, const TprLifetime* lifetime) __noexcept;
        TprResult(*resizeResource)(TprResource resource, uint64_t newSize) __noexcept;
        TprResult(*sizeofResource)(TprResource resource, uint64_t* pSize) __noexcept;
        TprResult(*getResourceRawRWDataPointer)(TprResource resource, char** pData) __noexcept;
        TprResult(*getResourceRawRODataPointer)(TprResource resource, const char** pData) __noexcept;

        void(*closeResource)(TprResource resource) __noexcept;

    } vfs;

    struct {

        TprResult(*openWindow)(TprWindow* pHandle, const TprWindowCreateInfo* pCreateInfo) __noexcept;
        void(*closeWindow)(TprWindow handle) __noexcept;

    } wm;

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
int32_t tprHookInit(void** ctx, const TprEngineAPI* engineApi) __noexcept;

/// @brief shutdowns the plugin. Required.
/// @details is called by the engine at the end of it's lifecycle or if any previous plugin's hook before returned negative exit code
void tprHookShutdown(void* ctx) __noexcept;

/// @brief returns required by plugin hooks. Required.
/// @param hooks a pointer to an array given by engine.
/// @returns number of hooks.
/// @details is called by the engine twice:
/// 1st time with *hooks set to NULL to obtain the number of hooks;
/// 2nd time with *hooks pointing to an array of size, obtained with the first call.
/// Returned hooks must not have any required hook listed
uint32_t tprHookGetNeededHooks(void* ctx, TprHook* hooks) __noexcept;


// ----------- optional hooks -----------
// Theese hooks are optional.
// However, all hooks passed in tprHookGetNeededHooks must be present as a valid symbol

/// @brief is being called at the start of every frame.
/// @returns exit code
int32_t tprHookUpdatePerFrame(void* ctx) __noexcept;

/// @brief is being called every frame right before rendering started
int32_t tprHookRenderBegin(void* ctx) __noexcept;

/// @brief gives ids of every entity to be drawn
void tprHookGetEntityDrawDescriptors(void* ctx, TprOArrayEntityDrawDesc oarray) __noexcept;



#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

