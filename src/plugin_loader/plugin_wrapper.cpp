
#include "plugin_wrapper.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "log_entry.hpp"

#include <cstdint>


PluginWrapper::PluginWrapper(Logger logger, TprEngineAPI* pAPI, std::filesystem::path path)
    : mLogger(logger), mPluginLib(), mPath(path), mpAPI(pAPI), mName(path.filename()) {}


TprResult PluginWrapper::init() {
    mLogger.debug() << "Loading plugin " << mName << " from " << mPath;
    mPluginLib.open(mPath).value();
    auto initSymbol = mPluginLib.sym<decltype(pluginInit)*>("pluginInit").value();
    int32_t initResult = initSymbol(mpAPI);
    if (initResult < 0) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "pluginInit of plugin " << mName << " failed [" << initResult << "]\n";
        return TPR_ERROR_NOT_LOADED;
    }
    mLogger.trace() << "pluginInit of " << mName << " returned succeeded [" << initResult << "]\n";
    mLogger.debug() << "Loaded plugin " << mName << "\n";
    return TPR_SUCCESS;
}

const std::string_view PluginWrapper::name() const noexcept {
    return mName;
}
