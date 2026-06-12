
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin.hpp"

#include <atomic>


// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;


class PluginLoader {

    public:
        PluginLoader(Logger& rLogger, Settings& rSettings, std::atomic<int32_t>& rAliveTokens);
        ~PluginLoader() noexcept;

        TprResult loadPlugin(const PluginLoadInfo* pLoadInfo);
        expected<std::vector<TprResult>, TprResult> triggerCallback(PluginCallback callback);

        expected<TprSetting, TprResult> createSetting(std::string_view name) noexcept;

    private:
        Logger& mrLogger;
        Settings& mrSettings;
        std::atomic<int32_t>& mrAliveTokens;

        std::vector<std::unique_ptr<Plugin>> mPlugins;

        uint32_t mCurrentPlugin = 0;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

