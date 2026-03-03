

#ifndef TEMPOR_ENGINE_TEMPOR_HPP_
#define TEMPOR_ENGINE_TEMPOR_HPP_


#include "core.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "plugin_loader.hpp"
#include "scene_graph.hpp"
#include "hardware_memory_optimizator.hpp"
#include "logger.hpp"
#include "window_manager.hpp"
#include "resource_registry.hpp"
#include "hardware_layer_interface.hpp"
#include "asset_store.hpp"

#include "sleep_clock.hpp"

#include <csignal>
#include <stdexcept>
#include <cassert>



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
            return *reinterpret_cast<T*>(m_memory);
        }
        const T& get() const {
            assert(m_alive);
            return *reinterpret_cast<T*>(m_memory);
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
                    service_buffer<Logger>::get().trace(TPR_LOG_STYLE_TIMESTAMP1) << "Constructed service " << type_name<T>::value << "\n";
                } else {
                    std::printf("%s\n", ("Constructed service " + type_name<T>::value).c_str());
                    std::fflush(stdout);
                }
            } else {
                std::printf("%s\n", ("Constructed service " + type_name<T>::value).c_str());
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
        TemporEngine(size_t verbose_level, const TprEngineAPI* api);
        int init();
        int run();
        void shutdown();
        ~TemporEngine() noexcept;

        void sigint() noexcept;
        void sigterm() noexcept;

        Logger& getLogger();
        ResourceRegistry& getResourceRegistry();

    private:
        sleep_clock mClock;
        const TprEngineAPI* mpApi;

        service_singleton_holder<
            Logger, ResourceRegistry, WindowManager, std::unique_ptr<HardwareLayer>,
            HardwareMemoryOptimizator, AssetStore, SceneGraph, PluginLoader
        > mServHolder;

        ResourceRegistry* mpResReg = nullptr;
        Logger* mpLogger = nullptr;
        WindowManager* mpWinMan = nullptr;
        HardwareLayer* mpHWLI = nullptr;
        AssetStore* mpAssetStore = nullptr;
        PluginLoader* mpPlugLd = nullptr;
        SceneGraph* mpSceneGraph = nullptr;
        HardwareMemoryOptimizator* mpHWMO = nullptr;

        volatile sig_atomic_t mSigInt = 0;
        volatile sig_atomic_t mSigTerm = 0;
        bool shouldStop = false;

};



#endif  // TEMPOR_ENGINE_TEMPOR_HPP_
