
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "plugin_wrapper.hpp"
#include "logger.hpp"

#include <atomic>
#include <memory>
#include <unordered_map>


// from "settings.hpp"
class Settings;

// from "scheduler.hpp"
class Scheduler;


struct PluginInfo {
    std::string name;
};


class PluginLoader {

    public:
        PluginLoader(Logger logger, Settings& rSettings, Scheduler& rSched, TprEngineAPI* pAPI, std::atomic<TprResult>& rRunResult);
        ~PluginLoader() noexcept;

        TprResult init();
        TprResult loadPlugins();
        void shutdown();

        TprJob getShutdownJob() noexcept;

        std::optional<uint32_t> getActivePluginID();
        expected<PluginInfo, TprResult> getPluginInfo(uint32_t id);

    private:
        Logger mLogger;
        Settings& mrSettings;
        Scheduler& mrSched;
        std::atomic<TprResult>& mrRunResult;
        TprEngineAPI* mpAPI;

        TprJob mShutdownJob;

        std::unordered_map<uint32_t, std::unique_ptr<PluginWrapper>> mPlugins;
        uint32_t mPluginCounter = 0;

        std::optional<uint32_t> mCurrentPlugin;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

