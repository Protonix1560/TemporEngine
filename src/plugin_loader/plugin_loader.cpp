

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin.hpp"
#include "settings.hpp"

#include <memory>


PluginLoader::PluginLoader(Logger& rLogger, Settings& rSettings, std::atomic<int32_t>& rAliveTokens) : mrLogger(rLogger), mrSettings(rSettings), mrAliveTokens(rAliveTokens) {}

PluginLoader::~PluginLoader() noexcept {}


TprResult PluginLoader::loadPlugin(const PluginLoadInfo* pLoadInfo) {

    auto pluginIt = mPlugins.end();
    Plugin* plugin;

    switch (pLoadInfo->loadType) {
        case PluginLoadType::InThread: {
            pluginIt = mPlugins.try_emplace(
                mPluginCounter, std::make_unique<PluginInThread>(mrLogger, mrAliveTokens)
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
        plugin->shutdown();
        mPlugins.erase(pluginIt);
        return initRes;
    }
    mCurrentPlugin.reset();

    return TPR_SUCCESS;
}


expected<std::vector<TprResult>, TprResult> PluginLoader::triggerCallback(PluginCallback callback) {

    std::vector<TprResult> returns;
    returns.reserve(mPlugins.size());

    for (uint32_t i = 0; i < mPlugins.size(); i++) {
        auto& plugin = mPlugins[i];
        mCurrentPlugin = i;

        switch (callback) {

            case PluginCallback::PreShutdown:
                plugin->preShutdown();
                returns.push_back(TPR_SUCCESS);
                break;

            case PluginCallback::Shutdown:
                plugin->shutdown();
                returns.push_back(TPR_SUCCESS);
                break;

            case PluginCallback::UpdatePerFrame: {
                int32_t ret = plugin->updatePerFrame();
                returns.push_back(ret > 0 ? TPR_SUCCESS : TPR_USER_CODE_ERROR);
                break;
            }

            default:
                return unexpected(TPR_ERROR_INVALID_VALUE);
        }
    }

    mCurrentPlugin.reset();

    return returns;
}


std::optional<uint32_t> PluginLoader::getActivePluginID() {
    return mCurrentPlugin;
}


expected<PluginInfo, TprResult> PluginLoader::getPluginInfo(uint32_t id) {
    auto it = mPlugins.find(id);
    if (it == mPlugins.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    Plugin& plugin = *it->second.get();
    PluginInfo info{};
    info.name = plugin.name();
    return info;
}

