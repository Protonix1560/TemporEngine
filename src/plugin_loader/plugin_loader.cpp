

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

    Plugin* pPlugin = nullptr;

    switch (pLoadInfo->loadType) {
        case PluginLoadType::InThread: {
            pPlugin = mPlugins.emplace_back(std::make_unique<PluginInThread>(mrLogger, mrAliveTokens)).get();
            break;
        }

        default: return TPR_UNKNOWN_ERROR;
    }

    TprResult initRes = pPlugin->init(pLoadInfo);
    if (initRes < 0) {
        pPlugin->shutdown();
        mPlugins.pop_back();
        return initRes;
    }

    return TPR_SUCCESS;
}


expected<std::vector<TprResult>, TprResult> PluginLoader::triggerCallback(PluginCallback callback) {

    std::vector<TprResult> returns;
    returns.reserve(mPlugins.size());

    for (mCurrentPlugin = 0; mCurrentPlugin < mPlugins.size(); mCurrentPlugin++) {
        auto& plugin = mPlugins[mCurrentPlugin];

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
                return unexpected(TPR_INVALID_VALUE);
        }
    }

    return returns;
}


expected<TprSetting, TprResult> PluginLoader::createSetting(std::string_view name) noexcept {

    try {
        auto& plugin = mPlugins[mCurrentPlugin];
        std::string scopedName = std::format("{}.{}", plugin->name(), name);
        return mrSettings.createSetting(scopedName);

    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}

