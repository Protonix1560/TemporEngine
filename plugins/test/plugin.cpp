
#include "plugin.h"
#include "plugin_core.h"

#include <string>  // IWYU pragma: keep
#include <vulkan/vulkan_core.h>


using namespace std::string_literals;



#define ROF(__expr) do {            \
    auto __r = (__expr);            \
    if (__r < 0) return __r;         \
} while (0)




class Plugin {
    public:
        const TprEngineAPI* api;
        TprWindow window;
        TprAction quitAction;
        TprAction mouseAction;
        TprEntity entity;
        TprAsset model;
};



extern "C" {



int32_t init(void** ctx, const TprEngineAPI* api) noexcept {

    Plugin* plugin = new Plugin;
    if (!plugin) return -1;

    *ctx = reinterpret_cast<void*>(plugin);
    plugin->api = api;

    plugin->api->log->info("TEST PLUGIN INITIALIZATION\n");

    // ROF(plugin->api->scene.createEntity(0, nullptr, &plugin->entA));
    // ROF(plugin->api->scene.createEntity(0, nullptr, &plugin->entB));

    TprWindowCreateInfo windowCreateInfo{};
    windowCreateInfo.name = "Tempor Testing Initiative";
    windowCreateInfo.prefferedWidth = 1300;
    windowCreateInfo.prefferedHeight = 800;
    ROF(plugin->api->wm->openWindow(&windowCreateInfo, &plugin->window));

    TprActionCreateInfo quitActionInfo{};
    quitActionInfo.element = TPR_KEY_ESCAPE;
    quitActionInfo.lowThreshold = 0.3f;
    quitActionInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &quitActionInfo, &plugin->quitAction));

    TprActionCreateInfo mouseActionInfo{};
    mouseActionInfo.element = TPR_MOUSE_WHEEL_DOWN;
    mouseActionInfo.lowThreshold = 0.3f;
    mouseActionInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &mouseActionInfo, &plugin->mouseAction));

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
    ROF(plugin->api->vfs->openPathResource("plugins/test/model.glb", 0, 1, &modelResource));
    // TprAssetParseInfo parseInfo{};
    // parseInfo.resource = modelResource;
    // parseInfo.type = TPR_ASSET_TYPE_MODEL;
    // ROF(plugin->api->geo.parseAsset(&parseInfo, &plugin->model));
    plugin->api->vfs->closeResource(modelResource);

    return 0;
}



void pluginShutdown(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);
    plugin->api->log->info("TEST PLUGIN SHUTDOWN\n");
    plugin->api->input->destroyAction(plugin->quitAction);
    plugin->api->wm->closeWindow(plugin->window);
    delete plugin;
}



int32_t updatePerFrame(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);

    TprActionState quitActionState;
    ROF(plugin->api->input->getActionState(plugin->quitAction, &quitActionState));
    if (quitActionState.state) {
        plugin->api->wm->closeWindow(plugin->window);
    }

    TprActionState mouseActionState;
    ROF(plugin->api->input->getActionState(plugin->mouseAction, &mouseActionState));
    if (mouseActionState.state) {
        plugin->api->log->info((std::to_string(mouseActionState.framesActive) + ": " + std::to_string(mouseActionState.vector.x) + ", " + std::to_string(mouseActionState.vector.y) + "\n").c_str());
    }

    return 0;
}



int32_t getPluginCallbacks(TprPluginCallbacks *pCallbacks) noexcept {

    pCallbacks->init = init;
    pCallbacks->shutdown = pluginShutdown;
    pCallbacks->updatePerFrame = updatePerFrame;

    return 0;
}




} // extern "C"
