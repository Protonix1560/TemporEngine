
#ifndef PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_
#define PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_

#include "core.hpp"
#include "plugin_loader_common.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#ifdef LINUX
    #include "linux_helper.hpp"
#endif

#include <atomic>


class PluginWrapper {
    public:
        virtual ~PluginWrapper() noexcept = default;
        virtual TprResult init(const PluginLoadInfo* pLoadInfo) = 0;
        virtual const std::string_view name() noexcept = 0;
};



class PluginWrapperInThread : public PluginWrapper {
    public:
        PluginWrapperInThread(Logger logger, std::atomic<int32_t>& rAliveTokens);
        TprResult init(const PluginLoadInfo* pLoadInfo) override;
        const std::string_view name() noexcept override;
    private:
        Logger mLogger;
        std::atomic<int32_t>& mrAliveTokens;
        Lib mPluginLib;
        std::string mName;
        void* mCtx;
};


#endif  // PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_


