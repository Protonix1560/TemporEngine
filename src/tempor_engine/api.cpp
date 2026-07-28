
#include "core.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "tempor.hpp"

#include <mutex>


#pragma region log
    void TemporEngine::out_log(TprLogLevel logLevel, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, logLevel);
    }
    void TemporEngine::out_error(const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_ERROR, TPR_LOG_STYLE_ERROR1);
    }
    void TemporEngine::out_warn(const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_WARN, TPR_LOG_STYLE_WARN1);
    }
    void TemporEngine::out_info(const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_INFO);
    }
    void TemporEngine::out_debug(const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_DEBUG);
    }
    void TemporEngine::out_trace(const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_TRACE);
    }
    void TemporEngine::out_logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, logLevel, logStyle);
    }
    void TemporEngine::out_errorStyled(TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_ERROR, logStyle);
    }
    void TemporEngine::out_warnStyled(TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_WARN, logStyle);
    }
    void TemporEngine::out_infoStyled(TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_INFO, logStyle);
    }
    void TemporEngine::out_debugStyled(TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_DEBUG, logStyle);
    }
    void TemporEngine::out_traceStyled(TprLogStyle logStyle, const char* message) noexcept {
        auto sink = mpOutSink.load();
        if (!sink) return;
        sink->writeLog(message, TPR_LOG_LEVEL_TRACE, logStyle);
    }
    TprResult TemporEngine::out_writeMachineData(const char* pData, uint32_t size) noexcept {
        if (!mServHolder.alive<OutputSinkVariant>()) return TPR_MODULE_NOT_LOADED;
        return std::visit(overload{
            [pData, size](auto& sink) {
                return sink->writeData(std::span(reinterpret_cast<const std::byte*>(pData), size));
            },
        }, mServHolder.get<OutputSinkVariant>());
    }
#pragma endregion  // log

#pragma region fs
    TprResult TemporEngine::fs_openFile(const char* path, TprOpenFileFlags flags, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->openFile(path, flags);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createMemoryFile(TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->createMemoryFile();
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_forkFile(TprFile file, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->forkFile(file);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createFileCapability(TprFile file, TprFileCapabilityFlags mask, TprFile* pFile) noexcept {
        if (!pFile) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->createFileCapability(file, mask);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pFile = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::fs_closeFile(TprFile file) noexcept {
        if (!mpFileReg) return;
        mpFileReg->closeFile(file);
    }
    TprResult TemporEngine::fs_seek(TprFile file, int32_t offset, TprSeekWhence whence) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->seek(file, offset, whence);
    }
    TprResult TemporEngine::fs_tell(TprFile file, uint32_t* pPos) noexcept {
        if (!pPos) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->tell(file);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pPos = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_read(TprFile file, uint32_t n, char* pData) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->read(file, n, reinterpret_cast<std::byte*>(pData));
    }
    TprResult TemporEngine::fs_readAt(TprFile file, uint32_t pos, uint32_t n, char* pData) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->readAt(file, pos, n, reinterpret_cast<std::byte*>(pData));
    }
    TprResult TemporEngine::fs_resize(TprFile file, uint32_t newSize) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->resize(file, newSize);
    }
    TprResult TemporEngine::fs_write(TprFile file, uint32_t n, const char* pData) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->write(file, n, reinterpret_cast<const std::byte*>(pData));
    }
    TprResult TemporEngine::fs_writeAt(TprFile file, uint32_t pos, uint32_t n, const char* pData) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->writeAt(file, pos, n, reinterpret_cast<const std::byte*>(pData));
    }
    TprResult TemporEngine::fs_pathType(const char* path, TprPathType* pType) noexcept {
        if (!pType) return TPR_ERROR_INVALID_VALUE;
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpFileReg->pathType(path);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pType = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::fs_createDirectory(const char* path, TprCreateDirectoryFlags flags) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->createDirectory(path, flags);
    }
    TprResult TemporEngine::fs_touchFile(const char* path, TprTouchFileFlags flags) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->touchFile(path, flags);
    }
    TprResult TemporEngine::fs_remove(const char* path) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->remove(path);
    }
    TprResult TemporEngine::fs_move(const char* path, const char* newPath) noexcept {
        if (!mpFileReg) return TPR_MODULE_NOT_LOADED;
        return mpFileReg->move(path, newPath);
    }
#pragma endregion  // fs

