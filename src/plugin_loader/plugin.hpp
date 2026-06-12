
#ifndef PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_
#define PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_

#include "core.hpp"
#include "plugin.h"
#include "plugin_common_structs.hpp"
#include "plugin_core.h"

#ifdef LINUX
    #include "linux_helper.hpp"
#endif

#include <atomic>


class Plugin {
    public:
        virtual ~Plugin() noexcept = default;
        virtual TprResult init(const PluginLoadInfo* pLoadInfo) = 0;
        virtual void preShutdown() noexcept = 0;
        virtual void shutdown() noexcept = 0;
        virtual int32_t updatePerFrame() noexcept = 0;
        virtual const std::string_view name() noexcept = 0;
};


// from "logger.hpp"
class Logger;


class PluginInThread : public Plugin {
    public:
        PluginInThread(Logger& rLogger, std::atomic<int32_t>& rAliveTokens);
        TprResult init(const PluginLoadInfo* pLoadInfo) override;
        void preShutdown() noexcept override;
        void shutdown() noexcept override;
        int32_t updatePerFrame() noexcept override;
        const std::string_view name() noexcept override;
    private:
        Logger& mrLogger;
        std::atomic<int32_t>& mrAliveTokens;
        Lib mPluginLib;
        std::string mName;
        TprPluginCallbacks mCallbacks{};
        void* mCtx;
};


#endif  // PLUGIN_LOADER_PLUGIN_BOOTSTRAPPER_HPP_


