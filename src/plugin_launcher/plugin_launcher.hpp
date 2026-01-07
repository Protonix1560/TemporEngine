
#ifndef PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_
#define PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_


#include "core.hpp"
#include "logger.hpp"
#include "plugin.hpp"
#include "plugin_core.h"
#include "data_bridge.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>
#include <bitset>


using namespace std::string_literals;



enum PluginState {
    PluginFailed = 0
};




inline constexpr const char* kHookNames[] = {
    "tprHookInit",
    "tprHookShutdown",
    "tprHookGetNeededHooks",
    "tprHookUpdatePerFrame",
    "tprHookRenderBegin",
    "tprHookGetEntityDrawDescriptors"
};



struct Plugin {

    std::filesystem::path path;
    uint32_t id;
    pHookShutdown shutdownHook;
    void* pluginContext;
    std::bitset<32> state = 0;

    #if defined(__linux__)
        void* addr = nullptr;
    #endif

    inline std::string name() const {
        return "plugin"s + std::to_string(id);
    }

};



template <TprHook H>
struct HookFunctor {
    template <typename T, typename... Args>
    void operator()(T* This, Args&... args) const {

        for (const auto& [plugin, hookPtr] : This->mHookPtrs[H]) {

            if (plugin->state[PluginFailed]) continue;

            pStdHook hook = reinterpret_cast<pStdHook>(hookPtr);
            int32_t result = hook(plugin->pluginContext);

            if (result < 0) {
                gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                    << plugin->name() << "." << kHookNames[H] << " returned exit code "
                    << result << ". Shutting it down...\n";

                This->mFailedPlugins.push_back(plugin);
                plugin->state[PluginFailed] = true;
            }
        }
    }
};


template <>
struct HookFunctor<TPR_HOOK_GET_ENTITY_DRAW_ARRAY> {
    template <typename T>
    void operator()(T* This, DataBridge::OArrayHandle<TprOArrayEntityDrawDesc>& oarray) const {

        for (const auto& [plugin, hookPtr] : This->mHookPtrs[TPR_HOOK_GET_ENTITY_DRAW_ARRAY]) {
            if (plugin->state[PluginFailed]) continue;
            pHookGetEntityDrawArray hook = reinterpret_cast<pHookGetEntityDrawArray>(hookPtr);
            hook(plugin->pluginContext, oarray.opaqueHandle());
            if (!oarray.finalized()) oarray.clear();
        }
    }
};



class PluginLauncher {

    public:

        void init(const TprEngineAPI* api);
        const Plugin* load(std::filesystem::path plugin);
        void unload(const Plugin* plugin);
        void unloadAll() noexcept;
        void shutdown() noexcept;
        void update();
        
        template <TprHook H, typename... Args>
        void triggerHook(Args&&... args) {
            if (mHookPtrs.find(H) == mHookPtrs.end()) return;
            HookFunctor<H>{}(this, std::forward<Args>(args)...);
        }

    private:
        void unloadPluginLib(Plugin& plugin);

        struct HookEntry {
            Plugin* plugin; void* ptr;
        };

        const TprEngineAPI* mpApi;

        std::vector<std::unique_ptr<Plugin>> mPlugins;
        std::unordered_map<TprHook, std::vector<HookEntry>> mHookPtrs;
        uint32_t mCounter = 0;
        std::vector<Plugin*> mFailedPlugins;

        template <TprHook>
        friend struct HookFunctor;

};



#endif  // PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_

