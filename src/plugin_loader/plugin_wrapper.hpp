
#ifndef PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_
#define PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_

#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"
#include "plugin.h"

#ifdef LINUX
    #include "linux_helper.hpp"
#endif


class PluginWrapper {
    public:
        PluginWrapper(Logger logger, TprEngineAPI* pAPI, std::filesystem::path path);
        TprResult init();
        const std::string_view name() const noexcept;
    private:
        Logger mLogger;
        Lib mPluginLib;
        std::string mName;
        std::filesystem::path mPath;
        TprEngineAPI* mpAPI;
};


#endif  // PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_


