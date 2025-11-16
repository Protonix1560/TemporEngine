
#include "plugin_launcher.hpp"
#include "core.hpp"
#include "io.hpp"
#include "logger.hpp"
#include "plugin.h"
#include "plugin.hpp"
#include <algorithm>
#include <memory>



#if defined(__linux__)
    #include <dlfcn.h>
    inline void* DLOPEN_LOCAL_NOW(const char* file) noexcept {
        return dlopen(file, RTLD_LOCAL | RTLD_NOW);
    }
    inline void* DLSYM(void* handle, const char* name) noexcept {
        return dlsym(handle, name);
    }
    inline void DLCLOSE(void* handle) noexcept {
        dlclose(handle);
    }
    inline std::string DLERROR() noexcept {
        const char* sc = dlerror();
        std::string s;
        if (sc) s.append(sc);
        return s;
    }
#endif




const char* const kHookNames[] = {
    "tprHookInit",
    "tprHookShutdown",
    "tprHookGetNeededHooks",
    "tprHookUpdatePerFrame"
};




void PluginLauncher::init(const GlobalServiceLocator* serviceLocator, const TprEngineAPI* api) {

    mpServiceLocator = serviceLocator;
    mpApi = api;

}


const Plugin* PluginLauncher::load(std::filesystem::path pluginPath) {

    auto& log = mpServiceLocator->get<Logger>();
    IOManager& io = mpServiceLocator->get<IOManager>();

    uint32_t pluginId = mCounter;

    mCounter++;

    mPlugins.emplace_back(std::make_unique<Plugin>());
    Plugin* plugin = mPlugins.back().get();
    plugin->path = pluginPath;
    plugin->id = pluginId;

    log.info(TPR_LOG_STYLE_TIMESTAMP1) << "Loading plugin " << pluginPath << " with engine name " << plugin->name() << "...\n";

    {
        ROPacket packet = io.openRO(plugin->path);
        auto tmpPath = packet.getVirtualPath();

        plugin->addr = DLOPEN_LOCAL_NOW(plugin->path.c_str());
        if (!plugin->addr) {
            plugin->addr = nullptr;
            throw Exception(ErrCode::InternalError, "Failed to load "s + plugin->name() + ": "s + DLERROR() + "\n"s);
        }
        log.trace() << "Opened " << plugin->name() << " dynamic library\n";

        auto hookInit = reinterpret_cast<pHookInit>(DLSYM(plugin->addr, kHookNames[TPR_HOOK_INIT]));
        {
            auto err = DLERROR();
            if (!err.empty()) {
                throw Exception(
                    ErrCode::PluginError, "No required symbol "s + kHookNames[TPR_HOOK_INIT] +
                    " in " + plugin->name() + ". Error:\n"s + err
                );
            }
        }
        log.trace() << "Loaded " << plugin->name() << "." << kHookNames[TPR_HOOK_INIT] << "\n";

        plugin->shutdownHook = reinterpret_cast<pHookShutdown>(DLSYM(plugin->addr, kHookNames[TPR_HOOK_SHUTDOWN]));
        {
            auto err = DLERROR();
            if (!err.empty()) {
                throw Exception(
                    ErrCode::PluginError, "No required symbol "s + kHookNames[TPR_HOOK_SHUTDOWN] +
                    " in " + plugin->name() + ". Error:\n"s + err
                );
            }
        }
        log.trace() << "Loaded " << plugin->name() << "." << kHookNames[TPR_HOOK_SHUTDOWN] << "\n";

        log.trace() << "Calling " << plugin->name() << "." << kHookNames[TPR_HOOK_INIT] << "...\n";
        int initErr = hookInit(&plugin->pluginContext, mpApi);
        if (initErr < 0) {
            throw Exception(
                ErrCode::InternalError, 
                "Plugin"s + std::to_string(plugin->id) + "."s + kHookNames[TPR_HOOK_INIT] + " returned exit code "s + std::to_string(initErr)
            );
        }
        log.trace() << plugin->name() << "." << kHookNames[TPR_HOOK_INIT] << " returned exit code " << initErr << ". Success\n";

        auto hookGetNeededHooks = reinterpret_cast<pHookGetNeededHooks>(DLSYM(plugin->addr, kHookNames[TPR_HOOK_GET_NEEDED_HOOKS]));
        {
            auto err = DLERROR();
            if (!err.empty()) {
                throw Exception(
                    ErrCode::PluginError, "No required symbol "s + kHookNames[TPR_HOOK_GET_NEEDED_HOOKS] +
                    " in " + plugin->name() + ". Error:\n"s + err
                );
            }
        }
        log.trace() << "Loaded " << plugin->name() << "." << kHookNames[TPR_HOOK_GET_NEEDED_HOOKS] << "\n";

        unsigned int hookCount = hookGetNeededHooks(plugin->pluginContext, nullptr);
        std::vector<TprHook> hooks(hookCount);
        hookGetNeededHooks(plugin->pluginContext, hooks.data());
        std::vector<void*> ptrs;
        ptrs.reserve(hookCount);
        
        for (TprHook hookType : hooks) {
            void* sym = DLSYM(plugin->addr, kHookNames[hookType]);
            auto err = DLERROR();
            if (!err.empty()) {
                throw Exception(
                    ErrCode::PluginError, "No listed by "s + kHookNames[TPR_HOOK_GET_NEEDED_HOOKS] + " symbol "s + kHookNames[hookType] +
                    " in " + plugin->name() + ". Error:\n"s + err
                );
            }
            ptrs.push_back(sym);
            log.trace() << "Loaded " << plugin->name() << "." << kHookNames[hookType] << "\n";
        }

        for (size_t i = 0; i < ptrs.size(); i++) {
            mHookPtrs[hooks[i]].push_back({plugin, ptrs[i]});
        }
    }

    log.info(TprLogStyle::TPR_LOG_STYLE_TIMESTAMP1) << "Loaded " << plugin->name() << "\n";

    return plugin;
}



