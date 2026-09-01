

#ifndef TEMPOR_ENGINE_TEMPOR_HPP_
#define TEMPOR_ENGINE_TEMPOR_HPP_


#include "core.hpp"

#include "plugin.h"
#include "plugin_core.h"

#include "plugin_loader.hpp"
#include "scene_graph.hpp"
#include "logger.hpp"
#include "windowing.hpp"
#include "file_registry.hpp"
#include "i_graphics_device.hpp"
#include "asset_store.hpp"
#include "settings.hpp"
#include "scheduler.hpp"
#include "log_entry.hpp"
#include "output_sink.hpp"

#include <atomic>
#include <csignal>
#include <optional>
#include <stdexcept>
#include <cassert>
#include <unordered_map>


template <typename T>
struct service_buffer {
    public:
        service_buffer() = default;
        service_buffer(const service_buffer&) = delete;
        service_buffer(service_buffer&&) = delete;
        service_buffer& operator=(const service_buffer&) = delete;
        service_buffer& operator=(service_buffer&&) = delete;

        template <typename... Args>
        T& construct(Args&&... args) {
            if (!m_alive) {
                new (m_memory) T(std::forward<Args>(args)...);
                m_alive = true;
                return get();

            } else {
                throw std::runtime_error("service_singleton_holder: service already constructed: " + type_name<T>::value);
            }
        }

        void destruct() noexcept {
            if (m_alive) {
                get().~T();
                m_alive = false;
            }
        }

        bool alive() const { return m_alive; }

        T& get() {
            assert(m_alive);
            return *std::launder(reinterpret_cast<T*>(m_memory));
        }
        const T& get() const {
            assert(m_alive);
            return *std::launder(reinterpret_cast<T*>(m_memory));
        }

        ~service_buffer() {
            if (m_alive) get().~T();
        }

    private:
        alignas(T) std::byte m_memory[sizeof(T)];
        bool m_alive = false;
};

template<typename...>
struct are_unique : std::true_type {};

template<typename T, typename... Rest>
struct are_unique<T, Rest...> : std::bool_constant<(!std::is_same_v<T, Rest> && ...) && are_unique<Rest...>::value> {};

template <typename... Ts>
class service_singleton_holder : public service_buffer<Ts>... {
    public:
        service_singleton_holder() {
            static_assert((are_unique<Ts...>::value), "service_singleton_holder: all types must be unique");
            static_assert((std::is_nothrow_destructible_v<Ts> && ...), "service_singleton_holder: all types must have a non-throwing destructor");
        }

        service_singleton_holder(const service_singleton_holder&) = delete;
        service_singleton_holder(service_singleton_holder&&) = delete;
        service_singleton_holder& operator=(const service_singleton_holder&) = delete;
        service_singleton_holder& operator=(service_singleton_holder&&) = delete;

        template <typename T, typename... Args>
        T& construct(Args&&... args) {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            T& ref = service_buffer<T>::construct(std::forward<Args>(args)...);
            return ref;
        }

        template <typename T>
        void destruct() noexcept {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            service_buffer<T>::destruct();
        }

        template <typename T>
        T& get() {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            return service_buffer<T>::get();
        }

        template <typename T>
        const T& get() const {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            return service_buffer<T>::get();
        }

        template <typename T>
        bool alive() const { return service_buffer<T>::alive(); }

    private:
        template<typename T> static constexpr bool contains_v = (std::is_same_v<T, Ts> || ...);

};


class TemporEngine {

    public:
        TemporEngine(
            size_t verboseLevel, std::filesystem::path configPath, bool flushConfig,
            bool configEnabled, bool colourEnabled
        );
        int runtime();
        ~TemporEngine() noexcept;

        void signal(int sig) noexcept;

