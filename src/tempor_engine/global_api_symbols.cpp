
#include "tempor.hpp"


namespace {
    TemporEngine* gEngine = nullptr;
}


namespace api {
    namespace log {
        void log(TprLogLevel logLevel, const char* message) noexcept { assert(gEngine); gEngine->out_log(logLevel, message); }
        void info(const char* message) noexcept { assert(gEngine); gEngine->out_info(message); }
        void warn(const char* message) noexcept { assert(gEngine); gEngine->out_warn(message); }
        void error(const char* message) noexcept { assert(gEngine); gEngine->out_error(message); }
        void debug(const char* message) noexcept { assert(gEngine); gEngine->out_debug(message); }
        void trace(const char* message) noexcept { assert(gEngine); gEngine->out_trace(message); }
        void logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_logStyled(logLevel, logStyle, message); }
        void infoStyled(TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_infoStyled(logStyle, message); }
        void warnStyled(TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_warnStyled(logStyle, message); }
        void errorStyled(TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_errorStyled(logStyle, message); }
        void debugStyled(TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_debugStyled(logStyle, message); }
        void traceStyled(TprLogStyle logStyle, const char* message) noexcept { assert(gEngine); gEngine->out_traceStyled(logStyle, message); }
        TprResult writeMachineData(const char* pData, uint32_t size) noexcept { assert(gEngine); return gEngine->out_writeMachineData(pData, size); }
    }
    
    namespace scene {
        TprResult createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept {
            assert(gEngine); return gEngine->scene_createComponent(componentSize, pComponent);
        }
        void destroyComponent(TprComponent component) noexcept {
            assert(gEngine); gEngine->scene_destroyComponent(component);
        }
        TprResult spawnEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept {
            assert(gEngine); return gEngine->scene_spawnEntity(pComponents, componentCount, pEntity);
        }
        void killEntity(TprEntity entity) noexcept {
            assert(gEngine); gEngine->scene_killEntity(entity);
        }
        TprResult modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {
            assert(gEngine); return gEngine->scene_modifyEntityComponentSet(entity, pComponents, componentCount);
        }
        TprResult copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) noexcept {
            assert(gEngine); return gEngine->scene_copyEntityComponentData(entity, component, start, n, componentData);
        }
        TprResult writeEntityComponentData(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) noexcept {
            assert(gEngine); return gEngine->scene_writeEntityComponentData(entity, component, componentData, start, n);
        }
        TprResult getComponentChunkHandles(TprComponent component, TprResource resource) noexcept {
            assert(gEngine); return gEngine->scene_getComponentChunkHandles(component, resource);
        }
        uint32_t getComponentChunkMaxElementCount() noexcept {
            assert(gEngine); return gEngine->scene_getComponentChunkMaxElementCount();
        }
        TprResult getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept {
            assert(gEngine); return gEngine->scene_getComponentChunkElementCount(chunk, pCount);
        }
        TprResult getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept {
            assert(gEngine); return gEngine->scene_getComponentChunkVersion(chunk, pVersion);
        }
        TprResult copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept {
            assert(gEngine); return gEngine->scene_copyComponentChunkData(chunk, offset, n, pData);
        }
        TprResult writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept {
            assert(gEngine); return gEngine->scene_writeComponentChunkData(chunk, version, pData, offset, n);
        }
    }

    namespace vfs {
        TprResult openPathResource(const char* path, TprOpenPathResourceFlags flags, TprResource* pResource) noexcept {
            assert(gEngine); return gEngine->vfs_openPathResource(path, flags, pResource);
        }
        TprResult openReferenceResource(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept {
            assert(gEngine); return gEngine->vfs_openReferenceResource(begin, end, flags, pResource);
        }
        TprResult openViewResource(const char* begin, const char* end, TprOpenViewResourceFlags flags, TprResource* pResource) noexcept {
            assert(gEngine); return gEngine->vfs_openViewResource(begin, end, flags, pResource);
        }
        TprResult openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            assert(gEngine); return gEngine->vfs_openEmptyResource(size, flags, alignment, pResource);
        }
        TprResult openCapabilityResource(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprResourceCapabilityFlags protectFlags, TprResource* pResource) noexcept {
            assert(gEngine); return gEngine->vfs_openCapabilityResource(protectResource, flags, protectFlags, pResource);
        }
        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept {
            assert(gEngine); return gEngine->vfs_resizeResource(resource, newSize);
        }
        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
            assert(gEngine); return gEngine->vfs_sizeofResource(resource, pSize);
        }
        TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept {
            assert(gEngine); return gEngine->vfs_getResourceRawDataPointer(resource, pData);
        }
        TprResult getResourceConstPointer(TprResource resource, const char** pData) noexcept {
            assert(gEngine); return gEngine->vfs_getResourceConstPointer(resource, pData);
        }
        void closeResource(TprResource resource) noexcept {
            assert(gEngine); gEngine->vfs_closeResource(resource);
        }
    }

    namespace win {
        TprResult openWindow(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) noexcept {
            assert(gEngine); return gEngine->win_openWindow(pCreateInfo, pWindow);
        }
        void closeWindow(TprWindow window) noexcept {
            assert(gEngine); gEngine->win_closeWindow(window);
        }
    }

    namespace input {
        TprResult createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) noexcept {
            assert(gEngine); return gEngine->input_createAction(window, pCreateInfo, pAction);
        }
        void destroyAction(TprAction action) noexcept {
            assert(gEngine); gEngine->input_destroyAction(action);
        }
        TprResult getActionState(TprAction action, TprActionState* pState) noexcept {
            assert(gEngine); return gEngine->input_getActionState(action, pState);
        }
        TprResult getInputElementVector(TprWindow window, TprInputElement element, TprInputElementVector* pVector) noexcept {
            assert(gEngine); return gEngine->input_getInputElementVector(window, element, pVector);
        }
    }

    namespace geo {
        TprResult createMesh(const TprMeshCreateInfo* pCreateInfo, TprMesh* pMesh) noexcept {
            assert(gEngine); return gEngine->geo_createMesh(pCreateInfo, pMesh);
        }
        TprResult loadMesh(TprMesh mesh, const TprMeshLoadInfo* pLoadInfo) noexcept {
            assert(gEngine); return gEngine->geo_loadMesh(mesh, pLoadInfo);
        }
        void unloadMesh(TprMesh mesh) noexcept { assert(gEngine); gEngine->geo_unloadMesh(mesh); }
        void destroyMesh(TprMesh mesh) noexcept { assert(gEngine); gEngine->geo_destroyMesh(mesh); }
    }

    namespace conf {
        TprResult getRootSetting(TprSetting* pSetting) noexcept {
            assert(gEngine); return gEngine->conf_getRootSetting(pSetting);
        }
        TprResult createSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
            assert(gEngine); return gEngine->conf_createSetting(baseSetting, name, pSetting);
        }
        TprResult readSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
            assert(gEngine); return gEngine->conf_readSetting(baseSetting, name, pSetting);
        }
        void destroySetting(TprSetting pSetting) noexcept {
            assert(gEngine); gEngine->conf_destroySetting(pSetting);
        }
        TprResult getSettingType(TprSetting setting, TprSettingType* pType) noexcept {
            assert(gEngine); return gEngine->conf_getSettingType(setting, pType);
        }
        TprResult getSettingDouble(TprSetting setting, double* pData) noexcept {
            assert(gEngine); return gEngine->conf_getSettingDouble(setting, pData);
        }
        TprResult getSettingInteger(TprSetting setting, int64_t* pData) noexcept {
            assert(gEngine); return gEngine->conf_getSettingInteger(setting, pData);
        }
        TprResult getSettingBool(TprSetting setting, TprBool8* pData) noexcept {
            assert(gEngine); return gEngine->conf_getSettingBool(setting, pData);
        }
        double getSettingDoubleOr(TprSetting setting, double fallback) noexcept {
            assert(gEngine); return gEngine->conf_getSettingDoubleOr(setting, fallback);
        }
        int64_t getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept {
            assert(gEngine); return gEngine->conf_getSettingIntegerOr(setting, fallback);
        }
        TprBool8 getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept {
            assert(gEngine); return gEngine->conf_getSettingBoolOr(setting, fallback);
        }
        TprResult getSettingStringSize(TprSetting setting, uint32_t* pSize) noexcept {
            assert(gEngine); return gEngine->conf_getSettingStringSize(setting, pSize);
        }
        TprResult copySettingString(TprSetting setting, char* pData) noexcept {
            assert(gEngine); return gEngine->conf_copySettingString(setting, pData);
        }
        TprResult setSettingDouble(TprSetting setting, double data) noexcept {
            assert(gEngine); return gEngine->conf_setSettingDouble(setting, data);
        }
        TprResult setSettingInteger(TprSetting setting, int64_t data) noexcept {
            assert(gEngine); return gEngine->conf_setSettingInteger(setting, data);
        }
        TprResult setSettingBool(TprSetting setting, TprBool8 data) noexcept {
            assert(gEngine); return gEngine->conf_setSettingBool(setting, data);
        }
        TprResult setSettingString(TprSetting setting, const char* pData) noexcept {
            assert(gEngine); return gEngine->conf_setSettingString(setting, pData);
        }
        TprResult setSettingNull(TprSetting setting) noexcept {
            assert(gEngine); return gEngine->conf_setSettingNull(setting);
        }
        TprResult unsetSetting(TprSetting setting) noexcept {
            assert(gEngine); return gEngine->conf_unsetSetting(setting);
        }
        TprResult setSettingStruct(TprSetting setting) noexcept {
            assert(gEngine); return gEngine->conf_setSettingStruct(setting);
        }
        TprResult setSettingArray(TprSetting setting) noexcept {
            assert(gEngine); return gEngine->conf_setSettingArray(setting);
        }
        TprResult getSettingArraySize(TprSetting setting, uint32_t* pSize) noexcept {
            assert(gEngine); return gEngine->conf_getSettingArraySize(setting, pSize);
        }
        TprResult getSettingArrayElement(TprSetting setting, uint32_t index, TprSetting* pElement) noexcept {
            assert(gEngine); return gEngine->conf_getSettingArrayElement(setting, index, pElement);
        }
        TprResult resizeSettingArray(TprSetting setting, uint32_t size) noexcept {
            assert(gEngine); return gEngine->conf_resizeSettingArray(setting, size);
        }
    }

    namespace render {
        TprResult createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept {
            assert(gEngine); return gEngine->render_createDepthDomain(pInfo, pDomain);
        }
        void destroyDepthDomain(TprDepthDomain domain) noexcept {
            assert(gEngine); return gEngine->render_destroyDepthDomain(domain);
        }
        TprResult createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept {
            assert(gEngine); return gEngine->render_createRenderTarget(pInfo, pTarget);
        }
        void destroyRenderTarget(TprRenderTarget target) noexcept {
            assert(gEngine); gEngine->render_destroyRenderTarget(target);
        }
        TprComponent getComponentRenderable() noexcept {
            assert(gEngine); return gEngine->render_getComponentRenderable();
        }
        TprResult createObjectImage(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) noexcept {
            assert(gEngine); return gEngine->render_createObjectImage(pInfo, pImage);
        }
        void destroyObjectImage(TprObjectImage image) noexcept {
            assert(gEngine); gEngine->render_destroyObjectImage(image);
        }
    }

    namespace thread {
        TprResult createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept { assert(gEngine); return gEngine->thread_createJob(pInfo, pJob); }
        TprResult createDetachedJob(const TprJobCreateInfo* pInfo) noexcept { assert(gEngine); return gEngine->thread_createDetachedJob(pInfo); }
        TprResult jobFinished(TprJob job, TprBool8* pData) noexcept { assert(gEngine); return gEngine->thread_jobFinished(job, pData); }
        void joinJob(TprJob job) noexcept { assert(gEngine); gEngine->thread_joinJob(job); }
    }
}


