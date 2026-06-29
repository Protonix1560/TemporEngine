
#include "plugin.hpp"
#include "core.hpp"
#include "plugin_common_structs.hpp"
#include "plugin.h"
#include "logger.hpp"
#include "plugin_core.h"
#include <cstdint>


PluginInThread::PluginInThread(Logger logger, std::atomic<int32_t>& rAliveTokens) : mLogger(logger), mrAliveTokens(rAliveTokens), mPluginLib() {}


TprResult PluginInThread::init(const PluginLoadInfo* pLoadInfo) {

    mrAliveTokens++;

    mLogger.debug() << "Loading plugin " << pLoadInfo->name << " (\"" << pLoadInfo->path.string() << "\") in-thread" << "\n";

    mName = pLoadInfo->name;

    mPluginLib.open(pLoadInfo->path).value();

    auto callbacksHook = mPluginLib.sym<decltype(getPluginCallbacks)*>("getPluginCallbacks").value();

    int32_t getCallbacksRet = callbacksHook(&mCallbacks);
    if (getCallbacksRet < 0) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "getPluginCallbacks of " << mName << " returned negative exit code [" << getCallbacksRet << "]\n";
        mrAliveTokens--;
        return TPR_USER_CODE_ERROR;
    }
    mLogger.trace() << "getPluginCallbacks of " << mName << " returned non-negative exit code [" << getCallbacksRet << "]\n";

    if (mCallbacks.init) {
        int32_t initRet = mCallbacks.init(&mCtx, pLoadInfo->pAPI);
        if (initRet < 0) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "init callback of " << mName << " returned negative exit code [" << initRet << "]\n";
            mrAliveTokens--;
            return TPR_USER_CODE_ERROR;
        }
        mLogger.trace() << "init callback of " << mName << " returned non-negative exit code [" << initRet << "]\n";
    }

    mLogger.debug() << "Loaded plugin " << mName << "\n";

    mrAliveTokens--;

    return TPR_SUCCESS;
}


void PluginInThread::preShutdown() noexcept {
    if (mCallbacks.preShutdown) mCallbacks.preShutdown(mCtx);
}


void PluginInThread::shutdown() noexcept {
    if (mCallbacks.shutdown) mCallbacks.shutdown(mCtx);
    mPluginLib.close();
}


int32_t PluginInThread::updatePerFrame() noexcept {
    if (mCallbacks.updatePerFrame) return mCallbacks.updatePerFrame(mCtx);
    return 0;
}


const std::string_view PluginInThread::name() noexcept {
    return mName;
}

