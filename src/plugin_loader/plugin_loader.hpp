
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin.hpp"

#include <atomic>
#include <memory>
#include <unordered_map>


// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;


struct PluginInfo {
    std::string name;
};


class PluginLoader {

    public:
        PluginLoader(Logger& rLogger, Settings& rSettings, std::atomic<int32_t>& rAliveTokens);
        ~PluginLoader() noexcept;

        TprResult loadPlugin(const PluginLoadInfo* pLoadInfo);
        expected<std::vector<TprResult>, TprResult> triggerCallback(PluginCallback callback);

        std::optional<uint32_t> getActivePluginID();
        expected<PluginInfo, TprResult> getPluginInfo(uint32_t id);

    private:
        Logger& mrLogger;
        Settings& mrSettings;
        std::atomic<int32_t>& mrAliveTokens;

        std::unordered_map<uint32_t, std::unique_ptr<Plugin>> mPlugins;
        uint32_t mPluginCounter = 0;

        std::optional<uint32_t> mCurrentPlugin;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

