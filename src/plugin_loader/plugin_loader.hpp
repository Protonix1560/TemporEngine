
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "plugin_loader_common.hpp"
#include "plugin_core.h"
#include "plugin_wrapper.hpp"
#include "logger.hpp"

#include <atomic>
#include <memory>
#include <unordered_map>


// from "settings.hpp"
class Settings;


struct PluginInfo {
    std::string name;
};


class PluginLoader {

    public:
        PluginLoader(Logger logger, Settings& rSettings, std::atomic<int32_t>& rAliveTokens);
        ~PluginLoader() noexcept;

        TprResult loadPlugin(const PluginLoadInfo* pLoadInfo);

        std::optional<uint32_t> getActivePluginID();
        expected<PluginInfo, TprResult> getPluginInfo(uint32_t id);

    private:
        Logger mLogger;
        Settings& mrSettings;
        std::atomic<int32_t>& mrAliveTokens;

        std::unordered_map<uint32_t, std::unique_ptr<PluginWrapper>> mPlugins;
        uint32_t mPluginCounter = 0;

        std::optional<uint32_t> mCurrentPlugin;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

