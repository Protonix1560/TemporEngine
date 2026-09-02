
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "tempor.hpp"

#pragma region log
    void TemporEngine::out_log(TprLogLevel logLevel, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), logLevel);
    }
    void TemporEngine::out_error(const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_ERROR, TPR_LOG_STYLE_ERROR1);
    }
    void TemporEngine::out_warn(const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_WARN, TPR_LOG_STYLE_WARN1);
    }
    void TemporEngine::out_info(const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_INFO);
    }
    void TemporEngine::out_debug(const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_DEBUG);
    }
    void TemporEngine::out_trace(const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_TRACE);
    }
    void TemporEngine::out_logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), logLevel, logStyle);
    }
    void TemporEngine::out_errorStyled(TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_ERROR, logStyle);
    }
    void TemporEngine::out_warnStyled(TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_WARN, logStyle);
    }
    void TemporEngine::out_infoStyled(TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_INFO, logStyle);
    }
    void TemporEngine::out_debugStyled(TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_DEBUG, logStyle);
    }
    void TemporEngine::out_traceStyled(TprLogStyle logStyle, const char* message) noexcept {
        mpOutSink->writeLog(format_sequence(message), TPR_LOG_LEVEL_TRACE, logStyle);
    }
    TprResult TemporEngine::out_writeMachineData(const char* pData, uint32_t size) noexcept {
        return mpOutSink->writeData(std::span(reinterpret_cast<const std::byte*>(pData), size));
    }
#pragma endregion  // log

#pragma region fs
    TprResult TemporEngine::fs_openFile(const char* path, TprOpenFileFlags flags, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->openFile(path, flags);
        if (!exp.has_value()) return exp.error();
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createMemoryFile(TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->createMemoryFile();
        if (!exp.has_value()) return exp.error();
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_forkFile(TprFile file, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->forkFile(file);
        if (!exp.has_value()) return exp.error();
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createFileCapability(TprFile file, TprFileCapabilityFlags mask, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->createFileCapability(file, mask);
        if (!exp.has_value()) return exp.error();
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::fs_closeFile(TprFile file) noexcept {
        if (!mpFileReg) return;
        mpFileReg->closeFile(file);
    }
    TprResult TemporEngine::fs_seek(TprFile file, int32_t offset, TprSeekWhence whence) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->seek(file, offset, whence);
    }
    TprResult TemporEngine::fs_tell(TprFile file, uint32_t* pPos) noexcept {
        if (!pPos) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->tell(file);
        if (!exp.has_value()) return exp.error();
        *pPos = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_read(TprFile file, uint32_t n, char* pData) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->read(file, n, reinterpret_cast<std::byte*>(pData));
    }
    TprResult TemporEngine::fs_readAt(TprFile file, uint32_t pos, uint32_t n, char* pData) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->readAt(file, pos, n, reinterpret_cast<std::byte*>(pData));
    }
    TprResult TemporEngine::fs_resize(TprFile file, uint32_t newSize) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->resize(file, newSize);
    }
    TprResult TemporEngine::fs_write(TprFile file, uint32_t n, const char* pData) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->write(file, n, reinterpret_cast<const std::byte*>(pData));
    }
    TprResult TemporEngine::fs_writeAt(TprFile file, uint32_t pos, uint32_t n, const char* pData) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->writeAt(file, pos, n, reinterpret_cast<const std::byte*>(pData));
    }
    TprResult TemporEngine::fs_pathType(const char* path, TprPathType* pType) noexcept {
        if (!pType) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        auto exp = mpFileReg->pathType(path);
        if (!exp.has_value()) return exp.error();
        *pType = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createDirectory(const char* path, TprCreateDirectoryFlags flags) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->createDirectory(path, flags);
    }
    TprResult TemporEngine::fs_touchFile(const char* path, TprTouchFileFlags flags) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->touchFile(path, flags);
    }
    TprResult TemporEngine::fs_remove(const char* path) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->remove(path);
    }
    TprResult TemporEngine::fs_move(const char* path, const char* newPath) noexcept {
        if (!mpFileReg) return TPR_ERROR_NOT_LOADED;
        return mpFileReg->move(path, newPath);
    }
#pragma endregion  // fs

