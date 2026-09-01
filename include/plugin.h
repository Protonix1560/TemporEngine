
#ifndef TEMPOR_PLUGIN_H_
#define TEMPOR_PLUGIN_H_

#include "plugin_core.h"
#include <stdint.h>


typedef struct TprEngineAPI {

    struct Output {

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

    struct FileSystem {

        TprResult(*openFile)(const char* path, TprOpenFileFlags flags, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*createMemoryFile)(TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*forkFile)(TprFile file, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
        TprResult(*createFileCapability)(TprFile file, TprFileCapabilityFlags mask, TprFile* pFile) _TPR_NOEXCEPT_ATTR;
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

    struct Windowing {

        TprResult(*openWindow)(const TprWindowCreateInfo* pInfo, TprWindow* pWindow) _TPR_NOEXCEPT_ATTR;
        TprResult(*createWindowCapability)(TprWindow window, TprWindowCapabilityFlags mask, TprWindow* pWindow) _TPR_NOEXCEPT_ATTR;
        void(*closeWindow)(TprWindow window) _TPR_NOEXCEPT_ATTR;
        TprResult(*createAction)(const TprActionCreateInfo* pInfo, TprAction* pAction) _TPR_NOEXCEPT_ATTR;
        TprResult(*createActionCapability)(TprAction action, TprActionCapabilityFlags mask, TprAction* pAction) _TPR_NOEXCEPT_ATTR;
        void(*destroyAction)(TprAction action) _TPR_NOEXCEPT_ATTR;

        TprResult(*getActionsHistorySize)(uint32_t filterCount, const TprAction* pFilters, uint32_t* pSize) _TPR_NOEXCEPT_ATTR;
        TprResult(*copyActionsHistory)(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) _TPR_NOEXCEPT_ATTR;
        TprResult(*getActionState)(TprAction action, TprActionState* pState) _TPR_NOEXCEPT_ATTR;

        TprJob(*getInputUpdateJob)() _TPR_NOEXCEPT_ATTR;

    } *win;

    struct Geometry {

        TprResult(*createMesh)(const TprMeshCreateInfo* pInfo, TprMesh* pMesh) _TPR_NOEXCEPT_ATTR;
        TprResult(*createMeshCapability)(TprMesh mesh, TprMeshCapabilityFlags mask, TprMesh* pMesh) _TPR_NOEXCEPT_ATTR;
        void(*destroyMesh)(TprMesh mesh) _TPR_NOEXCEPT_ATTR;

        TprResult(*requireMeshLoaded)(TprMesh mesh) _TPR_NOEXCEPT_ATTR;
        TprResult(*unrequireMeshLoaded)(TprMesh mesh) _TPR_NOEXCEPT_ATTR;

    } *geo;

    struct Configuration {

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
        TprResult(*createDepthDomainCapability)(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask, TprDepthDomain* pDomain) _TPR_NOEXCEPT_ATTR;
        void(*destroyDepthDomain)(TprDepthDomain domain) _TPR_NOEXCEPT_ATTR;

        TprResult(*createRenderTarget)(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) _TPR_NOEXCEPT_ATTR;
        TprResult(*createRenderTargetCapability)(TprRenderTarget target, TprRenderTargetCapabilityFlags mask, TprRenderTarget* pTarget) _TPR_NOEXCEPT_ATTR;
        void(*destroyRenderTarget)(TprRenderTarget target) _TPR_NOEXCEPT_ATTR;

        TprResult(*createRenderTargetSet)(const TprRenderTargetSetCreateInfo* pInfo, TprRenderTargetSet* pSet) _TPR_NOEXCEPT_ATTR;
        TprResult(*createRenderTargetSetCapability)(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask, TprRenderTargetSet* pSet) _TPR_NOEXCEPT_ATTR;
        void(*destroyRenderTargetSet)(TprRenderTargetSet set) _TPR_NOEXCEPT_ATTR;

        TprResult(*createEntityImage)(const TprEntityImageCreateInfo* pInfo, TprEntityImage* pImage) _TPR_NOEXCEPT_ATTR;
        TprResult(*createEntityImageCapability)(TprEntityImage image, TprEntityImageCapabilityFlags mask, TprEntityImage* pImage) _TPR_NOEXCEPT_ATTR;
        void(*destroyEntityImage)(TprEntityImage image) _TPR_NOEXCEPT_ATTR;

        TprJob(*getRenderJob)() _TPR_NOEXCEPT_ATTR;
        TprJob(*getRenderSignalJob)() _TPR_NOEXCEPT_ATTR;
        TprComponent(*getComponentRenderable)() _TPR_NOEXCEPT_ATTR;

    } *render;

    struct Scheduling {

        TprResult(*createJob)(const TprJobCreateInfo* pInfo, TprJob* pJob) _TPR_NOEXCEPT_ATTR;
        TprResult(*createJobCapability)(TprJob job, TprJobCapabilityFlags mask, TprJob* pJob) _TPR_NOEXCEPT_ATTR;
        TprResult(*scheduleJob)(TprJob job, uint64_t timepoint) _TPR_NOEXCEPT_ATTR;
        void(*pendJobDestruction)(TprJob job) _TPR_NOEXCEPT_ATTR;

        TprJob(*getShutdownJob)() _TPR_NOEXCEPT_ATTR;

        uint64_t(*now)() _TPR_NOEXCEPT_ATTR;

    } *sched;

} TprEngineAPI;


#ifdef __cplusplus
extern "C" {
#endif

int32_t pluginInit(const TprEngineAPI* pAPI) _TPR_NOEXCEPT;

#ifdef __cplusplus
}
#endif


#endif  // TEMPOR_PLUGIN_H_