        // ========== API ==========
        #pragma region api
            // out
            void out_log(TprLogLevel logLevel, const char* message) noexcept;
            void out_info(const char* message) noexcept;
            void out_warn(const char* message) noexcept;
            void out_error(const char* message) noexcept;
            void out_debug(const char* message) noexcept;
            void out_trace(const char* message) noexcept;
            void out_logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept;
            void out_infoStyled(TprLogStyle logStyle, const char* message) noexcept;
            void out_warnStyled(TprLogStyle logStyle, const char* message) noexcept;
            void out_errorStyled(TprLogStyle logStyle, const char* message) noexcept;
            void out_debugStyled(TprLogStyle logStyle, const char* message) noexcept;
            void out_traceStyled(TprLogStyle logStyle, const char* message) noexcept;
            TprResult out_writeMachineData(const char* pData, uint32_t size) noexcept;
            // scene
            TprResult scene_createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept;
            void scene_destroyComponent(TprComponent component) noexcept;
            TprResult scene_spawnEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept;
            void scene_killEntity(TprEntity entity) noexcept;
            TprResult scene_modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept;
            TprResult scene_copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) noexcept;
            TprResult scene_writeEntityComponentData(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) noexcept;
            TprResult scene_getComponentChunkHandles(TprComponent component, TprFile resource) noexcept;
            uint32_t scene_getComponentChunkMaxElementCount() noexcept;
            TprResult scene_getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept;
            TprResult scene_getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept;
            TprResult scene_copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept;
            TprResult scene_writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept;
            // fs
            TprResult fs_openFile(const char* path, TprOpenFileFlags flags, TprFile* pFile) noexcept;
            TprResult fs_createMemoryFile(TprFile* pFile) noexcept;
            TprResult fs_forkFile(TprFile file, TprFile* pFile) noexcept;
            TprResult fs_createFileCapability(TprFile file, TprFileCapabilityFlags mask, TprFile* pFile) noexcept;
            void fs_closeFile(TprFile file) noexcept;
            TprResult fs_seek(TprFile file, int32_t offset, TprSeekWhence whence) noexcept;
            TprResult fs_tell(TprFile file, uint32_t* pPos) noexcept;
            TprResult fs_read(TprFile file, uint32_t n, char* pData) noexcept;
            TprResult fs_readAt(TprFile file, uint32_t pos, uint32_t n, char* pData) noexcept;
            TprResult fs_resize(TprFile file, uint32_t newSize) noexcept;
            TprResult fs_write(TprFile file, uint32_t n, const char* pData) noexcept;
            TprResult fs_writeAt(TprFile file, uint32_t pos, uint32_t n, const char* pData) noexcept;
            TprResult fs_pathType(const char* path, TprPathType* pType) noexcept;
            TprResult fs_createDirectory(const char* path, TprCreateDirectoryFlags flags) noexcept;
            TprResult fs_touchFile(const char* path, TprTouchFileFlags flags) noexcept;
            TprResult fs_remove(const char* path) noexcept;
            TprResult fs_move(const char* path, const char* newPath) noexcept;
            // win
            TprResult win_openWindow(const TprWindowCreateInfo* pInfo, TprWindow* pWindow) noexcept;
            TprResult win_createWindowCapability(TprWindow window, TprWindowCapabilityFlags mask, TprWindow* pWindow) noexcept;
            void win_closeWindow(TprWindow window) noexcept;
            TprResult win_createAction(const TprActionCreateInfo* pInfo, TprAction* pAction) noexcept;
            TprResult win_createActionCapability(TprAction action, TprActionCapabilityFlags mask, TprAction* pAction) noexcept;
            void win_destroyAction(TprAction action) noexcept;
            TprResult win_getActionsHistorySize(uint32_t filterCount, const TprAction* pFilters, uint32_t* pSize) noexcept;
            TprResult win_copyActionsHistory(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) noexcept;
            TprResult win_getActionState(TprAction action, TprActionState* pState) noexcept;
            TprJob win_getInputUpdateJob() noexcept;
            // geo
            TprResult geo_createMesh(const TprMeshCreateInfo* pInfo, TprMesh* pMesh) noexcept;
            TprResult geo_createMeshCapability(TprMesh mesh, TprMeshCapabilityFlags mask, TprMesh* pMesh) noexcept;
            void geo_destroyMesh(TprMesh mesh) noexcept;
            TprResult geo_requireMeshLoaded(TprMesh mesh) noexcept;
            TprResult geo_unrequireMeshLoaded(TprMesh mesh) noexcept;
            // conf
            TprResult conf_getRootSetting(TprSetting* pSetting) noexcept;
            TprResult conf_createSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept;
            TprResult conf_readSetting(TprSetting baseSetting, const char* name, TprSetting* pSetting) noexcept;
            void conf_destroySetting(TprSetting pSetting) noexcept;
            TprResult conf_getSettingType(TprSetting setting, TprSettingType* pType) noexcept;
            TprResult conf_getSettingDouble(TprSetting setting, double* pData) noexcept;
            TprResult conf_getSettingInteger(TprSetting setting, int64_t* pData) noexcept;
            TprResult conf_getSettingBool(TprSetting setting, TprBool8* pData) noexcept;
            double conf_getSettingDoubleOr(TprSetting setting, double fallback) noexcept;
            int64_t conf_getSettingIntegerOr(TprSetting setting, int64_t fallback) noexcept;
            TprBool8 conf_getSettingBoolOr(TprSetting setting, TprBool8 fallback) noexcept;
            TprResult conf_getSettingStringSize(TprSetting setting, uint32_t* pSize) noexcept;
            TprResult conf_copySettingString(TprSetting setting, char* pData) noexcept;
            TprResult conf_setSettingDouble(TprSetting setting, double data) noexcept;
            TprResult conf_setSettingInteger(TprSetting setting, int64_t data) noexcept;
            TprResult conf_setSettingBool(TprSetting setting, TprBool8 data) noexcept;
            TprResult conf_setSettingString(TprSetting setting, const char* pData) noexcept;
            TprResult conf_setSettingNull(TprSetting setting) noexcept;
            TprResult conf_unsetSetting(TprSetting setting) noexcept;
            TprResult conf_setSettingStruct(TprSetting setting) noexcept;
            TprResult conf_setSettingArray(TprSetting setting) noexcept;
            TprResult conf_getSettingArraySize(TprSetting setting, uint32_t* pSize) noexcept;
            TprResult conf_getSettingArrayElement(TprSetting setting, uint32_t index, TprSetting* pElement) noexcept;
            TprResult conf_resizeSettingArray(TprSetting setting, uint32_t size) noexcept;
            // render
            TprResult render_createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept;
            TprResult render_createDepthDomainCapability(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask, TprDepthDomain* pDomain) noexcept;
            void render_destroyDepthDomain(TprDepthDomain domain) noexcept;
            TprResult render_createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept;
            TprResult render_createRenderTargetCapability(TprRenderTarget target, TprRenderTargetCapabilityFlags mask, TprRenderTarget* pTarget) noexcept;
            void render_destroyRenderTarget(TprRenderTarget target) noexcept;
            TprResult render_createRenderTargetSet(const TprRenderTargetSetCreateInfo* pInfo, TprRenderTargetSet* pSet) noexcept;
            TprResult render_createRenderTargetSetCapability(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask, TprRenderTargetSet* pSet) noexcept;
            void render_destroyRenderTargetSet(TprRenderTargetSet set) noexcept;
            TprResult render_createEntityImage(const TprEntityImageCreateInfo* pInfo, TprEntityImage* pImage) noexcept;
            TprResult render_createEntityImageCapability(TprEntityImage image, TprEntityImageCapabilityFlags mask, TprEntityImage* pImage) noexcept;
            void render_destroyEntityImage(TprEntityImage image) noexcept;
            TprJob render_getRenderJob() noexcept;
            TprJob render_getRenderSignalJob() noexcept;
            TprComponent render_getComponentRenderable() noexcept;
            // sched
            TprResult sched_createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept;
            TprResult sched_createJobCapability(TprJob job, TprJobCapabilityFlags mask, TprJob* pJob) noexcept;
            TprResult sched_scheduleJob(TprJob job, uint64_t timepoint) noexcept;
            void sched_pendJobDestruction(TprJob job) noexcept;
            TprJob sched_getShutdownJob() noexcept;
            uint64_t sched_now() noexcept;
        #pragma endregion  // api

    private:
        TprEngineAPI::Output mOutAPI{};
        TprEngineAPI::FileSystem mFSAPI{};
        TprEngineAPI::Scene mSceneAPI{};
        TprEngineAPI::Geometry mGeoAPI{};
        TprEngineAPI::Windowing mWinAPI{};
        TprEngineAPI::Configuration mConfAPI{};
        TprEngineAPI::Render mRenderAPI{};
        TprEngineAPI::Scheduling mSchedAPI{};
        TprEngineAPI mAPI{};

        template <typename T, typename... Args>
        T& constructService(Args&&... args) {
            T& r = mServHolder.construct<T>(std::forward<Args>(args)...);
            if (mLogger.has_value()) {
                mLogger->info(TPR_LOG_STYLE_TIMESTAMP1) << "Constructed service " << type_name_v<T> << " (" << type_name_v_s<T> << ")";
            }
            return r;
        }

        template <typename T>
        void destructService() {
            mServHolder.destruct<T>();
            if (mLogger.has_value()) {
                mLogger->info(TPR_LOG_STYLE_TIMESTAMP1) << "Destructed service " << type_name_v<T> << " (" << type_name_v_s<T> << ")";
            }
        }

        void registerAPI();
        expected<uint32_t, TprResult> activePluginID();
        expected<PluginInfo, TprResult> activePluginInfo();

        service_singleton_holder<
            Windowing, PGraphicsDevice, AssetStore, SceneGraph, PluginLoader,
            FileRegistry, Settings, Scheduler, OutputSink
        > mServHolder;

        std::filesystem::path mConfigPath;
        bool mFlushConfig;
        bool mConfigEnabled;
        bool mAllowTermColour;
        TprLogLevel mTermLevel;

        FileRegistry* mpFileReg = nullptr;
        Windowing* mpWindowing = nullptr;
        IGraphicsDevice* mpGDev = nullptr;
        AssetStore* mpAssetStore = nullptr;
        PluginLoader* mpPlugLd = nullptr;
        SceneGraph* mpSceneGraph = nullptr;
        Settings* mpSettings = nullptr;
        Scheduler* mpSched = nullptr;
        OutputSink* mpOutSink = nullptr;

        volatile sig_atomic_t mSignal = 0;
        std::atomic<TprResult> mRunResult = _TPR_RESULT_MAX_ENUM;

        std::unordered_map<uint64_t, uint32_t> mJobPluginMap;

        std::optional<Logger> mLogger;

};



#endif  // TEMPOR_ENGINE_TEMPOR_HPP_
