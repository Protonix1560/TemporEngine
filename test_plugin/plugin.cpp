
#include "plugin.h"

#include <cstring>
#include <iterator>



class Plugin {
    public:
        const TprEngineAPI* api;
};



extern "C" {



int tprHookInit(void **ctx, const TprEngineAPI* api) noexcept {

    Plugin* plugin = new Plugin;
    if (!plugin) return -1;

    *ctx = reinterpret_cast<void*>(plugin);
    plugin->api = api;
    plugin->api->logInfo("Hello, World!\n");

    return 0;
}



void tprHookShutdown(void *ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);
    delete plugin;
}



unsigned int tprHookGetNeededHooks(void* ctx, TprHook *hooks) noexcept {
    
    TprHook h[] = {
        TPR_HOOK_UPDATE_PER_FRAME
    };

    if (hooks) {
        std::memcpy(hooks, h, sizeof(h));
    }

    return std::size(h);
}
    
    
    
int tprHookUpdatePerFrame(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);
    return 0;
}




} // extern "C"
