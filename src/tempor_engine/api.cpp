
#include "core.hpp"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "tempor.hpp"
#include <exception>


#pragma region log
    void TemporEngine::out_log(TprLogLevel logLevel, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, logLevel);
    }
    void TemporEngine::out_error(const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_ERROR, TPR_LOG_STYLE_ERROR1);
    }
    void TemporEngine::out_warn(const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_WARN, TPR_LOG_STYLE_WARN1);
    }
    void TemporEngine::out_info(const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_INFO);
    }
    void TemporEngine::out_debug(const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_DEBUG);
    }
    void TemporEngine::out_trace(const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_TRACE);
    }
    void TemporEngine::out_logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, logLevel, logStyle);
    }
    void TemporEngine::out_errorStyled(TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_ERROR, logStyle);
    }
    void TemporEngine::out_warnStyled(TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_WARN, logStyle);
    }
    void TemporEngine::out_infoStyled(TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_INFO, logStyle);
    }
    void TemporEngine::out_debugStyled(TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_DEBUG, logStyle);
    }
    void TemporEngine::out_traceStyled(TprLogStyle logStyle, const char* message) noexcept {
        if (!mpLogSink) return;
        mpLogSink->writeLog(message, TPR_LOG_LEVEL_TRACE, logStyle);
    }
    TprResult TemporEngine::out_writeMachineData(const char* pData, uint32_t size) noexcept {
        if (!mpLogSink) return TPR_MODULE_NOT_LOADED;
        return mpLogSink->writeData(std::span(reinterpret_cast<const std::byte*>(pData), size));
    }
#pragma endregion  // log

