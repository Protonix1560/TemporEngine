

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"
#include "plugin.hpp"
#include <memory>



PluginLoader::PluginLoader(Logger& rLogger) : mrLogger(rLogger) {}

PluginLoader::~PluginLoader() noexcept {}



TprResult PluginLoader::loadPlugin(const PluginLoadInfo* pLoadInfo) {

    Plugin* pPlugin = nullptr;

    switch (pLoadInfo->loadType) {
        case PluginLoadType::InThread: {
            pPlugin = mPlugins.emplace_back(std::make_unique<PluginInThread>(mrLogger)).get();
            break;
        }

        default: return TPR_UNKNOWN_ERROR;
    }

    pPlugin->init(pLoadInfo);

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

            default:
                return unexpected(TPR_INVALID_VALUE);
        }
    }

    return returns;
}

