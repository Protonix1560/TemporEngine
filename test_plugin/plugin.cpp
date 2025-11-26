
#include "plugin.h"

#include <cstring>
#include <iterator>
#include <vector>
#include <iostream>




#define ROF(__expr) do {            \
    auto __r = (__expr);            \
    if (__r < 0) return -1;         \
} while (0)




class Plugin {
    public:
        const TprEngineAPI* api;
};



extern "C" {



int32_t tprHookInit(void** ctx, const TprEngineAPI* api) noexcept {

    Plugin* plugin = new Plugin;
    if (!plugin) return -1;

    *ctx = reinterpret_cast<void*>(plugin);
    plugin->api = api;

    uint32_t sittableComp;
    ROF(plugin->api->declareComponent(4, "test_plugin.components.sittableComponent", &sittableComp));

    uint32_t c;
    ROF(plugin->api->acquireComponent("test_plugin.components.sittableComponent", &c));
    std::cout << c << "\n";

    uint32_t chairId;
    ROF(plugin->api->createEntity(1, &sittableComp, &chairId));

    std::vector<char> sittableData = {
        'C', 'H', 'A', 'R'
    };

    ROF(plugin->api->writeEntityComponentData(chairId, sittableComp, sittableData.data(), 0, 0));

    sittableData.assign(4, 0);

    ROF(plugin->api->writeEntityComponent8bit(chairId, sittableComp, 'G', 0));

    ROF(plugin->api->writeEntityComponent8bit(chairId, sittableComp, 'Q', 1));

    ROF(plugin->api->copyEntityComponentData(chairId, sittableComp, 0, 0, sittableData.data()));

    std::cout << sittableData[0] << sittableData[1] << sittableData[2] << sittableData[3] << "\n";

    uint32_t a;
    ROF(plugin->api->readEntityComponent32bit(chairId, sittableComp, 0, &a));
    std::cout << a << "\n";

    ROF(plugin->api->destroyEntity(chairId));

    return 0;
}



void tprHookShutdown(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);
    delete plugin;
}



uint32_t tprHookGetNeededHooks(void* ctx, TprHook* hooks) noexcept {
    
    TprHook h[] = {
        TPR_HOOK_UPDATE_PER_FRAME
    };

    if (hooks) {
        std::memcpy(hooks, h, sizeof(h));
    }

    return std::size(h);
}
    
    
    
int32_t tprHookUpdatePerFrame(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);
    return 0;
}




} // extern "C"
