

#include "hardware_layer_interface.hpp"
#include "plugin_loader.hpp"
#include "settings.hpp"
#if !defined(__linux__)
    #error "Unsupported OS type"
#endif


// everything low-level, that is a part of bootstrap sequence or is a generally useful helper is in snake_case or ALL_CAPS
// everything else is in camelCase, PascalCase or ALL_CAPS


#include "core.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "tempor.hpp"
#include "arg_parser.hpp"

#include <cstdio>
#include <csignal>


namespace {
    TemporEngine* g_engine = nullptr;
}

namespace api {

    namespace log {
        void log(TprLogLevel logLevel, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->log(logLevel) << message;
        }

        void error(const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->error(TPR_LOG_STYLE_ERROR1) << message;
        }

        void warn(const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->warn(TPR_LOG_STYLE_WARN1) << message;
        }

        void info(const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->info() << message;
        }

        void debug(const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->debug() << message;
        }

        void trace(const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->trace() << message;
        }

        void logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->log(logLevel, logStyle) << message;
        }

        void errorStyled(TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->error(logStyle) << message;
        }

        void warnStyled(TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->warn(logStyle) << message;
        }

        void infoStyled(TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->info(logStyle) << message;
        }

        void debugStyled(TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->debug(logStyle) << message;
        }

