
#include "plugin.h"
#include "plugin_core.h"

#include <cstring>
#include <string>  // IWYU pragma: keep


using namespace std::string_literals;



#define ROF(__expr) do {            \
    auto __r = (__expr);            \
    if (__r < 0) return -1;         \
} while (0)




class Plugin {
    public:
        const TprEngineAPI* api;
        TprWindow window;
        TprEntityHandle entity;
        TprAsset model;
};



extern "C" {



int32_t tprHookInit(void** ctx, const TprEngineAPI* api) noexcept {

    Plugin* plugin = new Plugin;
    if (!plugin) return -1;

    *ctx = reinterpret_cast<void*>(plugin);
    plugin->api = api;

    plugin->api->logInfoStyled(TPR_LOG_STYLE_STANDART, "TEST PLUGIN INITIALIZATION\n");

    // ROF(plugin->api->scene.createEntity(0, nullptr, &plugin->entA));
    // ROF(plugin->api->scene.createEntity(0, nullptr, &plugin->entB));

    TprWindowCreateInfo windowCreateInfo{};
    windowCreateInfo.name = "Tempor Testing Initiative";
    windowCreateInfo.prefferedHeight = 800;
    windowCreateInfo.prefferedWidth = 800;
    ROF(plugin->api->wm.openWindow(&plugin->window, &windowCreateInfo));

    /*

    // API.vfs::
    // TprResult createResouceByPath(const char* path, TprCreateResourceFlags flags)
    //  - used to create resource with data from a file, the resource is always deleted next frame
    // TprResult createResourceByBuffer(const char* begin, const char* end, TprCreateResourceFlags flags)
    //  - used to create resource with data from a buffer in memory, the resource is always deleted next frame
    // TprResult createResourceEmpty(uint64_t size, TprCreateResourceFlags flags)
    //  - used to create resource with set size filled with zeros, the resource is always deleted next frame
    // TprResult createResouceByPathLifetimed(const char* path, TprCreateResourceFlags flags, TprLifetime lifetime)
    //  - used to create resource with data from a file, allows lifetime control
    // TprResult createResourceByBufferLifetimedconst char* begin, const char* end, TprCreateResourceFlags flags, TprLifetime lifetime)
    //  - used to create resource with data from a buffer in memory, allows lifetime control
    // TprResult createResourceEmptyLifetimed(uint64_t size, TprCreateResourceFlags flags, TprLifetime lifetime)
    //  - used to create resource with set size filled with zeros, allows lifetime control
    //
    // Resource - an abstract wrapper of some data,
    // used to abstract filesystem and memory


    // initializing
    sceneFile = openFile(path/to/file);

    vector materials;
    for (material : sceneFile) {
        resource = API.vfs.createResourceByBuffer(material, sizeof(Material), TPR_CREATE_RESOURCE_DONT_COPY);
        // all the data is copied inside the resource by default,
        // but in this case it is unnesesary because materialHandle already will encapsulate all the data,
        // so the appropriate flag must be passed to get rid of extra copy

        materialHandle = API.geo.loadMaterialResource(resource);
        materials.push_back(materialHandle);
    }

    vector models;
    for (model : sceneFile) {
        resource = API.vfs.createResourceByPath(model.path, TPR_CREATE_RESOURCE_DONT_COPY);
        // data still doesn't need to be copied, so the appropriate flag is passed

        modelHandle = API.geo.loadModelResource(resource);
        // doesn't auto-load anything from handle,
        // everything else (e. g. materials) must be loaded manually separately.
        // Everything else is linked to a model through a local index, so
        // user code must manually link it to a handle

        for (materialId : model.materials) {
            API.geo.connectMaterial(modelHandle, materials[materialId], materialId);
        }

        models.push_back(modelHandle);
    }

    */

    TprResource modelResource;
    ROF(plugin->api->vfs.openPathResource("plugins/test/model.glb", TPR_OPEN_PATH_RESOURCE_READ_FLAG_BIT, 1, &modelResource));
    TprAssetParseInfo parseInfo{};
    parseInfo.resource = modelResource;
    parseInfo.type = TPR_ASSET_TYPE_MODEL;
    ROF(plugin->api->geo.parseAsset(&parseInfo, &plugin->model));
    plugin->api->vfs.closeResource(modelResource);

    return 0;
}



void tprHookShutdown(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);

    // plugin->api->geo.destroyAsset(plugin->model);
    plugin->api->wm.closeWindow(plugin->window);

    delete plugin;
}



uint32_t tprHookGetNeededHooks(void* ctx, TprHook* hooks) noexcept {
    
    TprHook h[] = {
        // TPR_HOOK_GET_ENTITY_DRAW_ARRAY
    };

    if (hooks) {
        std::memcpy(hooks, h, sizeof(h));
    }

    return 0;
}



void tprHookGetEntityDrawDescriptors(void *ctx, TprOArrayEntityDrawDesc oarray) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);

    uint32_t size;
    if (plugin->api->render.sizeofEntityDrawOArray(oarray, &size) < 0) return;
    
    TprEntityDrawDesc* arr;
    if (plugin->api->render.growEntityDrawOArray(oarray, 2, &arr) < 0) return;

    arr[0 + size].flags = TPR_ENTITY_DRAW_DESC_VISIBLE_FLAG_BIT;
    arr[0 + size].entityHandle = plugin->entity;

    if (plugin->api->render.endEntityDrawOArray(oarray, 2) < 0) return;
}




} // extern "C"