#pragma region win
    TprResult TemporEngine::win_openWindow(const TprWindowCreateInfo* pInfo, TprWindow* pWindow) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pWindow) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->openWindow(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pWindow = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::win_createWindowCapability(TprWindow window, TprWindowCapabilityFlags mask, TprWindow* pWindow) noexcept {
        if (!pWindow) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->createWindowCapability(window, mask);
        if (!exp.has_value()) return exp.error();
        *pWindow = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::win_closeWindow(TprWindow window) noexcept {
        if (!mpWindowing) return;
        mpWindowing->closeWindow(window);
    }
    TprResult TemporEngine::win_createAction(const TprActionCreateInfo* pInfo, TprAction* pAction) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pAction) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->createAction(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pAction = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::win_createActionCapability(TprAction action, TprActionCapabilityFlags mask, TprAction* pAction) noexcept {
        if (!pAction) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->createActionCapability(action, mask);
        if (!exp.has_value()) return exp.error();
        *pAction = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::win_destroyAction(TprAction action) noexcept {
        if (!mpWindowing) return;
        mpWindowing->destroyAction(action);
    }
    TprResult TemporEngine::win_getActionsHistorySize(uint32_t filterCount, const TprAction* pFilters, uint32_t* pSize) noexcept {
        if (!pSize) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->getActionsHistorySize(filterCount, pFilters);
        if (!exp.has_value()) return exp.error();
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::win_copyActionsHistory(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) noexcept {
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        return mpWindowing->copyActionsHistory(pEntries, filterCount, pFilters);
    }
    TprResult TemporEngine::win_getActionState(TprAction action, TprActionState* pState) noexcept {
        if (!pState) return TPR_ERROR_INVALID_VALUE;
        if (!mpWindowing) return TPR_ERROR_NOT_LOADED;
        auto exp = mpWindowing->getActionState(action);
        if (!exp.has_value()) return exp.error();
        *pState = exp.value();
        return TPR_SUCCESS;
    }
    TprJob TemporEngine::win_getInputUpdateJob() noexcept {
        return mpWindowing->getInputUpdateJob();
    }
#pragma endregion  // win

#pragma region scene
    TprResult TemporEngine::scene_createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept {
        if (!pComponent) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSceneGraph->createComponent(componentSize);
        if (!exp.has_value()) return exp.error();
        *pComponent = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::scene_destroyComponent(TprComponent component) noexcept {
        if (!mpSceneGraph) return;
        mpSceneGraph->destroyComponent(component);
    }
    TprResult TemporEngine::scene_spawnEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept {
        if (!pEntity) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSceneGraph->spawnEntity(pComponents, componentCount);
        if (!exp.has_value()) return exp.error();
        *pEntity = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::scene_killEntity(TprEntity entity) noexcept {
        if (!mpSceneGraph) return;
        mpSceneGraph->killEntity(entity);
    }
    TprResult TemporEngine::scene_modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->modifyEntityComponentSet(entity, pComponents, componentCount);
    }
    TprResult TemporEngine::scene_copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->copyEntityComponentData(entity, component, offset, n, pData);
    }
    TprResult TemporEngine::scene_writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->writeEntityComponentData(entity, component, pData, offset, n);
    }
    TprResult TemporEngine::scene_getComponentChunkHandles(TprComponent component, TprFile resource) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->getComponentChunkHandles(component, resource);
    }
    uint32_t TemporEngine::scene_getComponentChunkMaxElementCount() noexcept {
        if (!mpSceneGraph) return 0;
        return mpSceneGraph->getComponentChunkMaxElementCount();
    }
    TprResult TemporEngine::scene_getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept {
        if (!pCount) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSceneGraph->getComponentChunkElementCount(chunk);
        if (!exp.has_value()) return exp.error();
        *pCount = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::scene_getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept {
        if (!pVersion) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSceneGraph->getComponentChunkVersion(chunk);
        if (!exp.has_value()) return exp.error();
        *pVersion = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::scene_copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->copyComponentChunkData(chunk, offset, n, pData);
    }
    TprResult TemporEngine::scene_writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_ERROR_NOT_LOADED;
        return mpSceneGraph->writeComponentChunkData(chunk, version, pData, offset, n);
    }
#pragma endregion  // scene

#pragma region geo
    TprResult TemporEngine::geo_createMesh(const TprMeshCreateInfo* pInfo, TprMesh* pMesh) noexcept {
        if (!pMesh) return TPR_ERROR_INVALID_VALUE;
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!mpAssetStore) return TPR_ERROR_NOT_LOADED;
        auto exp = mpAssetStore->createMesh(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pMesh = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::geo_createMeshCapability(TprMesh mesh, TprMeshCapabilityFlags mask, TprMesh* pMesh) noexcept {
        if (!pMesh) return TPR_ERROR_INVALID_VALUE;
        if (!mpAssetStore) return TPR_ERROR_NOT_LOADED;
        auto exp = mpAssetStore->createMeshCapability(mesh, mask);
        if (!exp.has_value()) return exp.error();
        *pMesh = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::geo_destroyMesh(TprMesh mesh) noexcept {
        if (!mpAssetStore) return;
        mpAssetStore->destroyMesh(mesh);
    }
    TprResult TemporEngine::geo_requireMeshLoaded(TprMesh mesh) noexcept {
        if (!mpAssetStore) return TPR_ERROR_NOT_LOADED;
        return mpAssetStore->requireMeshLoaded(mesh);
    }
    TprResult TemporEngine::geo_unrequireMeshLoaded(TprMesh mesh) noexcept {
        if (!mpAssetStore) return TPR_ERROR_NOT_LOADED;
        return mpAssetStore->unrequireMeshLoaded(mesh);
    }
#pragma endregion  // geo

#pragma region conf
    TprResult TemporEngine::conf_getRootSetting(TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        if (!mpPlugLd) return TPR_ERROR_NOT_LOADED;
        auto infoExp = activePluginInfo();
        if (!infoExp.has_value()) return infoExp.error();
        auto info = infoExp.value();
        auto exp = mpSettings->createSetting(mpSettings->getRoot(), info.name);
        if (!exp.has_value()) return exp.error();
        TprSetting value = exp.value();
        if (auto r = mpSettings->setSettingStruct(value); r != TPR_SUCCESS) return r;
        *pSetting = value;
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_createSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        if (!mpPlugLd) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->createSetting(baseSetting, name);
        if (!exp.has_value()) return exp.error();
        *pSetting = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_readSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        if (!mpPlugLd) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->readSetting(baseSetting, name);
        if (!exp.has_value()) return exp.error();
        *pSetting = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::conf_destroySetting(TprSetting setting) noexcept {
        if (!mpSettings) return;
        mpSettings->destroySetting(setting);
    }
    TprResult TemporEngine::conf_getSettingType(TprSetting setting, TprSettingType* pType) noexcept {
        if (!pType) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingType(setting);
        if (!exp.has_value()) return exp.error();
        *pType = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingDouble(TprSetting setting, double* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingDouble(setting);
        if (!exp.has_value()) return exp.error();
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingInteger(TprSetting setting, int64_t* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingInteger(setting);
        if (!exp.has_value()) return exp.error();
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingBool(TprSetting setting, TprBool8* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingBool(setting);
        if (!exp.has_value()) return exp.error();
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingStringSize(TprSetting setting, uint32_t* pSize) noexcept {
        if (!pSize) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingStringSize(setting);
        if (!exp.has_value()) return exp.error();
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_copySettingString(TprSetting setting, char* pData) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->copySettingString(setting, pData);
    }
    TprResult TemporEngine::conf_setSettingDouble(TprSetting setting, double data) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingDouble(setting, data);
    }
    TprResult TemporEngine::conf_setSettingInteger(TprSetting setting, int64_t data) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingInteger(setting, data);
    }
    TprResult TemporEngine::conf_setSettingBool(TprSetting setting, TprBool8 data) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingBool(setting, data);
    }
    TprResult TemporEngine::conf_setSettingString(TprSetting setting, const char* pData) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingString(setting, pData);
    }
    TprResult TemporEngine::conf_setSettingNull(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingNull(setting);
    }
    TprResult TemporEngine::conf_unsetSetting(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->unsetSetting(setting);
    }
    TprResult TemporEngine::conf_setSettingStruct(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingStruct(setting);
    }
    TprResult TemporEngine::conf_setSettingArray(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->setSettingArray(setting);
    }
    double TemporEngine::conf_getSettingDoubleOr(TprSetting setting, double fallback) noexcept {
        if (!mpSettings) return fallback;
        return mpSettings->getSettingDoubleOr(setting, fallback);
    }
    int64_t TemporEngine::conf_getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept {
        if (!mpSettings) return fallback;
        return mpSettings->getSettingIntegerOr(setting, fallback);
    }
    TprBool8 TemporEngine::conf_getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept {
        if (!mpSettings) return fallback;
        return mpSettings->getSettingBoolOr(setting, fallback);
    }
    TprResult TemporEngine::conf_getSettingArraySize(TprSetting setting, uint32_t* pSize) noexcept {
        if (!pSize) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingArraySize(setting);
        if (!exp.has_value()) return exp.error();
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingArrayElement(TprSetting setting, uint32_t index, TprSetting* pElement) noexcept {
        if (!pElement) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSettings->getSettingArrayElement(setting, index);
        if (!exp.has_value()) return exp.error();
        *pElement = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_resizeSettingArray(TprSetting setting, uint32_t size) noexcept {
        if (!mpSettings) return TPR_ERROR_NOT_LOADED;
        return mpSettings->resizeSettingArray(setting, size);
    }
#pragma endregion  // conf

#pragma region render
    TprResult TemporEngine::render_createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pDomain) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createDepthDomain(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pDomain = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::render_createDepthDomainCapability(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask, TprDepthDomain* pDomain) noexcept {
        if (!pDomain) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createDepthDomainCapability(domain, mask);
        if (!exp.has_value()) return exp.error();
        *pDomain = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyDepthDomain(TprDepthDomain domain) noexcept {
        if (!mpGDev) return;
        mpGDev->destroyDepthDomain(domain);
    }
    TprResult TemporEngine::render_createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pTarget) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createRenderTarget(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pTarget = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::render_createRenderTargetCapability(TprRenderTarget target, TprRenderTargetCapabilityFlags mask, TprRenderTarget* pTarget) noexcept {
        if (!pTarget) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createRenderTargetCapability(target, mask);
        if (!exp.has_value()) return exp.error();
        *pTarget = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyRenderTarget(TprRenderTarget target) noexcept {
        if (!mpGDev) return;
        mpGDev->destroyRenderTarget(target);
    }
    TprResult TemporEngine::render_createRenderTargetSet(const TprRenderTargetSetCreateInfo* pInfo, TprRenderTargetSet* pSet) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pSet) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createRenderTargetSet(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pSet = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::render_createRenderTargetSetCapability(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask, TprRenderTargetSet* pSet) noexcept {
        if (!pSet) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createRenderTargetSetCapability(set, mask);
        if (!exp.has_value()) return exp.error();
        *pSet = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyRenderTargetSet(TprRenderTargetSet set) noexcept {
        if (!mpGDev) return;
        mpGDev->destroyRenderTargetSet(set);
    }
    TprResult TemporEngine::render_createEntityImage(const TprEntityImageCreateInfo* pInfo, TprEntityImage* pImage) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pImage) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createEntityImage(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pImage = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::render_createEntityImageCapability(TprEntityImage image, TprEntityImageCapabilityFlags mask, TprEntityImage* pImage) noexcept {
        if (!pImage) return TPR_ERROR_INVALID_VALUE;
        if (!mpGDev) return TPR_ERROR_NOT_LOADED;
        auto exp = mpGDev->createEntityImageCapability(image, mask);
        if (!exp.has_value()) return exp.error();
        *pImage = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyEntityImage(TprEntityImage image) noexcept {
        if (!mpGDev) return;
        mpGDev->destroyEntityImage(image);
    }
    TprJob TemporEngine::render_getRenderJob() noexcept {
        return mpGDev->getRenderJob();
    }
    TprJob TemporEngine::render_getRenderSignalJob() noexcept {
        return mpGDev->getRenderSignalJob();
    }
    TprComponent TemporEngine::render_getComponentRenderable() noexcept {
        return mpGDev->getComponentRenderable();
    }
#pragma endregion  // render

#pragma region sched
    TprResult TemporEngine::sched_createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept {
        if (!pInfo) return TPR_ERROR_INVALID_VALUE;
        if (!pJob) return TPR_ERROR_INVALID_VALUE;
        if (!mpSched) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSched->createJob(*pInfo);
        if (!exp.has_value()) return exp.error();
        *pJob = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::sched_createJobCapability(TprJob job, TprJobCapabilityFlags mask, TprJob* pJob) noexcept {
        if (!pJob) return TPR_ERROR_INVALID_VALUE;
        if (!mpSched) return TPR_ERROR_NOT_LOADED;
        auto exp = mpSched->createJobCapability(job, mask);
        if (!exp.has_value()) return exp.error();
        *pJob = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::sched_scheduleJob(TprJob job, uint64_t timepoint) noexcept {
        if (!mpSched) return TPR_ERROR_NOT_LOADED;
        return mpSched->scheduleJob(job, timepoint);
    }
    void TemporEngine::sched_invalidateJob(TprJob job) noexcept {
        if (!mpSched) return;
        mpSched->invalidateJob(job);
    }
    void TemporEngine::sched_destroyJob(TprJob job) noexcept {
        if (!mpSched) return;
        mpSched->destroyJob(job);
    }
    TprJob TemporEngine::sched_getShutdownJob() noexcept {
        return mpPlugLd->getShutdownJob();
    }
    uint64_t TemporEngine::sched_now() noexcept {
        if (!mpSched) return 0;
        return mpSched->now();
    }
#pragma endregion  // sched
