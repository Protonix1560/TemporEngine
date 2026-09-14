
#include "plugin_wrapper.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "tempor.h"
#include "log_entry.hpp"
#include "thread_info.hpp"

#include <cstdint>
#include <memory>


PluginWrapper::PluginWrapper(Logger logger, TprEngineAPI* pAPI, std::filesystem::path path)
    : mLogger(logger), mPluginLib(), mPath(path), mpAPI(pAPI) {}


TprResult PluginWrapper::init(uint32_t id) {
    mName = mPath.filename();
    mLogger.debug() << "Loading plugin " << mName << " from " << mPath;
    mCtx = std::make_shared<PluginContext>(id, mName);

    mPluginLib.open(mPath).value();
    auto initSymbol = mPluginLib.sym<decltype(pluginInit)*>("pluginInit").value();

    threadInfo.currentPlugin = mCtx;
    threadInfo.currentPlugin->executions.fetch_add(1);
    int32_t initResult = initSymbol(mpAPI);
    threadInfo.currentPlugin->executions.fetch_sub(1);
    threadInfo.currentPlugin = nullptr;

    if (initResult < 0) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "pluginInit of plugin " << mName << " failed [" << initResult << "]";
        return TPR_ERROR_NOT_LOADED;
    }
    
    mLogger.debug() << "pluginInit of " << mName << " returned succeeded [" << initResult << "]";
    mLogger.info(TPR_LOG_STYLE_TIMESTAMP1) << "Loaded plugin " << mName;
    return TPR_SUCCESS;
}

const std::string_view PluginWrapper::name() const noexcept {
    return mName;
}

std::shared_ptr<PluginContext> PluginWrapper::context() const noexcept {
    return mCtx;
}
