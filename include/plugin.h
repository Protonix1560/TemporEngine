
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_

#include "plugin_core.h"
#include <stdint.h>


typedef struct TprEngineAPI {

    struct Log {

        void(*log)(TprLogLevel logLevel, const char* message) NOEXCEPT_ATTR;
        void(*info)(const char* message) NOEXCEPT_ATTR;
        void(*warn)(const char* message) NOEXCEPT_ATTR;
        void(*error)(const char* message) NOEXCEPT_ATTR;
        void(*debug)(const char* message) NOEXCEPT_ATTR;
        void(*trace)(const char* message) NOEXCEPT_ATTR;

        void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;
        void(*infoStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;
        void(*warnStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;
        void(*errorStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;
        void(*debugStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;
        void(*traceStyled)(TprLogStyle logStyle, const char* message) NOEXCEPT_ATTR;

    } *log;

    struct Scene {

        TprResult(*createComponent)(uint32_t componentSize, TprComponent* pComponent) NOEXCEPT_ATTR;
        void(*destroyComponent)(TprComponent component) NOEXCEPT_ATTR;

        TprResult(*spawnEntity)(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) NOEXCEPT_ATTR;
        void(*killEntity)(TprEntity entity) NOEXCEPT_ATTR;

        TprResult(*modifyEntityComponentSet)(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) NOEXCEPT_ATTR;

        TprResult(*copyEntityComponentData)(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) NOEXCEPT_ATTR;
        TprResult(*writeEntityComponentData)(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) NOEXCEPT_ATTR;

        TprResult(*getComponentChunkHandles)(TprComponent component, TprResource resource) NOEXCEPT_ATTR;
        uint32_t(*getComponentChunkMaxElementCount)() NOEXCEPT_ATTR;
        TprResult(*getComponentChunkElementCount)(TprComponentChunk chunk, uint32_t* pCount) NOEXCEPT_ATTR;
        TprResult(*getComponentChunkVersion)(TprComponentChunk chunk, uint32_t* pVersion) NOEXCEPT_ATTR;
        TprResult(*copyComponentChunkData)(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) NOEXCEPT_ATTR;
        TprResult(*writeComponentChunkData)(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) NOEXCEPT_ATTR;

    } *scene;

    struct VFS {

        TprResult(*openPathResource)(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT_ATTR;
        TprResult(*openReferenceResource)(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) NOEXCEPT_ATTR;
        TprResult(*openEmptyResource)(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) NOEXCEPT_ATTR;
        TprResult(*openCapabilityResource)(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) NOEXCEPT_ATTR;

        TprResult(*resizeResource)(TprResource resource, uint64_t newSize) NOEXCEPT_ATTR;
        TprResult(*sizeofResource)(TprResource resource, uint64_t* pSize) NOEXCEPT_ATTR;
        TprResult(*getResourceRawDataPointer)(TprResource resource, char** pData) NOEXCEPT_ATTR;
        TprResult(*getResourceConstPointer)(TprResource resource, const char** pData) NOEXCEPT_ATTR;

        void(*closeResource)(TprResource resource) NOEXCEPT_ATTR;

    } *vfs;

    struct Win {

        TprResult(*openWindow)(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) NOEXCEPT_ATTR;
        void(*closeWindow)(TprWindow window) NOEXCEPT_ATTR;

    } *win;

    struct Input {

        TprResult(*createAction)(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) NOEXCEPT_ATTR;
        void(*destroyAction)(TprAction action) NOEXCEPT_ATTR;
        TprResult(*getActionState)(TprAction action, TprActionState* pState) NOEXCEPT_ATTR;
        TprResult(*getInputElementVector)(TprWindow window, TprInputElement element, TprInputElementVector* pVector) NOEXCEPT_ATTR;

    } *input;

    struct Geo {

        TprResult(*createMesh)(const TprMeshCreateInfo* pCreateInfo, TprMesh* pMesh) NOEXCEPT_ATTR;
        TprResult(*loadMesh)(TprMesh mesh, const TprMeshLoadInfo* pLoadInfo) NOEXCEPT_ATTR;
        void(*unloadMesh)(TprMesh mesh) NOEXCEPT_ATTR;
        void(*destroyMesh)(TprMesh mesh) NOEXCEPT_ATTR;

    } *geo;

    struct Conf {

        TprResult(*createSetting)(const char* name, TprSetting* pSetting) NOEXCEPT_ATTR;
        void(*destroySetting)(TprSetting pSetting) NOEXCEPT_ATTR;

        TprResult(*getSettingType)(TprSetting setting, TprSettingType* pType) NOEXCEPT_ATTR;
        TprResult(*getSettingDouble)(TprSetting setting, double* pData) NOEXCEPT_ATTR;
        TprResult(*getSettingInteger)(TprSetting setting, int64_t* pData) NOEXCEPT_ATTR;
        TprResult(*getSettingBool)(TprSetting setting, TprBool8* pData) NOEXCEPT_ATTR;

        double(*getSettingDoubleOr)(TprSetting setting, double fallback) NOEXCEPT_ATTR;
        int64_t(*getSettingIntegerOr)(TprSetting setting, int64_t fallback) NOEXCEPT_ATTR;
        TprBool8(*getSettingBoolOr)(TprSetting setting, TprBool8 fallback) NOEXCEPT_ATTR;

        TprResult(*getSettingStringSize)(TprSetting setting, uint32_t* pSize) NOEXCEPT_ATTR;
        TprResult(*copySettingString)(TprSetting setting, char* pData) NOEXCEPT_ATTR;

        TprResult(*setSettingDouble)(TprSetting setting, double data) NOEXCEPT_ATTR;
        TprResult(*setSettingInteger)(TprSetting setting, int64_t data) NOEXCEPT_ATTR;
        TprResult(*setSettingBool)(TprSetting setting, TprBool8 data) NOEXCEPT_ATTR;
        TprResult(*setSettingString)(TprSetting setting, const char* pData) NOEXCEPT_ATTR;
        TprResult(*setSettingNull)(TprSetting setting) NOEXCEPT_ATTR;

    } *conf;

    struct Render {

        TprResult(*createDepthDomain)(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) NOEXCEPT_ATTR;
        void(*destroyDepthDomain)(TprDepthDomain domain) NOEXCEPT_ATTR;

        TprResult(*createRenderTarget)(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) NOEXCEPT_ATTR;
        void(*destroyRenderTarget)(TprRenderTarget target) NOEXCEPT_ATTR;

        TprComponent(*getComponentRenderable)() NOEXCEPT_ATTR;

        TprResult(*createObjectImage)(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) NOEXCEPT_ATTR;
        void(*destroyObjectImage)(TprObjectImage image) NOEXCEPT_ATTR;

    } *render;

    struct Thread {

        TprResult(*createJob)(const TprJobCreateInfo* pInfo, TprJob* pJob) NOEXCEPT_ATTR;
        TprResult(*createDetachedJob)(const TprJobCreateInfo* pInfo) NOEXCEPT_ATTR;
        TprResult(*jobFinished)(TprJob job, TprBool8* pData) NOEXCEPT_ATTR;
        void(*joinJob)(TprJob job) NOEXCEPT_ATTR;

    } *thread;

} TprEngineAPI;


typedef struct TprPluginCallbacks {

    int32_t(*init)(void** ctx, const TprEngineAPI* pEngineAPI) NOEXCEPT_ATTR;
    void(*preShutdown)(void* ctx) NOEXCEPT_ATTR;
    void(*shutdown)(void* ctx) NOEXCEPT_ATTR;
    int32_t(*updatePerFrame)(void* ctx) NOEXCEPT_ATTR;

} TprPluginCallbacks;


#ifdef __cplusplus
extern "C" {
#endif

int32_t getPluginCallbacks(TprPluginCallbacks* pCallbacks) NOEXCEPT;

#ifdef __cplusplus
}
#endif


#endif  // TEMPOR_PLUGIN_H_