        void traceStyled(TprLogStyle logStyle, const char* message) noexcept {
            Logger* logger = g_engine->getLogger();
            if (!logger) return;
            logger->trace(logStyle) << message;
        }
    }

    namespace vfs {
        TprResult openPathResource(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            if (!pResource) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->openResource(std::filesystem::path(path), flags, alignment);
            if (!exp.has_value()) return exp.error();
            *pResource = exp.value();
            return TPR_SUCCESS;
        }

        TprResult openReferenceResource(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept {
            if (!pResource) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->openResource(reinterpret_cast<std::byte*>(begin), reinterpret_cast<std::byte*>(end), flags);
            if (!exp.has_value()) return exp.error();
            *pResource = exp.value();
            return TPR_SUCCESS;
        }

        TprResult openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept {
            if (!pResource) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->openResource(size, flags, alignment);
            if (!exp.has_value()) return exp.error();
            *pResource = exp.value();
            return TPR_SUCCESS;
        }

        TprResult openCapabilityResource(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept {
            if (!pResource) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->openResource(protectResource, flags, protectFlags);
            if (!exp.has_value()) return exp.error();
            *pResource = exp.value();
            return TPR_SUCCESS;
        }

        TprResult resizeResource(TprResource resource, uint64_t newSize) noexcept {
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            return rreg->resizeResource(resource, newSize);
        }

        TprResult sizeofResource(TprResource resource, uint64_t* pSize) noexcept {
            if (!pSize) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->sizeofResource(resource);
            if (!exp.has_value()) return exp.error();
            *pSize = exp.value();
            return TPR_SUCCESS;
        }

        TprResult getResourceRawDataPointer(TprResource resource, char** pData) noexcept {
            if (!pData) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->getResourceRawDataPointer(resource);
            if (!exp.has_value()) return exp.error();
            *pData = reinterpret_cast<char*>(exp.value());
            return TPR_SUCCESS;
        }

        TprResult getResourceConstPointer(TprResource resource, const char** pData) noexcept {
            if (!pData) return TPR_INVALID_VALUE;
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return TPR_UNKNOWN_ERROR;
            auto exp = rreg->getResourceConstPointer(resource);
            if (!exp.has_value()) return exp.error();
            *pData = reinterpret_cast<const char*>(exp.value());
            return TPR_SUCCESS;
        }

        void closeResource(TprResource resource) noexcept {
            ResourceRegistry* rreg = g_engine->getResourceRegistry();
            if (!rreg) return;
            rreg->closeResource(resource);
        }

    }

    namespace input {
        TprResult createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) noexcept {
            if (!pAction) return TPR_INVALID_VALUE;
            WindowManager* win = g_engine->getWindowManager();
            if (!win) return TPR_UNKNOWN_ERROR;
            auto exp = win->createAction(window, pCreateInfo);
            if (exp.has_value()) {
                *pAction = exp.value();
            } else {
                return exp.error();
            }
            return TPR_SUCCESS;
        }

        void destroyAction(TprAction action) noexcept {
            WindowManager* win = g_engine->getWindowManager();
            if (!win) return;
            win->destroyAction(action);
        }

        TprResult getActionState(TprAction action, TprActionState* pState) noexcept {
            if (!pState) return TPR_INVALID_VALUE;
            WindowManager* win = g_engine->getWindowManager();
            if (!win) return TPR_UNKNOWN_ERROR;
            return win->getActionState(action, pState);
        }

        TprResult getInputElementVector(TprWindow window, TprInputElement element, TprInputElementVector* pVector) noexcept {
            WindowManager* win = g_engine->getWindowManager();
            if (!win) return TPR_UNKNOWN_ERROR;
            return win->getInputElementVector(window, element, pVector);
        }
    }

    namespace wm {
        TprResult openWindow(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) noexcept {
            if (!pWindow) return TPR_INVALID_VALUE;
            WindowManager* win = g_engine->getWindowManager();
            if (!win) return TPR_UNKNOWN_ERROR;
            TprWindow handle;
            auto exp = win->openWindow(pCreateInfo);
            if (exp.has_value()) {
                handle = exp.value();
            } else {
                return exp.error();
            }
            HardwareLayer* phwl = g_engine->getPHWL();
            if (phwl) {
                TprResult r = phwl->registerWindow(handle);
                // TODO: add HWLI recreation when result is TPR_INSUFFICIENT_INIT
                if (r < 0) {
                    win->closeWindow(handle);
                    return r;
                }
            }
            *pWindow = handle;
            return TPR_SUCCESS;
        }

        void closeWindow(TprWindow window) noexcept {
            HardwareLayer* phwl = g_engine->getPHWL();
            if (phwl) phwl->unregisterWindow(window);
            WindowManager* win = g_engine->getWindowManager();
            if (win) win->closeWindow(window);
        }
    }

    namespace scene {
        TprResult createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept {
            if (!pComponent) return TPR_INVALID_VALUE;
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            auto exp = scgr->createComponent(componentSize);
            if (!exp.has_value()) {
                return exp.error();
            }
            *pComponent = exp.value();
            return TPR_SUCCESS;
        }

        void destroyComponent(TprComponent component) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return;
            scgr->destroyComponent(component);
        }

        TprResult createEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept {
            if (!pEntity) return TPR_INVALID_VALUE;
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            auto exp = scgr->spawnEntity(pComponents, componentCount);
            if (!exp.has_value()) {
                return exp.error();
            }
            *pEntity = exp.value();
            return TPR_SUCCESS;
        }

        void destroyEntity(TprEntity entity) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return;
            scgr->killEntity(entity);
        }

        TprResult modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->modifyEntityComponentSet(entity, pComponents, componentCount);
        }

        TprResult copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->copyEntityComponentData(entity, component, offset, n, pData);
        }

        TprResult writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->writeEntityComponentData(entity, component, pData, offset, n);
        }

        TprResult getComponentChunkHandles(TprComponent component, TprResource resource) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->getComponentChunkHandles(component, resource);
        }

        uint32_t getComponentChunkMaxElementCount() noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return 0;
            return scgr->getComponentChunkMaxElementCount();
        }

        TprResult getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept {
            if (!pCount) return TPR_INVALID_VALUE;
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            auto exp = scgr->getComponentChunkElementCount(chunk);
            if (exp.has_value()) {
                *pCount = exp.value();
                return TPR_SUCCESS;
            }
            return exp.error();
        }

        TprResult getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept {
            if (!pVersion) return TPR_INVALID_VALUE;
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            auto exp = scgr->getComponentChunkVersion(chunk);
            if (exp.has_value()) {
                *pVersion = exp.value();
                return TPR_SUCCESS;
            }
            return exp.error();
        }

        TprResult copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->copyComponentChunkData(chunk, offset, n, pData);
        }

        TprResult writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept {
            SceneGraph* scgr = g_engine->getSceneGraph();
            if (!scgr) return TPR_UNKNOWN_ERROR;
            return scgr->writeComponentChunkData(chunk, version, pData, offset, n);
        }
    }

    namespace geo {
        TprResult createMesh(const TprMeshCreateInfo* pInfo, TprMesh* pMesh) noexcept {
            if (!pMesh) return TPR_INVALID_VALUE;
            AssetStore* astr = g_engine->getAssetStore();
            if (!astr) return TPR_UNKNOWN_ERROR;
            auto exp = astr->createMesh(pInfo);
            if (!exp.has_value()) {
                return exp.error();
            }
            *pMesh = exp.value();
            return TPR_SUCCESS;
        }

        TprResult loadMesh(TprMesh mesh, const TprMeshLoadInfo* pInfo) noexcept {
            AssetStore* astr = g_engine->getAssetStore();
            if (!astr) return TPR_UNKNOWN_ERROR;
            return astr->loadMesh(mesh, pInfo);
        }

        void unloadMesh(TprMesh mesh) noexcept {
            AssetStore* astr = g_engine->getAssetStore();
            if (!astr) return;
            astr->unloadMesh(mesh);
        }

        void destroyMesh(TprMesh mesh) noexcept {
            AssetStore* astr = g_engine->getAssetStore();
            if (!astr) return;
            astr->destroyMesh(mesh);
        }
    }

    namespace conf {
        TprResult createSetting(const char* name, TprSetting* pSetting) noexcept {
            if (!pSetting) return TPR_INVALID_VALUE;
            PluginLoader* plLd = g_engine->getPluginLoader();
            if (!plLd) return TPR_UNKNOWN_ERROR;
            auto exp = plLd->createSetting(name);
            if (!exp.has_value()) return exp.error();
            *pSetting = exp.value();
            return TPR_SUCCESS;
        }
        void destroySetting(TprSetting setting) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return;
            set->destroySetting(setting);
        }
        TprResult getSettingType(TprSetting setting, TprSettingType* pType) noexcept {
            if (!pType) return TPR_INVALID_VALUE;
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            auto exp = set->getSettingType(setting);
            if (!exp.has_value()) return exp.error();
            *pType = exp.value();
            return TPR_SUCCESS;
        }
        TprResult getSettingDouble(TprSetting setting, double* pData) noexcept {
            if (!pData) return TPR_INVALID_VALUE;
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            auto exp = set->getSettingDouble(setting);
            if (!exp.has_value()) return exp.error();
            *pData = exp.value();
            return TPR_SUCCESS;
        }
        TprResult getSettingInteger(TprSetting setting, int64_t* pData) noexcept {
            if (!pData) return TPR_INVALID_VALUE;
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            auto exp = set->getSettingInteger(setting);
            if (!exp.has_value()) return exp.error();
            *pData = exp.value();
            return TPR_SUCCESS;
        }
        TprResult getSettingBool(TprSetting setting, TprBool8* pData) noexcept {
            if (!pData) return TPR_INVALID_VALUE;
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            auto exp = set->getSettingBool(setting);
            if (!exp.has_value()) return exp.error();
            *pData = exp.value();
            return TPR_SUCCESS;
        }
        TprResult getSettingStringSize(TprSetting setting, uint32_t* pSize) noexcept {
            if (!pSize) return TPR_INVALID_VALUE;
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            auto exp = set->getSettingStringSize(setting);
            if (!exp.has_value()) return exp.error();
            *pSize = exp.value();
            return TPR_SUCCESS;
        }
        TprResult copySettingString(TprSetting setting, char* pData) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->copySettingString(setting, pData);
        }
        TprResult setSettingDouble(TprSetting setting, double data) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->setSettingDouble(setting, data);
        }
        TprResult setSettingInteger(TprSetting setting, int64_t data) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->setSettingInteger(setting, data);
        }
        TprResult setSettingBool(TprSetting setting, TprBool8 data) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->setSettingBool(setting, data);
        }
        TprResult setSettingString(TprSetting setting, const char* pData) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->setSettingString(setting, pData);
        }
        TprResult setSettingNull(TprSetting setting) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return TPR_UNKNOWN_ERROR;
            return set->setSettingNull(setting);
        }
        double getSettingDoubleOr(TprSetting setting, double fallback) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return fallback;
            return set->getSettingDoubleOr(setting, fallback);
        }
        int64_t getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return fallback;
            return set->getSettingIntegerOr(setting, fallback);
        }
        TprBool8 getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept {
            Settings* set = g_engine->getSettings();
            if (!set) return fallback;
            return set->getSettingBoolOr(setting, fallback);
        }
    }

    namespace render {
        TprResult createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept {
            if (!pDomain) return TPR_INVALID_VALUE;
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return TPR_UNKNOWN_ERROR;
            auto exp = hwl->createDepthDomain(pInfo);
            if (!exp.has_value()) return exp.error();
            *pDomain = exp.value();
            return TPR_SUCCESS;
        }
        void destroyDepthDomain(TprDepthDomain domain) noexcept {
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return;
            hwl->destroyDepthDomain(domain);
        }
        TprResult createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept {
            if (!pTarget) return TPR_INVALID_VALUE;
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return TPR_UNKNOWN_ERROR;
            auto exp = hwl->createRenderTarget(pInfo);
            if (!exp.has_value()) return exp.error();
            *pTarget = exp.value();
            return TPR_SUCCESS;
        }
        void destroyRenderTarget(TprRenderTarget target) noexcept {
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return;
            hwl->destroyRenderTarget(target);
        }
        TprComponent getComponentRenderable() noexcept {
            return g_engine->getComponentRenderable();
        }
        TprResult createObjectImage(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) noexcept {
            if (!pImage) return TPR_INVALID_VALUE;
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return TPR_UNKNOWN_ERROR;
            auto exp = hwl->createObjectImage(pInfo);
            if (!exp.has_value()) return exp.error();
            *pImage = exp.value();
            return TPR_SUCCESS;
        }
        void destroyObjectImage(TprObjectImage image) noexcept {
            HardwareLayer* hwl = g_engine->getPHWL();
            if (!hwl) return;
            hwl->destroyObjectImage(image);
        }
    }
}


