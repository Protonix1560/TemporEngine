
#ifndef PLUGIN_LOADER_PLUGIN_LOADER_HPP_
#define PLUGIN_LOADER_PLUGIN_LOADER_HPP_


#include "core.hpp"
#include "plugin.h"
// #include "plugin_core.h"

#include <filesystem>



// from "logger.hpp"
class Logger;



class PluginLoader {

    public:
        PluginLoader(Logger& rLogger, const TprEngineAPI* pAPI);
        ~PluginLoader() noexcept;

        TprResult loadPlugin(std::filesystem::path path);
        TprResult triggerHook(TprHook hook);

    private:
        Logger& mrLogger;
        const TprEngineAPI* mpAPI;

};

REGISTER_TYPE_NAME_S(PluginLoader, "PgLd");



#endif  // PLUGIN_LOADER_PLUGIN_LOADER_HPP_

