

#include "plugin_loader.hpp"
#include "logger.hpp"
#include "plugin.h"
#include "plugin_core.h"
#include "plugin_wrapper.hpp"

#include <thread>



void createPluginThread(std::filesystem::path path) {
    PluginWrapper* p = new PluginWrapper();
    p->init(path);
    p->run();
    delete p;
}



PluginLoader::PluginLoader(Logger& rLogger, const TprEngineAPI* pAPI)
    : mrLogger(rLogger), mpAPI(pAPI) {
}



PluginLoader::~PluginLoader() noexcept {

}



TprResult PluginLoader::loadPlugin(std::filesystem::path path) {

    std::thread([path]() { createPluginThread(path); }).detach();

    return TPR_SUCCESS;
}


