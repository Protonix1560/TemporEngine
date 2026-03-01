
#ifndef PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_
#define PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_


#include "core.hpp"
#include "plugin.hpp"
#include "plugin_core.h"
#include "logger.hpp"

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
    void operator()(T* This, Logger& rLogger, Args&... args) const {

        for (const auto& [plugin, hookPtr] : This->mHookPtrs[H]) {

            if (plugin->state[PluginFailed]) continue;

            pStdHook hook = reinterpret_cast<pStdHook>(hookPtr);
            int32_t result = hook(plugin->pluginContext);

            if (result < 0) {
                rLogger.error(TPR_LOG_STYLE_ERROR1)
                    << plugin->name() << "." << kHookNames[H] << " returned exit code "
                    << result << ". Shutting it down...\n";

                This->mFailedPlugins.push_back(plugin);
                plugin->state[PluginFailed] = true;
            }
        }
    }
};



class PluginLauncher {

    public:

        PluginLauncher(Logger& rLogger, const TprEngineAPI* api);
        const Plugin* load(std::filesystem::path plugin);
        void unload(const Plugin* plugin);
        void unloadAll() noexcept;
        void update();
        ~PluginLauncher() noexcept;
        
        template <TprHook H, typename... Args>
        void triggerHook(Args&&... args) {
            if (mHookPtrs.find(H) == mHookPtrs.end()) return;
            HookFunctor<H>{}(this, mrLogger, std::forward<Args>(args)...);
        }

    private:
        void unloadPluginLib(Plugin& plugin);

        struct HookEntry {
            Plugin* plugin; void* ptr;
        };

        const TprEngineAPI* mpApi;
        Logger& mrLogger;

        std::vector<std::unique_ptr<Plugin>> mPlugins;
        std::unordered_map<TprHook, std::vector<HookEntry>> mHookPtrs;
        uint32_t mCounter = 0;
        std::vector<Plugin*> mFailedPlugins;

        template <TprHook>
        friend struct HookFunctor;

};

REGISTER_TYPE_NAME(PluginLauncher);



#endif  // PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_

