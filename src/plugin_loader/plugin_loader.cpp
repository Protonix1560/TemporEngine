

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin.hpp"
#include <memory>



PluginLoader::PluginLoader(Logger& rLogger, std::atomic<int32_t>& rAliveTokens) : mrLogger(rLogger), mrAliveTokens(rAliveTokens) {}

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

    for (auto& plugin : mPlugins) {
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

