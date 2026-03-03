
#ifndef PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_
#define PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_


#include <filesystem>



class PluginWrapper {

    public:
        void init(std::filesystem::path path) noexcept;
        void run() noexcept;

    private:
        

};



#endif  // PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_


