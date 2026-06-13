

#ifndef TEMPOR_ENGINE_TEMPOR_HPP_
#define TEMPOR_ENGINE_TEMPOR_HPP_


#include "core.hpp"

#include "plugin.h"
#include "plugin_core.h"

#include "plugin_loader.hpp"
#include "scene_graph.hpp"
#include "logger.hpp"
#include "window_manager.hpp"
#include "resource_registry.hpp"
#include "hardware_layer_interface.hpp"
#include "asset_store.hpp"
#include "settings.hpp"
#include "threading.hpp"

#include "sleep_clock.hpp"

#include <csignal>
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
        T& construct(Args&&... args) noexcept(false) {
            if (!m_alive) {
                new (m_memory) T(std::forward<Args>(args)...);
                m_alive = true;
                return get();

            } else {
                throw std::runtime_error("service_singleton_holder: service already constructed: " + type_name<T>::value);
            }
        }

        void destruct() noexcept(true) {
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
        T& construct(Args&&... args) noexcept(false) {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            T& ref = service_buffer<T>::construct(std::forward<Args>(args)...);
            if constexpr (contains_v<Logger>) {
                if (service_buffer<Logger>::alive()) {
                    service_buffer<Logger>::get().trace(TPR_LOG_STYLE_TIMESTAMP1) << "Constructed service "
                        << type_name<T>::value << " (" << type_name<T>::value_short << ")" << "\n";
                } else {
                    std::printf("%s\n", ("Constructed service " + type_name<T>::value + " (" + type_name<T>::value_short + ")").c_str());
                    std::fflush(stdout);
                }
            } else {
                std::printf("%s\n", ("Constructed service " + type_name<T>::value + " (" + type_name<T>::value_short + ")").c_str());
                std::fflush(stdout);
            }
            return ref;
        }

        template <typename T>
        void destruct() noexcept(true) {
            static_assert(contains_v<T>, "service_singleton_holder: unspecified service");
            if constexpr (contains_v<Logger>) {
                if (service_buffer<Logger>::alive()) {
                    try {
                        service_buffer<Logger>::get().trace(TPR_LOG_STYLE_TIMESTAMP1) << "Destructing service " << type_name<T>::value << "\n";
                    } catch (...) {}
                } else {
                    std::printf("%s\n", ("Destructing service " + type_name<T>::value).c_str());
                    std::fflush(stdout);
                }
            } else {
                std::printf("%s\n", ("Destructing service " + type_name<T>::value).c_str());
                std::fflush(stdout);
            }
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
        TemporEngine(size_t verbose_level, std::string config_path);
        int init();
        int run();
        void shutdown();
        ~TemporEngine() noexcept;

        void sigint() noexcept;
        void sigterm() noexcept;

        // ========== API ==========
        // log
        void log_log(TprLogLevel logLevel, const char* message) noexcept;
        void log_info(const char* message) noexcept;
        void log_warn(const char* message) noexcept;
        void log_error(const char* message) noexcept;
        void log_debug(const char* message) noexcept;
        void log_trace(const char* message) noexcept;
        void log_logStyled(TprLogLevel logLevel, TprLogStyle logStyle, const char* message) noexcept;
        void log_infoStyled(TprLogStyle logStyle, const char* message) noexcept;
        void log_warnStyled(TprLogStyle logStyle, const char* message) noexcept;
        void log_errorStyled(TprLogStyle logStyle, const char* message) noexcept;
        void log_debugStyled(TprLogStyle logStyle, const char* message) noexcept;
        void log_traceStyled(TprLogStyle logStyle, const char* message) noexcept;
        // scene
        TprResult scene_createComponent(uint32_t componentSize, TprComponent* pComponent) noexcept;
        void scene_destroyComponent(TprComponent component) noexcept;
        TprResult scene_spawnEntity(const TprComponent* pComponents, uint32_t componentCount, TprEntity* pEntity) noexcept;
        void scene_killEntity(TprEntity entity) noexcept;
        TprResult scene_modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept;
        TprResult scene_copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* componentData) noexcept;
        TprResult scene_writeEntityComponentData(TprEntity entity, TprComponent component, const char* componentData, uint32_t start, uint32_t n) noexcept;
        TprResult scene_getComponentChunkHandles(TprComponent component, TprResource resource) noexcept;
        uint32_t scene_getComponentChunkMaxElementCount() noexcept;
        TprResult scene_getComponentChunkElementCount(TprComponentChunk chunk, uint32_t* pCount) noexcept;
        TprResult scene_getComponentChunkVersion(TprComponentChunk chunk, uint32_t* pVersion) noexcept;
        TprResult scene_copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept;
        TprResult scene_writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept;
        // vfs
        TprResult vfs_openPathResource(const char* path, TprOpenPathResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept;
        TprResult vfs_openReferenceResource(char* begin, char* end, TprOpenReferenceResourceFlags flags, TprResource* pResource) noexcept;
        TprResult vfs_openEmptyResource(uint64_t size, TprOpenEmptyResourceFlags flags, uint64_t alignment, TprResource* pResource) noexcept;
        TprResult vfs_openCapabilityResource(TprResource protectResource, TprOpenEmptyResourceFlags flags, TprProtectResourceFlags protectFlags, TprResource* pResource) noexcept;
        TprResult vfs_resizeResource(TprResource resource, uint64_t newSize) noexcept;
        TprResult vfs_sizeofResource(TprResource resource, uint64_t* pSize) noexcept;
        TprResult vfs_getResourceRawDataPointer(TprResource resource, char** pData) noexcept;
        TprResult vfs_getResourceConstPointer(TprResource resource, const char** pData) noexcept;
        void vfs_closeResource(TprResource resource) noexcept;
        // win
        TprResult win_openWindow(const TprWindowCreateInfo* pCreateInfo, TprWindow* pWindow) noexcept;
        void win_closeWindow(TprWindow windo) noexcept;
        // input
        TprResult input_createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo, TprAction* pAction) noexcept;
        void input_destroyAction(TprAction action) noexcept;
        TprResult input_getActionState(TprAction action, TprActionState* pState) noexcept;
        TprResult input_getInputElementVector(TprWindow window, TprInputElement element, TprInputElementVector* pVector) noexcept;
        // geo
        TprResult geo_createMesh(const TprMeshCreateInfo* pCreateInfo, TprMesh* pMesh) noexcept;
        TprResult geo_loadMesh(TprMesh mesh, const TprMeshLoadInfo* pLoadInfo) noexcept;
        void geo_unloadMesh(TprMesh mesh) noexcept;
        void geo_destroyMesh(TprMesh mesh) noexcept;
        // conf
        TprResult conf_createSetting(const char* name, TprSetting* pSetting) noexcept;
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
        // render
        TprResult render_createDepthDomain(const TprDepthDomainCreateInfo* pInfo, TprDepthDomain* pDomain) noexcept;
        void render_destroyDepthDomain(TprDepthDomain domain) noexcept;
        TprResult render_createRenderTarget(const TprRenderTargetCreateInfo* pInfo, TprRenderTarget* pTarget) noexcept;
        void render_destroyRenderTarget(TprRenderTarget target) noexcept;
        TprComponent render_getComponentRenderable() noexcept;
        TprResult render_createObjectImage(const TprObjectImageCreateInfo* pInfo, TprObjectImage* pImage) noexcept;
        void render_destroyObjectImage(TprObjectImage image) noexcept;
        // thread
        TprResult thread_createJob(const TprJobCreateInfo* pInfo, TprJob* pJob) noexcept;
        TprResult thread_createDetachedJob(const TprJobCreateInfo* pInfo) noexcept;
        TprResult thread_jobFinished(TprJob job, TprBool8* pData) noexcept;
        void thread_joinJob(TprJob job) noexcept;

    private:
        sleep_clock mClock;

        TprEngineAPI::Log mLogAPI;
        TprEngineAPI::VFS mVFSAPI;
        TprEngineAPI::Scene mSceneAPI;
        TprEngineAPI::Geo mGeoAPI;
        TprEngineAPI::Win mWinAPI;
        TprEngineAPI::Input mInputAPI;
        TprEngineAPI::Conf mConfAPI;
        TprEngineAPI::Render mRenderAPI;
        TprEngineAPI::Thread mThreadAPI;
        TprEngineAPI mAPI;

        void registerAPI();
        expected<uint32_t, TprResult> activePluginID();
        expected<PluginInfo, TprResult> activePluginInfo();

        service_singleton_holder<
            Logger, WindowManager, PHardwareLayer, AssetStore, SceneGraph, PluginLoader, ResourceRegistry,
            Settings, Threading
        > mServHolder;

        std::string mConfigPath;

        ResourceRegistry* mpResReg = nullptr;
        Logger* mpLogger = nullptr;
        WindowManager* mpWinMan = nullptr;
        HardwareLayer* mpHWLI = nullptr;
        AssetStore* mpAssetStore = nullptr;
        PluginLoader* mpPlugLd = nullptr;
        SceneGraph* mpSceneGraph = nullptr;
        Settings* mpSettings = nullptr;
        Threading* mpThread = nullptr;

        TprComponent mComponentRenderable;

        volatile sig_atomic_t mSigInt = 0;
        volatile sig_atomic_t mSigTerm = 0;
        std::atomic<int32_t> mAliveTokens = 0;
        bool mMustShutdown = false;
        std::atomic<bool> mPanic = false;

        std::unordered_map<uint32_t, uint32_t> mJobPluginMap;

};



#endif  // TEMPOR_ENGINE_TEMPOR_HPP_
