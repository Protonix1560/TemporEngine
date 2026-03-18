
#ifndef PLUGIN_LOADER_PLUGIN_COMMON_STRUCTS_HPP_
#define PLUGIN_LOADER_PLUGIN_COMMON_STRUCTS_HPP_


#include "plugin.h"

#include <string>
#include <filesystem>


enum class PluginCallback {
    PreShutdown, Shutdown, UpdatePerFrame
};


enum class PluginLoadType {
    Process, OutThread, InThread
};


struct PluginLoadInfo {
    std::string name;
    std::filesystem::path path;
    const TprEngineAPI* pAPI;
    PluginLoadType loadType;
};



#endif  // PLUGIN_LOADER_PLUGIN_COMMON_STRUCTS_HPP_
