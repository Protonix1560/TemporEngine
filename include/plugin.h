
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

        TprResult(*createComponent)(uint32_t componentSize, TprComponent* pComponent) NOEXCEPT_T;
        void(*destroyComponent)(TprComponent component) NOEXCEPT_T;

        TprResult(*spawnEntity)(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) NOEXCEPT_T;
        void(*killEntity)(TprEntity entity) NOEXCEPT_T;

        TprResult(*modifyEntityComponentSet)(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) NOEXCEPT_T;

        TprResult(*copyEntityComponentData)(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) NOEXCEPT_T;
        TprResult(*readEntityComponent8bit)(TprEntity entity, TprComponent component, uint32_t offset, uint8_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent16bit)(TprEntity entity, TprComponent component, uint32_t offset, uint16_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent32bit)(TprEntity entity, TprComponent component, uint32_t offset, uint32_t* data) NOEXCEPT_T;
        TprResult(*readEntityComponent64bit)(TprEntity entity, TprComponent component, uint32_t offset, uint64_t* data) NOEXCEPT_T;

        TprResult(*writeEntityComponentData)(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) NOEXCEPT_T;
        TprResult(*writeEntityComponent8bit)(TprEntity entity, TprComponent component, uint8_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent16bit)(TprEntity entity, TprComponent component, uint16_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent32bit)(TprEntity entity, TprComponent component, uint32_t data, uint32_t offset) NOEXCEPT_T;
        TprResult(*writeEntityComponent64bit)(TprEntity entity, TprComponent component, uint64_t data, uint32_t offset) NOEXCEPT_T;

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

        TprResult(*openWindow)(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) NOEXCEPT_T;
        void(*closeWindow)(TprWindow windo) NOEXCEPT_T;

    } *wm;

    struct Input {

        TprResult(*createAction)(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) NOEXCEPT_T;
        void(*destroyAction)(TprAction action) NOEXCEPT_T;
        TprResult(*getActionState)(TprAction action, TprActionState* pState) NOEXCEPT_T;
        TprResult(*getInputElementVector)(TprWindow window, TprInputElement element, TprInputElementVector* pVector) NOEXCEPT_T;

    } *input;

} TprEngineAPI;



typedef struct TprPluginCallbacks {

    int32_t(*init)(void** ctx, const TprEngineAPI* pEngineAPI) NOEXCEPT_T;
    void(*preShutdown)(void* ctx) NOEXCEPT_T;
    void(*shutdown)(void* ctx) NOEXCEPT_T;
    int32_t(*updatePerFrame)(void* ctx) NOEXCEPT_T;

} TprPluginCallbacks;



#ifdef __cplusplus
extern "C" {
#endif


int32_t getPluginCallbacks(TprPluginCallbacks* pCallbacks) NOEXCEPT;


#ifdef __cplusplus
}
#endif



#endif  // TEMPOR_PLUGIN_H_

