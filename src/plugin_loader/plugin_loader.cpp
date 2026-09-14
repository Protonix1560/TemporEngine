

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin_wrapper.hpp"
#include "settings.hpp"
#include "tempor.h"
#include "scheduler.hpp"
#include "log_entry.hpp"
#include "thread_info.hpp"

#include <memory>
#include <mutex>


PluginLoader::PluginLoader(Logger logger, Settings& rSettings, Scheduler& rSched, TprEngineAPI* pAPI, std::atomic<TprResult>& rRunResult)
    : mLogger(logger), mrSettings(rSettings), mrSched(rSched), mpAPI(pAPI), mrRunResult(rRunResult) {}

PluginLoader::~PluginLoader() noexcept {}

TprResult PluginLoader::init() {
    auto shutdownJobExp = mrSched.createJob({.duration = TPR_JOB_DURATION_SHORT, .triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE});
    if (!shutdownJobExp.has_value()) return shutdownJobExp.error();
    mShutdownJob = shutdownJobExp.value();
    return TPR_SUCCESS;
}

TprResult PluginLoader::loadPlugins() {
    // a stub for now

    auto plugin = std::make_unique<PluginWrapper>(mLogger, mpAPI, "plugins/test/libtest_plugin.so");
    TprResult result = plugin->init(mPluginCounter);
    if (result == TPR_SUCCESS) {
        std::lock_guard<std::mutex> lock(mMutex);
        mPlugins.insert_or_assign(mPluginCounter, std::move(plugin));
        mPluginCounter++;
    }

    return TPR_SUCCESS;
}

void PluginLoader::shutdown() {
    mrSched.scheduleJob(mShutdownJob, 0);
}

TprJob PluginLoader::getShutdownJob() noexcept {
    auto exp = mrSched.createJobCapability(mShutdownJob, 0);
    if (!exp.has_value()) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": createJobCapability failed [" << exp.error() << "]";
        mrRunResult.store(TPR_PANIC);
        return {};
    }
    return mShutdownJob;
}

void PluginLoader::shutdownReady() noexcept {
    if (!threadInfo.currentPlugin) return;
    threadInfo.currentPlugin->unloaded.store_true();
}

void PluginLoader::update() {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mPlugins.begin(); it != mPlugins.end();) {
        auto& [id, plugin] = *it;
        if (plugin->context()->unloaded.load() && plugin->context()->executions.load() == 0) {
            mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Unloaded plugin " << plugin->name();
            it = mPlugins.erase(it);
        } else {
            it++;
        }
    }
}

uint32_t PluginLoader::loadedPluginCount() const {
    std::lock_guard<std::mutex> lock(mMutex);
    uint32_t count = 0;
    for (auto& [id, plugin] : mPlugins) {
        if (!plugin->context()->unloaded.load()) count++;
    }
    return count;
}
