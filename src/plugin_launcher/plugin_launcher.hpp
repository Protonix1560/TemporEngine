
#ifndef PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_
#define PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_



#include "core.hpp"
#include "plugin.h"
#include "plugin.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>
#include <bitset>



enum PluginState {
    PluginFailed = 0
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



class PluginLauncher {

    public:

        void init(const GlobalServiceLocator* serviceLocator, const TprEngineAPI* api);
        const Plugin* load(std::filesystem::path plugin);
        void unload(const Plugin* plugin);
        void unloadAll() noexcept;
        void shutdown() noexcept;
        void update();
        void triggerHook(TprHook hook);

    private:
        void unloadPluginLib(Plugin& plugin);

        struct HookEntry {
            Plugin* plugin; void* ptr;
        };

        const GlobalServiceLocator* mpServiceLocator;
        const TprEngineAPI* mpApi;

        std::vector<std::unique_ptr<Plugin>> mPlugins;
        std::unordered_map<TprHook, std::vector<HookEntry>> mHookPtrs;
        uint32_t mCounter = 0;
        std::vector<Plugin*> mFailedPlugins;

};



#endif  // PLUGIN_LAUNCHER_PLUGIN_LAUNCHER_HPP_