#pragma region input
    TprResult TemporEngine::input_createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) noexcept {
        if (!pAction) return TPR_ERROR_INVALID_VALUE;
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto exp = mpWinMan->createAction(window, pCreateInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pAction = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::input_destroyAction(TprAction action) noexcept {
        if (!mpWinMan) return;
        mpWinMan->destroyAction(action);
    }
    TprResult TemporEngine::input_getActionState(TprAction action, TprActionState* pState) noexcept {
        if (!pState) return TPR_ERROR_INVALID_VALUE;
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto result = mpWinMan->getActionState(action, pState);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::input_getInputElementVector(TprWindow window, TprInputElement element, TprInputElementVector* pVector) noexcept {
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto result = mpWinMan->getInputElementVector(window, element, pVector);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
#pragma endregion  // input

#pragma region win
    TprResult TemporEngine::win_openWindow(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) noexcept {
        if (!pWindow) return TPR_ERROR_INVALID_VALUE;
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto exp = mpWinMan->openWindow(pCreateInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        TprWindow handle = exp.value();
        if (mpHWLI) {
            TprResult r = mpHWLI->registerWindow(handle);
            switch (r) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                case TPR_SUCCESS: break;
                default: return r;
            }
        }
        *pWindow = handle;
        return TPR_SUCCESS;
    }
    void TemporEngine::win_closeWindow(TprWindow window) noexcept {
        if (mpHWLI) mpHWLI->unregisterWindow(window);
        if (mpWinMan) mpWinMan->closeWindow(window);
    }
#pragma endregion  // win

#pragma region scene
    TprResult TemporEngine::scene_createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept {
        if (!pComponent) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSceneGraph->createComponent(componentSize);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pComponent = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::scene_destroyComponent(TprComponent component) noexcept {
        if (!mpSceneGraph) return;
        mpSceneGraph->destroyComponent(component);
    }
    TprResult TemporEngine::scene_spawnEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept {
        if (!pEntity) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSceneGraph->spawnEntity(pComponents, componentCount);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pEntity = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::scene_killEntity(TprEntity entity) noexcept {
        if (!mpSceneGraph) return;
        mpSceneGraph->killEntity(entity);
    }
    TprResult TemporEngine::scene_modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->modifyEntityComponentSet(entity, pComponents, componentCount);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::scene_copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->copyEntityComponentData(entity, component, offset, n, pData);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::scene_writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->writeEntityComponentData(entity, component, pData, offset, n);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::scene_getComponentChunkHandles(TprComponent component, TprFile resource) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->getComponentChunkHandles(component, resource);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    uint32_t TemporEngine::scene_getComponentChunkMaxElementCount() noexcept {
        if (!mpSceneGraph) return 0;
        return mpSceneGraph->getComponentChunkMaxElementCount();
    }
    TprResult TemporEngine::scene_getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept {
        if (!pCount) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSceneGraph->getComponentChunkElementCount(chunk);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pCount = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::scene_getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept {
        if (!pVersion) return TPR_ERROR_INVALID_VALUE;
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSceneGraph->getComponentChunkVersion(chunk);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pVersion = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::scene_copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->copyComponentChunkData(chunk, offset, n, pData);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::scene_writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->writeComponentChunkData(chunk, version, pData, offset, n);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
#pragma endregion  // scene

#pragma region geo
    TprResult TemporEngine::geo_createMesh(const TprMeshCreateInfo* pInfo, TprMesh* pMesh) noexcept {
        if (!pMesh) return TPR_ERROR_INVALID_VALUE;
        if (!mpAssetStore) return TPR_MODULE_NOT_LOADED;
        auto exp = mpAssetStore->createMesh(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pMesh = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::geo_loadMesh(TprMesh mesh, const TprMeshLoadInfo* pInfo) noexcept {
        if (!mpAssetStore) return TPR_MODULE_NOT_LOADED;
        auto result = mpAssetStore->loadMesh(mesh, pInfo);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    void TemporEngine::geo_unloadMesh(TprMesh mesh) noexcept {
        if (!mpAssetStore) return;
        mpAssetStore->unloadMesh(mesh);
    }
    void TemporEngine::geo_destroyMesh(TprMesh mesh) noexcept {
        if (!mpAssetStore) return;
        mpAssetStore->destroyMesh(mesh);
    }
#pragma endregion  // geo

#pragma region conf
    TprResult TemporEngine::conf_getRootSetting(TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        if (!mpPlugLd) return TPR_MODULE_NOT_LOADED;
        auto infoExp = activePluginInfo();
        if (!infoExp.has_value()) {
            switch (infoExp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return infoExp.error();
            }
        }
        auto info = infoExp.value();
        auto exp = mpSettings->createSetting(mpSettings->getRoot(), info.name);
        if (!exp.has_value()) {
        switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        TprSetting value = exp.value();
        auto result = mpSettings->setSettingStruct(value);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            case TPR_SUCCESS: break;
            default: return result;
        }
        *pSetting = value;
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_createSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        if (!mpPlugLd) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->createSetting(baseSetting, name);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pSetting = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_readSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        if (!mpPlugLd) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->readSetting(baseSetting, name);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pSetting = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::conf_destroySetting(TprSetting setting) noexcept {
        if (!mpSettings) return;
        mpSettings->destroySetting(setting);
    }
    TprResult TemporEngine::conf_getSettingType(TprSetting setting, TprSettingType* pType) noexcept {
        if (!pType) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingType(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pType = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingDouble(TprSetting setting, double* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingDouble(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingInteger(TprSetting setting, int64_t* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingInteger(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingBool(TprSetting setting, TprBool8* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingBool(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pData = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingStringSize(TprSetting setting, uint32_t* pSize) noexcept {
        if (!pSize) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingStringSize(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_copySettingString(TprSetting setting, char* pData) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->copySettingString(setting, pData);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingDouble(TprSetting setting, double data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingDouble(setting, data);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingInteger(TprSetting setting, int64_t data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingInteger(setting, data);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingBool(TprSetting setting, TprBool8 data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingBool(setting, data);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingString(TprSetting setting, const char* pData) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingString(setting, pData);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingNull(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingNull(setting);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_unsetSetting(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->unsetSetting(setting);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingStruct(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingStruct(setting);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    TprResult TemporEngine::conf_setSettingArray(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingArray(setting);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
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
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingArraySize(setting);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_getSettingArrayElement(TprSetting setting, uint32_t index, TprSetting* pElement) noexcept {
        if (!pElement) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->getSettingArrayElement(setting, index);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pElement = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_resizeSettingArray(TprSetting setting, uint32_t size) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->resizeSettingArray(setting, size);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
#pragma endregion  // conf

#pragma region render
    TprResult TemporEngine::render_createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept {
        if (!pDomain) return TPR_ERROR_INVALID_VALUE;
        if (!mpHWLI) return TPR_MODULE_NOT_LOADED;
        auto exp = mpHWLI->createDepthDomain(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pDomain = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyDepthDomain(TprDepthDomain domain) noexcept {
        if (!mpHWLI) return;
        mpHWLI->destroyDepthDomain(domain);
    }
    TprResult TemporEngine::render_createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept {
        if (!pTarget) return TPR_ERROR_INVALID_VALUE;
        if (!mpHWLI) return TPR_MODULE_NOT_LOADED;
        auto exp = mpHWLI->createRenderTarget(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pTarget = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyRenderTarget(TprRenderTarget target) noexcept {
        if (!mpHWLI) return;
        mpHWLI->destroyRenderTarget(target);
    }
    TprComponent TemporEngine::render_getComponentRenderable() noexcept {
        return mComponentRenderable;  // TODO: make it create a capability every time so that a plugin can't destroy this component
    }
    TprJob TemporEngine::render_getRenderJob() noexcept {
        return mRenderJob;  // TODO: make it create a capability every time so that a plugin can't destroy this job
    }
    TprResult TemporEngine::render_createObjectImage(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) noexcept {
        if (!pImage) return TPR_ERROR_INVALID_VALUE;
        if (!mpHWLI) return TPR_MODULE_NOT_LOADED;
        auto exp = mpHWLI->createObjectImage(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pImage = exp.value();
        return TPR_SUCCESS;
    }
    void TemporEngine::render_destroyObjectImage(TprObjectImage image) noexcept {
        if (!mpHWLI) return;
        mpHWLI->destroyObjectImage(image);
    }
#pragma endregion  // render

#pragma region sched
    TprResult TemporEngine::sched_createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept {
        if (!pJob) return TPR_ERROR_INVALID_VALUE;
        if (!mpSched) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSched->createJob(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pJob = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::sched_createJobCapability(TprJob job, TprJobCapabilityFlags mask, TprJob* pJob) noexcept {
        if (!pJob) return TPR_ERROR_INVALID_VALUE;
        if (!mpSched) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSched->createJobCapability(job, mask);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC: {
                    std::lock_guard<std::mutex> lock(mMainThreadMutex);
                    mRunResult.emplace(TPR_PANIC);
                    return TPR_PANIC;
                }
                default: return exp.error();
            }
        }
        *pJob = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::sched_scheduleJob(TprJob job, uint64_t timepoint) noexcept {
        if (!mpSched) return TPR_MODULE_NOT_LOADED;
        auto result = mpSched->scheduleJob(job, timepoint);
        switch (result) {
            case TPR_PANIC: {
                std::lock_guard<std::mutex> lock(mMainThreadMutex);
                mRunResult.emplace(TPR_PANIC);
                return TPR_PANIC;
            }
            default: return result;
        }
    }
    void TemporEngine::sched_pendJobDestruction(TprJob job) noexcept {
        if (!mpSched) return;
        mpSched->pendJobDestruction(job);
    }
    TprJob TemporEngine::sched_getShutdownJob() noexcept {
        return mShutdownJob;  // TODO: make it create a capability every time so that a plugin can't destroy this job
    }
    uint64_t TemporEngine::sched_now() noexcept {
        if (!mpSched) return 0;
        return mpSched->now();
    }
#pragma endregion  // sched