void PluginLauncher::unloadPluginLib(Plugin& plugin) {

    if (plugin.shutdownHook) {
        plugin.shutdownHook(plugin.pluginContext);
    }

    if (plugin.addr) {
        DLCLOSE(plugin.addr);
        plugin.addr = nullptr;
    }

}



void PluginLauncher::unload(const Plugin* plugin) {

    auto it = std::find_if(
        mPlugins.begin(), mPlugins.end(),
        [plugin](const std::unique_ptr<Plugin>& p) {
            return p.get() == plugin;
        }
    );

    if (it == mPlugins.end()) {
        throw Exception(ErrCode::InternalError, "No given plugin is loaded"s);
    }

    try {
        unloadPluginLib(*it->get());
    } catch(...) {
        mPlugins.erase(it);

        for (auto& [hookType, hookPtrs] : mHookPtrs) {
            hookPtrs.erase(
                std::remove_if(
                    hookPtrs.begin(), hookPtrs.end(), 
                    [plugin](const HookEntry& hook) -> bool {
                        return hook.plugin == plugin;
                    }
                ),
                hookPtrs.end()
            );
        }
        
        throw;
    }

    mPlugins.erase(it);

    for (auto& [hookType, hookPtrs] : mHookPtrs) {
        hookPtrs.erase(
            std::remove_if(
                hookPtrs.begin(), hookPtrs.end(), 
                [plugin](const HookEntry& hook) -> bool {
                    return hook.plugin == plugin;
                }
            ),
            hookPtrs.end()
        );
    }

}



void PluginLauncher::triggerHook(TprHook hookType) {

    if (mHookPtrs.find(hookType) == mHookPtrs.end()) return;

    for (const auto& [plugin, hookPtr] : mHookPtrs[hookType]) {

        if (plugin->state[PluginFailed]) continue;
        
        switch (hookType) {

            // hooks with standart layout
            case TPR_HOOK_UPDATE_PER_FRAME:
            {
                pStdHook hook = reinterpret_cast<pStdHook>(hookPtr);
                int result = hook(plugin->pluginContext);
                if (result < 0) {
                    mpServiceLocator->get<Logger>().error(TPR_LOG_STYLE_ERROR1)
                        << plugin->name() << "." << kHookNames[hookType] << " returned exit code "
                        << result << ". Shutting it down...\n";
                    mFailedPlugins.push_back(plugin);
                    plugin->state[PluginFailed] = true;
                }
                break;
            }

            default: break;
        }

    }

}



void PluginLauncher::update() {

    for (Plugin* plugin : mFailedPlugins) {
        std::string name = plugin->name();
        unload(plugin);
        mpServiceLocator->get<Logger>().info(TPR_LOG_STYLE_TIMESTAMP1) << "Shut down " << name << "\n";
    }
    mFailedPlugins.clear();

}



void PluginLauncher::shutdown() noexcept {

    unloadAll();

}



void PluginLauncher::unloadAll() noexcept {

    auto& log = mpServiceLocator->get<Logger>();

    for (auto& plugin : mPlugins) {
        try {
            unloadPluginLib(*plugin);
        } catch(const Exception& e) {
            log.error(TPR_LOG_STYLE_ERROR1) << "Failed to unload " << plugin->name() << " due to an expected exception[" << e.code() << "]: " << e.what() << "\n";
        } catch(const std::exception& e) {
            log.error(TPR_LOG_STYLE_ERROR1) << "Failed to unload " << plugin->name() << " due to an unexpected exception: " << e.what() << "\n";
        } catch(...) {
            log.error(TPR_LOG_STYLE_ERROR1) << "Failed to unload " << plugin->name() << " due to an unknown exception" << "\n";
        }
    }

    mFailedPlugins.clear();
    mHookPtrs.clear();
    mPlugins.clear();
    mCounter = 0;

}



