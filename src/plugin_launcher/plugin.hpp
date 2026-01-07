
#ifndef PLUGIN_LAUNCHER_PLUGIN_HPP_
#define PLUGIN_LAUNCHER_PLUGIN_HPP_


#include "plugin.h"
#include "plugin_core.h"



using pStdHook = int(*)(void* ctx);

// required hooks
using pHookInit = int(*)(void** ctx, const TprEngineAPI* api);
using pHookShutdown = void(*)(void* ctx);
using pHookGetNeededHooks = unsigned int(*)(void* ctx, TprHook* hooks);

using pHookUpdatePerFrame = pStdHook;
using pHookRenderBegin = pStdHook;
using pHookGetEntityDrawArray = void(*)(void* ctx, TprOArrayEntityDrawDesc oarray);




#endif  // PLUGIN_LAUNCHER_PLUGIN_HPP_