#pragma region vfs
    TprResult TemporEngine::vfs_openPathResource(const char* path, TprOpenPathResourceFlags flags, TprResource* pResource) noexcept {
        if (!pResource) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->openResource(std::filesystem::path(path), flags);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pResource = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_openReferenceResource(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept {
        if (!pResource) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->openResource(reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pResource = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_openViewResource(const char* begin, const char* end, TprOpenViewResourceFlags flags, TprResource* pResource) noexcept {
        if (!pResource) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->openResource(reinterpret_cast<const std::byte*>(begin), reinterpret_cast<const std::byte*>(end), flags);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pResource = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
        if (!pResource) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->openResource(size, flags, alignment);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pResource = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_openCapabilityResource(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprResourceCapabilityFlags protectFlags, TprResource* pResource) noexcept {
        if (!pResource) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->openResource(protectResource, flags, protectFlags);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pResource = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_resizeResource(TprResource resource, uint64_t newSize) noexcept {
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        return mpResReg->resizeResource(resource, newSize);
    }
    TprResult TemporEngine::vfs_sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
        if (!pSize) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->sizeofResource(resource);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_getResourceRawDataPointer(TprResource resource, char** pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->getResourceRawDataPointer(resource);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pData = reinterpret_cast<char*>(exp.value());
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::vfs_getResourceConstPointer(TprResource resource, const char** pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpResReg) return TPR_MODULE_NOT_LOADED;
        auto exp = mpResReg->getResourceConstPointer(resource);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pData = reinterpret_cast<const char*>(exp.value());
        return TPR_SUCCESS;
    }
    void TemporEngine::vfs_closeResource(TprResource resource) noexcept {
        if (!mpResReg) return;
        mpResReg->closeResource(resource);
    }
#pragma endregion  // vfs

#pragma region input
    TprResult TemporEngine::input_createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) noexcept {
        if (!pAction) return TPR_ERROR_INVALID_VALUE;
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto exp = mpWinMan->createAction(window, pCreateInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::input_getInputElementVector(TprWindow window, TprInputElement element, TprInputElementVector* pVector) noexcept {
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        auto result = mpWinMan->getInputElementVector(window, element, pVector);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
#pragma endregion  // input

#pragma region win
    TprResult TemporEngine::win_openWindow(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) noexcept {
        if (!pWindow) return TPR_ERROR_INVALID_VALUE;
        if (!mpWinMan) return TPR_MODULE_NOT_LOADED;
        TprWindow handle;
        auto exp = mpWinMan->openWindow(pCreateInfo);
        if (exp.has_value()) {
            handle = exp.value();
        } else {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        if (mpHWLI) {
            TprResult r = mpHWLI->registerWindow(handle);
            if (r != TPR_SUCCESS) {
                switch (exp.error()) {
                    case TPR_PANIC:
                        mPanic.store(true);
                        return TPR_PANIC;
                    default:
                        mpWinMan->closeWindow(handle);
                        // TODO: recreate HWLI
                        return exp.error();
                }
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::scene_copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->copyEntityComponentData(entity, component, offset, n, pData);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::scene_writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->writeEntityComponentData(entity, component, pData, offset, n);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::scene_getComponentChunkHandles(TprComponent component, TprResource resource) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->getComponentChunkHandles(component, resource);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pVersion = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::scene_copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->copyComponentChunkData(chunk, offset, n, pData);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::scene_writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept {
        if (!mpSceneGraph) return TPR_MODULE_NOT_LOADED;
        auto result = mpSceneGraph->writeComponentChunkData(chunk, version, pData, offset, n);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pMesh = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::geo_loadMesh(TprMesh mesh, const TprMeshLoadInfo* pInfo) noexcept {
        if (!mpAssetStore) return TPR_MODULE_NOT_LOADED;
        auto result = mpAssetStore->loadMesh(mesh, pInfo);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
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
        try {
            auto infoExp = activePluginInfo();
            if (!infoExp.has_value()) {
                if (infoExp.error() == TPR_PANIC) mPanic.store(true);
                return infoExp.error();
            }
            auto info = infoExp.value();
            auto exp = mpSettings->createSetting(mpSettings->getRoot(), info.name);
            if (!exp.has_value()) {
                switch (exp.error()) {
                    case TPR_SUCCESS:
                        break;
                    case TPR_PANIC:
                        mPanic.store(true);
                        return TPR_PANIC;
                    default:
                        return exp.error();
                }
            }
            TprSetting value = exp.value();
            auto result = mpSettings->setSettingStruct(value);
            switch (result) {
                case TPR_SUCCESS:
                    break;
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return result;
            }
            *pSetting = value;
        } catch (const std::exception& e) {
            mPanic.store(true);
            mLogger->panic() << "Exception at api.conf.createSetting: " << e.what() << "\n";
            return TPR_PANIC;
        } catch (...) {
            mPanic.store(true);
            mLogger->panic() << "Unexpected exception at api.conf.createSetting\n";
            return TPR_PANIC;
        }
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_createSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept {
        if (!pSetting) return TPR_ERROR_INVALID_VALUE;
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        if (!mpPlugLd) return TPR_MODULE_NOT_LOADED;
        auto exp = mpSettings->createSetting(baseSetting, name);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pSize = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_copySettingString(TprSetting setting, char* pData) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->copySettingString(setting, pData);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingDouble(TprSetting setting, double data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingDouble(setting, data);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingInteger(TprSetting setting, int64_t data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingInteger(setting, data);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingBool(TprSetting setting, TprBool8 data) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingBool(setting, data);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingString(TprSetting setting, const char* pData) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingString(setting, pData);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingNull(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingNull(setting);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_unsetSetting(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->unsetSetting(setting);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingStruct(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingStruct(setting);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }
    TprResult TemporEngine::conf_setSettingArray(TprSetting setting) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->setSettingArray(setting);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pElement = exp.value();
        return TPR_SUCCESS;
    }
    TprResult TemporEngine::conf_resizeSettingArray(TprSetting setting, uint32_t size) noexcept {
        if (!mpSettings) return TPR_MODULE_NOT_LOADED;
        auto result = mpSettings->resizeSettingArray(setting, size);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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
        return mComponentRenderable;
    }
    TprResult TemporEngine::render_createObjectImage(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) noexcept {
        if (!pImage) return TPR_ERROR_INVALID_VALUE;
        if (!mpHWLI) return TPR_MODULE_NOT_LOADED;
        auto exp = mpHWLI->createObjectImage(pInfo);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
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

#pragma region thread
    TprResult TemporEngine::thread_createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept {
        if (!pJob) return TPR_ERROR_INVALID_VALUE;
        if (!mpThread) return TPR_MODULE_NOT_LOADED;
        auto exp = mpThread->createJob(pInfo);
        if (!exp.has_value()) return exp.error();
        TprJob job = exp.value();
        try {
            auto pluginIdExp = activePluginID();
            if (!pluginIdExp.has_value()) return pluginIdExp.error();
            auto jobIdExp = mpThread->getJobID(job);
            if (!jobIdExp.has_value()) {
                // for some reason Job that was just created is invalid
                mPanic.store(true);
                mLogger->panic() << "Corrupted memory: Threading.getJobID\n";
                return TPR_PANIC;
            }
            mJobPluginMap.try_emplace(jobIdExp.value(), pluginIdExp.value());
        } catch (...) {
            
        }
        *pJob = job;
        return TPR_SUCCESS;
    }

    TprResult TemporEngine::thread_createDetachedJob(const TprJobCreateInfo* pInfo) noexcept {
        if (!mpThread) return TPR_MODULE_NOT_LOADED;
        auto result = mpThread->createDetachedJob(pInfo);
        switch (result) {
            case TPR_PANIC:
                mPanic.store(true);
                return TPR_PANIC;
            default:
                return result;
        }
    }

    TprResult TemporEngine::thread_jobFinished(TprJob job, TprBool8* pData) noexcept {
        if (!pData) return TPR_ERROR_INVALID_VALUE;
        if (!mpThread) return TPR_MODULE_NOT_LOADED;
        auto exp = mpThread->jobFinished(job);
        if (!exp.has_value()) {
            switch (exp.error()) {
                case TPR_PANIC:
                    mPanic.store(true);
                    return TPR_PANIC;
                default:
                    return exp.error();
            }
        }
        *pData = exp.value();
        return TPR_SUCCESS;
    }

    void TemporEngine::thread_joinJob(TprJob job) noexcept {
        if (!mpThread) return;
        mpThread->joinJob(job);
    }
#pragma endregion  // thread