void TemporEngine::registerAPI() {

    gEngine = this;

    // log
    mOutAPI.log = api::log::log;
    mOutAPI.info = api::log::info;
    mOutAPI.warn = api::log::warn;
    mOutAPI.error = api::log::error;
    mOutAPI.debug = api::log::debug;
    mOutAPI.trace = api::log::trace;
    mOutAPI.logStyled = api::log::logStyled;
    mOutAPI.infoStyled = api::log::infoStyled;
    mOutAPI.warnStyled = api::log::warnStyled;
    mOutAPI.errorStyled = api::log::errorStyled;
    mOutAPI.debugStyled = api::log::debugStyled;
    mOutAPI.traceStyled = api::log::traceStyled;
    mOutAPI.writeMachineData = api::log::writeMachineData;
    // vfs
    mVFSAPI.openPathResource = api::vfs::openPathResource;
    mVFSAPI.openReferenceResource = api::vfs::openReferenceResource;
    mVFSAPI.openViewResource = api::vfs::openViewResource;
    mVFSAPI.openEmptyResource = api::vfs::openEmptyResource;
    mVFSAPI.openCapabilityResource = api::vfs::openCapabilityResource;
    mVFSAPI.resizeResource = api::vfs::resizeResource;
    mVFSAPI.sizeofResource = api::vfs::sizeofResource;
    mVFSAPI.getResourceRawDataPointer = api::vfs::getResourceRawDataPointer;
    mVFSAPI.getResourceConstPointer = api::vfs::getResourceConstPointer;
    mVFSAPI.closeResource = api::vfs::closeResource;
    // scene
    mSceneAPI.createComponent = api::scene::createComponent;
    mSceneAPI.destroyComponent = api::scene::destroyComponent;
    mSceneAPI.spawnEntity = api::scene::spawnEntity;
    mSceneAPI.killEntity = api::scene::killEntity;
    mSceneAPI.modifyEntityComponentSet = api::scene::modifyEntityComponentSet;
    mSceneAPI.copyEntityComponentData = api::scene::copyEntityComponentData;
    mSceneAPI.writeEntityComponentData = api::scene::writeEntityComponentData;
    mSceneAPI.getComponentChunkHandles = api::scene::getComponentChunkHandles;
    mSceneAPI.getComponentChunkMaxElementCount = api::scene::getComponentChunkMaxElementCount;
    mSceneAPI.getComponentChunkElementCount = api::scene::getComponentChunkElementCount;
    mSceneAPI.getComponentChunkVersion = api::scene::getComponentChunkVersion;
    mSceneAPI.copyComponentChunkData = api::scene::copyComponentChunkData;
    mSceneAPI.writeComponentChunkData = api::scene::writeComponentChunkData;
    // geo
    mGeoAPI.createMesh = api::geo::createMesh;
    mGeoAPI.loadMesh = api::geo::loadMesh;
    mGeoAPI.unloadMesh = api::geo::unloadMesh;
    mGeoAPI.destroyMesh = api::geo::destroyMesh;
    // win
    mWinAPI.openWindow = api::win::openWindow;
    mWinAPI.closeWindow = api::win::closeWindow;
    // input
    mInputAPI.getInputElementVector = api::input::getInputElementVector;
    mInputAPI.createAction = api::input::createAction;
    mInputAPI.destroyAction = api::input::destroyAction;
    mInputAPI.getActionState = api::input::getActionState;
    // conf
    mConfAPI.getRootSetting = api::conf::getRootSetting;
    mConfAPI.createSetting = api::conf::createSetting;
    mConfAPI.readSetting = api::conf::readSetting;
    mConfAPI.destroySetting = api::conf::destroySetting;
    mConfAPI.getSettingType = api::conf::getSettingType;
    mConfAPI.getSettingDouble = api::conf::getSettingDouble;
    mConfAPI.getSettingInteger = api::conf::getSettingInteger;
    mConfAPI.getSettingBool = api::conf::getSettingBool;
    mConfAPI.getSettingStringSize = api::conf::getSettingStringSize;
    mConfAPI.copySettingString = api::conf::copySettingString;
    mConfAPI.setSettingString = api::conf::setSettingString;
    mConfAPI.setSettingBool = api::conf::setSettingBool;
    mConfAPI.setSettingDouble = api::conf::setSettingDouble;
    mConfAPI.setSettingInteger = api::conf::setSettingInteger;
    mConfAPI.setSettingNull = api::conf::setSettingNull;
    mConfAPI.unsetSetting = api::conf::unsetSetting;
    mConfAPI.setSettingStruct = api::conf::setSettingStruct;
    mConfAPI.setSettingArray = api::conf::setSettingArray;
    mConfAPI.getSettingDoubleOr = api::conf::getSettingDoubleOr;
    mConfAPI.getSettingIntegerOr = api::conf::getSettingIntegerOr;
    mConfAPI.getSettingBoolOr = api::conf::getSettingBoolOr;
    mConfAPI.getSettingArraySize = api::conf::getSettingArraySize;
    mConfAPI.getSettingArrayElement = api::conf::getSettingArrayElement;
    mConfAPI.resizeSettingArray = api::conf::resizeSettingArray;
    // render
    mRenderAPI.createDepthDomain = api::render::createDepthDomain;
    mRenderAPI.destroyDepthDomain = api::render::destroyDepthDomain;
    mRenderAPI.createRenderTarget = api::render::createRenderTarget;
    mRenderAPI.destroyRenderTarget = api::render::destroyRenderTarget;
    mRenderAPI.getComponentRenderable = api::render::getComponentRenderable;
    mRenderAPI.createObjectImage = api::render::createObjectImage;
    mRenderAPI.destroyObjectImage = api::render::destroyObjectImage;
    // thread
    mThreadAPI.createJob = api::thread::createJob;
    mThreadAPI.createDetachedJob = api::thread::createDetachedJob;
    mThreadAPI.jobFinished = api::thread::jobFinished;
    mThreadAPI.joinJob = api::thread::joinJob;

    mAPI.out = &mOutAPI;
    mAPI.win = &mWinAPI;
    mAPI.vfs = &mVFSAPI;
    mAPI.scene = &mSceneAPI;
    mAPI.geo = &mGeoAPI;
    mAPI.input = &mInputAPI;
    mAPI.conf = &mConfAPI;
    mAPI.render = &mRenderAPI;
    mAPI.thread = &mThreadAPI;
}

