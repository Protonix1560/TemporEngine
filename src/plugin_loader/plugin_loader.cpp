

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "plugin_core.h"

#include <memory>


PluginLoader::PluginLoader(Logger logger, Settings& rSettings, std::atomic<int32_t>& rAliveTokens) : mLogger(logger), mrSettings(rSettings), mrAliveTokens(rAliveTokens) {}

PluginLoader::~PluginLoader() noexcept {}


TprResult PluginLoader::loadPlugin(const PluginLoadInfo* pLoadInfo) {

    auto pluginIt = mPlugins.end();
    PluginWrapper* plugin;

    switch (pLoadInfo->loadType) {
        case PluginLoadType::InThread: {
            pluginIt = mPlugins.try_emplace(
                mPluginCounter, std::make_unique<PluginWrapperInThread>(mLogger, mrAliveTokens)
            ).first;
            plugin = pluginIt->second.get();
            mPluginCounter++;
            break;
        }

        default: return TPR_ERROR_INVALID_VALUE;
    }

    mCurrentPlugin = pluginIt->first;
    TprResult initRes = plugin->init(pLoadInfo);
    if (initRes < 0) {
        mPlugins.erase(pluginIt);
        return initRes;
    }
    mCurrentPlugin.reset();

    return TPR_SUCCESS;
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

