

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin_wrapper.hpp"
#include "settings.hpp"
#include "plugin_core.h"
#include "scheduler.hpp"
#include "log_entry.hpp"

#include <memory>


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
    mCurrentPlugin = mPluginCounter;
    
    TprResult result = plugin->init();
    if (result == TPR_SUCCESS) {
        mPlugins.insert_or_assign(mPluginCounter, std::move(plugin));
        mPluginCounter++;
    }
    mCurrentPlugin.reset();
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

std::optional<uint32_t> PluginLoader::getActivePluginID() {
    return mCurrentPlugin;
}

expected<PluginInfo, TprResult> PluginLoader::getPluginInfo(uint32_t id) {
    auto it = mPlugins.find(id);
    if (it == mPlugins.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    PluginWrapper& plugin = *it->second.get();
    PluginInfo info{};
    info.name = plugin.name();
    return info;
}