enum class bootstrap_err_code : int {
    success = 0,
    engine_lifetime_violation = 1,
    unhandled_exception = 2,
    invalid_argv = 3
};


void sigint_handler(int) noexcept {
    if (g_engine) g_engine->sigint();
}
void sigterm_handler(int) noexcept {
    if (g_engine) g_engine->sigterm();
}


int main(int argc, char* argv[]) {

    size_t verbose_level = 0;

    TprEngineAPI::Log apiLog;
    apiLog.log = api::log::log;
    apiLog.info = api::log::info;
    apiLog.warn = api::log::warn;
    apiLog.error = api::log::error;
    apiLog.debug = api::log::debug;
    apiLog.trace = api::log::trace;
    apiLog.logStyled = api::log::logStyled;
    apiLog.infoStyled = api::log::infoStyled;
    apiLog.warnStyled = api::log::warnStyled;
    apiLog.errorStyled = api::log::errorStyled;
    apiLog.debugStyled = api::log::debugStyled;
    apiLog.traceStyled = api::log::traceStyled;

    TprEngineAPI::VFS apiVFS;
    apiVFS.openPathResource = api::vfs::openPathResource;
    apiVFS.openReferenceResource = api::vfs::openReferenceResource;
    apiVFS.openEmptyResource = api::vfs::openEmptyResource;
    apiVFS.openCapabilityResource = api::vfs::openCapabilityResource;
    apiVFS.resizeResource = api::vfs::resizeResource;
    apiVFS.sizeofResource = api::vfs::sizeofResource;
    apiVFS.getResourceRawDataPointer = api::vfs::getResourceRawDataPointer;
    apiVFS.getResourceConstPointer = api::vfs::getResourceConstPointer;
    apiVFS.closeResource = api::vfs::closeResource;

    TprEngineAPI::Scene apiScene;
    apiScene.createComponent = api::scene::createComponent;
    apiScene.destroyComponent = api::scene::destroyComponent;
    apiScene.spawnEntity = api::scene::createEntity;
    apiScene.killEntity = api::scene::destroyEntity;
    apiScene.modifyEntityComponentSet = api::scene::modifyEntityComponentSet;
    apiScene.copyEntityComponentData = api::scene::copyEntityComponentData;
    apiScene.writeEntityComponentData = api::scene::writeEntityComponentData;
    apiScene.getComponentChunkHandles = api::scene::getComponentChunkHandles;
    apiScene.getComponentChunkMaxElementCount = api::scene::getComponentChunkMaxElementCount;
    apiScene.getComponentChunkElementCount = api::scene::getComponentChunkElementCount;
    apiScene.getComponentChunkVersion = api::scene::getComponentChunkVersion;
    apiScene.copyComponentChunkData = api::scene::copyComponentChunkData;
    apiScene.writeComponentChunkData = api::scene::writeComponentChunkData;

    TprEngineAPI::Geo apiGeo;
    apiGeo.createMesh = api::geo::createMesh;
    apiGeo.loadMesh = api::geo::loadMesh;
    apiGeo.unloadMesh = api::geo::unloadMesh;
    apiGeo.destroyMesh = api::geo::destroyMesh;
    
    TprEngineAPI::WM apiWM;
    apiWM.openWindow = api::wm::openWindow;
    apiWM.closeWindow = api::wm::closeWindow;

    TprEngineAPI::Input apiInput;
    apiInput.getInputElementVector = api::input::getInputElementVector;
    apiInput.createAction = api::input::createAction;
    apiInput.destroyAction = api::input::destroyAction;
    apiInput.getActionState = api::input::getActionState;

    TprEngineAPI::Conf apiConf;
    apiConf.createSetting = api::conf::createSetting;
    apiConf.destroySetting = api::conf::destroySetting;
    apiConf.getSettingType = api::conf::getSettingType;
    apiConf.getSettingDouble = api::conf::getSettingDouble;
    apiConf.getSettingInteger = api::conf::getSettingInteger;
    apiConf.getSettingBool = api::conf::getSettingBool;
    apiConf.getSettingStringSize = api::conf::getSettingStringSize;
    apiConf.copySettingString = api::conf::copySettingString;
    apiConf.setSettingString = api::conf::setSettingString;
    apiConf.setSettingBool = api::conf::setSettingBool;
    apiConf.setSettingDouble = api::conf::setSettingDouble;
    apiConf.setSettingInteger = api::conf::setSettingInteger;
    apiConf.setSettingNull = api::conf::setSettingNull;
    apiConf.getSettingDoubleOr = api::conf::getSettingDoubleOr;
    apiConf.getSettingIntegerOr = api::conf::getSettingIntegerOr;
    apiConf.getSettingBoolOr = api::conf::getSettingBoolOr;

    TprEngineAPI::Render apiRender;
    apiRender.createDepthDomain = api::render::createDepthDomain;
    apiRender.destroyDepthDomain = api::render::destroyDepthDomain;
    apiRender.createRenderTarget = api::render::createRenderTarget;
    apiRender.destroyRenderTarget = api::render::destroyRenderTarget;
    apiRender.getComponentRenderable = api::render::getComponentRenderable;
    apiRender.createObjectImage = api::render::createObjectImage;
    apiRender.destroyObjectImage = api::render::destroyObjectImage;

    TprEngineAPI api;
    api.log = &apiLog;
    api.wm = &apiWM;
    api.vfs = &apiVFS;
    api.scene = &apiScene;
    api.geo = &apiGeo;
    api.input = &apiInput;
    api.conf = &apiConf;
    api.render = &apiRender;

    try {

        std::ios::sync_with_stdio(false);
        std::signal(SIGINT, sigint_handler);
        std::signal(SIGTERM, sigterm_handler);

        arg_parser parser{};

        auto root_h = parser.define_flag('h', {}, 0, nullptr, "Shows help message. -h: simplified, -hh: advanced");
        auto root_help = parser.define_flag(0, "help", 0, nullptr, "Shows advanced help message");
        auto root_v = parser.define_flag('v', {}, 0, nullptr, "Sets runtime log verbosity. -v: 3, -vv: 4, -vvv: 5");
        auto root_verbose = parser.define_flag(0, "verbose", FLAG_HAS_VALUE_FLAG_BIT, nullptr, "Sets runtime log verbosity [0-5]. Overrides -v");

        parser.parse(argc, argv);

        if (root_help.present()) {
            parser.print_help_advanced("Tempor - a game engine", "tempor");
            return 0;
        }
        if (root_h.count() == 1) {
            parser.print_help("tempor");
            return 0;
        } else if (root_h.count() >= 2) {
            parser.print_help_advanced("Tempor - a game engine", "tempor");
            return 0;
        }

        if (root_v.count() == 1) {
            verbose_level = 3;
        } else if (root_v.count() == 2) {
            verbose_level = 4;
        } else if (root_v.count() >= 3) {
            verbose_level = 5;
        }
        if (root_verbose.present()) {
            verbose_level = root_verbose.value<size_t>(root_verbose.count() - 1);
            if (verbose_level > 5) {
                std::fprintf(stderr, "--verbose value is not in [0-5]: %ld\n", verbose_level);
                return to_underlying(bootstrap_err_code::invalid_argv);
            }
        }

    } catch (const std::exception& e) {
        std::printf("Parsing error: %s\n", e.what());
    }

    /*
            END OF STDIO-ONLY REGION
    */
    std::fflush(stdout);

    alignas(TemporEngine) static std::byte engine_mem[sizeof(TemporEngine)];

    try {
        g_engine = new (engine_mem) TemporEngine(verbose_level, &api);

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to construct engine: %s\n", e.what());
        std::fflush(stderr);
        return to_underlying(bootstrap_err_code::engine_lifetime_violation);
    } catch (...) {
        std::fprintf(stderr, "Failed to construct engine\n");
        std::fflush(stderr);
        return to_underlying(bootstrap_err_code::engine_lifetime_violation);
    }

    int exit_code = 0;

    try {

        g_engine->init();
        exit_code = !g_engine->run() ? 0 : to_underlying(bootstrap_err_code::unhandled_exception);
        g_engine->shutdown();

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Unhandled exception detected: %s\n", e.what());
        std::fflush(stderr);
        exit_code = to_underlying(bootstrap_err_code::unhandled_exception);
    } catch (...) {
        std::fprintf(stderr, "Unhandled exception detected\n");
        std::fflush(stderr);
        exit_code = to_underlying(bootstrap_err_code::unhandled_exception);
    }

    g_engine->~TemporEngine();

    return exit_code;
}
