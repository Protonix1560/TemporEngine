
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_

#include "plugin_core.h"
#include <stdint.h>


typedef struct TprEngineAPI {

    struct Out {

        void(*log)(TprLogLevel logLevel, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*info)(const char* message) _TPR_NOEXCEPT_ATTR;
        void(*warn)(const char* message) _TPR_NOEXCEPT_ATTR;
        void(*error)(const char* message) _TPR_NOEXCEPT_ATTR;
        void(*debug)(const char* message) _TPR_NOEXCEPT_ATTR;
        void(*trace)(const char* message) _TPR_NOEXCEPT_ATTR;

        void(*logStyled)(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*infoStyled)(TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*warnStyled)(TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*errorStyled)(TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*debugStyled)(TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;
        void(*traceStyled)(TprLogStyle logStyle, const char* message) _TPR_NOEXCEPT_ATTR;

        TprResult(*writeMachineData)(const char* pData, uint32_t size) _TPR_NOEXCEPT_ATTR;

    } *out;

    struct Scene {

        TprResult(*createComponent)(uint32_t componentSize, TprComponent* pComponent) _TPR_NOEXCEPT_ATTR;
        void(*destroyComponent)(TprComponent component) _TPR_NOEXCEPT_ATTR;

        TprResult(*spawnEntity)(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) _TPR_NOEXCEPT_ATTR;
        void(*killEntity)(TprEntity entity) _TPR_NOEXCEPT_ATTR;

        TprResult(*modifyEntityComponentSet)(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) _TPR_NOEXCEPT_ATTR;

        TprResult(*copyEntityComponentData)(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) _TPR_NOEXCEPT_ATTR;
        TprResult(*writeEntityComponentData)(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) _TPR_NOEXCEPT_ATTR;

        TprResult(*getComponentChunkHandles)(TprComponent component, TprFile resource) _TPR_NOEXCEPT_ATTR;
        uint32_t(*getComponentChunkMaxElementCount)() _TPR_NOEXCEPT_ATTR;
        TprResult(*getComponentChunkElementCount)(TprComponentChunk chunk, uint32_t* pCount) _TPR_NOEXCEPT_ATTR;
        TprResult(*getComponentChunkVersion)(TprComponentChunk chunk, uint32_t* pVersion) _TPR_NOEXCEPT_ATTR;
        TprResult(*copyComponentChunkData)(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*writeComponentChunkData)(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) _TPR_NOEXCEPT_ATTR;

    } *scene;

    struct FS {

        TprResult(*openFile)(const char* path, TprOpenFileFlags flags, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*createMemoryFile)(TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*forkFile)(TprFile file, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*createCapability)(TprFile file, TprFileCapabilityFlags mask, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        void(*closeFile)(TprFile file) _TPR_NOEXCEPT_ATTR;

        TprResult(*seek)(TprFile file, int32_t offset, TprSeekWhence whence) _TPR_NOEXCEPT_ATTR;
        TprResult(*tell)(TprFile file, uint32_t* pPos) _TPR_NOEXCEPT_ATTR;
        TprResult(*read)(TprFile file, uint32_t n, char* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*readAt)(TprFile file, uint32_t pos, uint32_t n, char* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*resize)(TprFile file, uint32_t newSize) _TPR_NOEXCEPT_ATTR;
        TprResult(*write)(TprFile file, uint32_t n, const char* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*writeAt)(TprFile file, uint32_t pos, uint32_t n, const char* pData) _TPR_NOEXCEPT_ATTR;

        TprResult(*pathType)(const char* path, TprPathType* pType) _TPR_NOEXCEPT_ATTR;
        TprResult(*createDirectory)(const char* path, TprCreateDirectoryFlags flags) _TPR_NOEXCEPT_ATTR;
        TprResult(*touchFile)(const char* path, TprTouchFileFlags flags) _TPR_NOEXCEPT_ATTR;
        TprResult(*remove)(const char* path) _TPR_NOEXCEPT_ATTR;
        TprResult(*move)(const char* path, const char* newPath) _TPR_NOEXCEPT_ATTR;

    } *fs;

    struct Win {

        TprResult(*openWindow)(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) _TPR_NOEXCEPT_ATTR;
        void(*closeWindow)(TprWindow window) _TPR_NOEXCEPT_ATTR;

    } *win;

    struct Input {

        TprResult(*createAction)(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) _TPR_NOEXCEPT_ATTR;
        void(*destroyAction)(TprAction action) _TPR_NOEXCEPT_ATTR;
        TprResult(*getActionState)(TprAction action, TprActionState* pState) _TPR_NOEXCEPT_ATTR;
        TprResult(*getInputElementVector)(TprWindow window, TprInputElement element, TprInputElementVector* pVector) _TPR_NOEXCEPT_ATTR;

    } *input;

    struct Geo {

        TprResult(*createMesh)(const TprMeshCreateInfo* pCreateInfo, TprMesh* pMesh) _TPR_NOEXCEPT_ATTR;
        TprResult(*loadMesh)(TprMesh mesh, const TprMeshLoadInfo* pLoadInfo) _TPR_NOEXCEPT_ATTR;
        void(*unloadMesh)(TprMesh mesh) _TPR_NOEXCEPT_ATTR;
        void(*destroyMesh)(TprMesh mesh) _TPR_NOEXCEPT_ATTR;

    } *geo;

    struct Conf {

        TprResult(*getRootSetting)(TprSetting* pSetting) _TPR_NOEXCEPT_ATTR;

        TprResult(*createSetting)(TprSetting baseSetting, const char* name, TprSetting* pSetting) _TPR_NOEXCEPT_ATTR;
        TprResult(*readSetting)(TprSetting baseSetting, const char* name, TprSetting* pSetting) _TPR_NOEXCEPT_ATTR;
        void(*destroySetting)(TprSetting pSetting) _TPR_NOEXCEPT_ATTR;

        TprResult(*getSettingType)(TprSetting setting, TprSettingType* pType) _TPR_NOEXCEPT_ATTR;
        TprResult(*getSettingDouble)(TprSetting setting, double* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*getSettingInteger)(TprSetting setting, int64_t* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*getSettingBool)(TprSetting setting, TprBool8* pData) _TPR_NOEXCEPT_ATTR;

        double(*getSettingDoubleOr)(TprSetting setting, double fallback) _TPR_NOEXCEPT_ATTR;
        int64_t(*getSettingIntegerOr)(TprSetting setting, int64_t fallback) _TPR_NOEXCEPT_ATTR;
        TprBool8(*getSettingBoolOr)(TprSetting setting, TprBool8 fallback) _TPR_NOEXCEPT_ATTR;

        TprResult(*getSettingStringSize)(TprSetting setting, uint32_t* pSize) _TPR_NOEXCEPT_ATTR;
        TprResult(*copySettingString)(TprSetting setting, char* pData) _TPR_NOEXCEPT_ATTR;

        TprResult(*setSettingDouble)(TprSetting setting, double data) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingInteger)(TprSetting setting, int64_t data) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingBool)(TprSetting setting, TprBool8 data) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingString)(TprSetting setting, const char* pData) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingNull)(TprSetting setting) _TPR_NOEXCEPT_ATTR;
        TprResult(*unsetSetting)(TprSetting setting) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingStruct)(TprSetting setting) _TPR_NOEXCEPT_ATTR;
        TprResult(*setSettingArray)(TprSetting setting) _TPR_NOEXCEPT_ATTR;

        TprResult(*getSettingArraySize)(TprSetting setting, uint32_t* pSize) _TPR_NOEXCEPT_ATTR;
        TprResult(*getSettingArrayElement)(TprSetting setting, uint32_t index, TprSetting* pElement) _TPR_NOEXCEPT_ATTR;
        TprResult(*resizeSettingArray)(TprSetting setting, uint32_t size) _TPR_NOEXCEPT_ATTR;

    } *conf;

    struct Render {

        TprResult(*createDepthDomain)(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) _TPR_NOEXCEPT_ATTR;
        void(*destroyDepthDomain)(TprDepthDomain domain) _TPR_NOEXCEPT_ATTR;

        TprResult(*createRenderTarget)(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) _TPR_NOEXCEPT_ATTR;
        void(*destroyRenderTarget)(TprRenderTarget target) _TPR_NOEXCEPT_ATTR;

        TprComponent(*getComponentRenderable)() _TPR_NOEXCEPT_ATTR;

        TprResult(*createObjectImage)(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) _TPR_NOEXCEPT_ATTR;
        void(*destroyObjectImage)(TprObjectImage image) _TPR_NOEXCEPT_ATTR;

    } *render;

    struct Thread {

        TprResult(*createJob)(const TprJobCreateInfo* pInfo, TprJob* pJob) _TPR_NOEXCEPT_ATTR;
        TprResult(*createDetachedJob)(const TprJobCreateInfo* pInfo) _TPR_NOEXCEPT_ATTR;
        TprResult(*jobFinished)(TprJob job, TprBool8* pData) _TPR_NOEXCEPT_ATTR;
        void(*joinJob)(TprJob job) _TPR_NOEXCEPT_ATTR;

    } *thread;

} TprEngineAPI;


typedef struct TprPluginCallbacks {

    int32_t(*init)(void** ctx, const TprEngineAPI* pEngineAPI) _TPR_NOEXCEPT_ATTR;
    void(*preShutdown)(void* ctx) _TPR_NOEXCEPT_ATTR;
    void(*shutdown)(void* ctx) _TPR_NOEXCEPT_ATTR;
    int32_t(*updatePerFrame)(void* ctx) _TPR_NOEXCEPT_ATTR;

} TprPluginCallbacks;


#ifdef __cplusplus
extern "C" {
#endif

int32_t getPluginCallbacks(TprPluginCallbacks* pCallbacks) _TPR_NOEXCEPT;

#ifdef __cplusplus
}
#endif


#endif  // TEMPOR_PLUGIN_H_

