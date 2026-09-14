
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "tempor.h"
#include "plugin_wrapper.hpp"
#include "logger.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>


// from "settings.hpp"
class Settings;

// from "scheduler.hpp"
class Scheduler;


class PluginLoader {

    public:
        PluginLoader(Logger logger, Settings& rSettings, Scheduler& rSched, TprEngineAPI* pAPI, std::atomic<TprResult>& rRunResult);
        ~PluginLoader() noexcept;

        TprResult init();
        TprResult loadPlugins();
        void update();
        void shutdown();
        uint32_t loadedPluginCount() const;

        TprJob getShutdownJob() noexcept;
        void shutdownReady() noexcept;

    private:
        Logger mLogger;
        Settings& mrSettings;
        Scheduler& mrSched;
        std::atomic<TprResult>& mrRunResult;
        TprEngineAPI* mpAPI;

        mutable std::mutex mMutex;

        TprJob mShutdownJob;

        std::unordered_map<uint32_t, std::unique_ptr<PluginWrapper>> mPlugins;
        uint32_t mPluginCounter = 0;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

