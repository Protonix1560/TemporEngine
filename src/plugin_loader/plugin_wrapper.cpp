
#include "plugin_wrapper.hpp"
#include "core.hpp"
#include "logger.hpp"

#include <cstdint>


PluginWrapperInThread::PluginWrapperInThread(Logger logger, std::atomic<int32_t>& rAliveTokens) : mLogger(logger), mrAliveTokens(rAliveTokens), mPluginLib() {}


TprResult PluginWrapperInThread::init(const PluginLoadInfo* pLoadInfo) {

    mrAliveTokens++;

    mLogger.debug() << "Loading plugin " << pLoadInfo->name << " (\"" << pLoadInfo->path.string() << "\") in-thread" << "\n";

    mName = pLoadInfo->name;

    mPluginLib.open(pLoadInfo->path).value();

    auto initSymbol = mPluginLib.sym<decltype(pluginInit)*>("pluginInit").value();

    int32_t initResult = initSymbol(pLoadInfo->pAPI);
    if (initResult < 0) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "pluginInit of " << mName << " returned negative exit code [" << initResult << "]\n";
        mrAliveTokens--;
        return TPR_USER_CODE_ERROR;
    }
    mLogger.trace() << "pluginInit of " << mName << " returned non-negative exit code [" << initResult << "]\n";

    mLogger.debug() << "Loaded plugin " << mName << "\n";

    mrAliveTokens--;

    return TPR_SUCCESS;
}


const std::string_view PluginWrapperInThread::name() noexcept {
    return mName;
}

