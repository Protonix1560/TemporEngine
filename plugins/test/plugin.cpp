
#include "plugin.h"
#include "plugin_core.h"

#include <format>
#include <string>  // IWYU pragma: keep
#include <vulkan/vulkan_core.h>


using namespace std::string_literals;



#define ROF(__expr) do {            \
    auto __r = (__expr);            \
    if (__r < 0) return __r;        \
} while (0)




class Plugin {
    public:
        const TprEngineAPI* api;
        TprWindow window;
        TprAction quitAction;
        TprAsset model;
};



extern "C" {



int32_t init(void** ctx, const TprEngineAPI* api) noexcept {

    Plugin* plugin = new Plugin;
    if (!plugin) return -1;

    *ctx = plugin;
    plugin->api = api;

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

    TprComponent c1;
    TprComponent c2;
    TprEntity e1;
    TprEntity e2;

    const char hello[] = "hello, world!";
    const char bye[] = "bye, world!";

    char buffer[20];
    std::fill(buffer, buffer + std::size(buffer), 'q');
    buffer[std::size(buffer) - 1] = '\0';

    ROF(plugin->api->scene->createComponent(sizeof(hello), &c1));
    ROF(plugin->api->scene->createComponent(sizeof(bye), &c2));

    TprComponent components[] = { c1, c2 };

    ROF(plugin->api->scene->spawnEntity(&c1, 1, &e1));
    ROF(plugin->api->scene->spawnEntity(components, 2, &e2));

    ROF(plugin->api->scene->writeEntityComponentData(e1, c1, hello, 0, 0));
    ROF(plugin->api->scene->writeEntityComponentData(e2, c1, hello, 0, 0));

    ROF(plugin->api->scene->writeEntityComponentData(e2, c2, bye, 0, 0));

    ROF(plugin->api->scene->copyEntityComponentData(e1, c1, 0, 0, buffer));
    plugin->api->log->info(std::format("{}\n", buffer).c_str());
    ROF(plugin->api->scene->copyEntityComponentData(e2, c1, 0, 0, buffer));
    plugin->api->log->info(std::format("{}\n", buffer).c_str());
    ROF(plugin->api->scene->copyEntityComponentData(e2, c2, 0, 0, buffer));
    plugin->api->log->info(std::format("{}\n", buffer).c_str());

    ROF(plugin->api->scene->writeEntityComponentData(e2, c1, bye, 0, 0));
    ROF(plugin->api->scene->copyEntityComponentData(e2, c1, 0, 0, buffer));
    plugin->api->log->info(std::format("{}\n", buffer).c_str());

    plugin->api->scene->destroyComponent(c1);

    // ROF(plugin->api->scene->copyEntityComponentData(e1, c1, 0, 0, buffer));
    // plugin->api->log->info(std::format("{}\n", buffer).c_str());
    // ROF(plugin->api->scene->copyEntityComponentData(e2, c1, 0, 0, buffer));
    // plugin->api->log->info(std::format("{}\n", buffer).c_str());
    ROF(plugin->api->scene->copyEntityComponentData(e2, c2, 0, 0, buffer));
    plugin->api->log->info(std::format("{}\n", buffer).c_str());

    /*
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
    delete plugin;
}



int32_t updatePerFrame(void* ctx) noexcept {
    Plugin* plugin = reinterpret_cast<Plugin*>(ctx);

    TprActionState quitActionState;
    ROF(plugin->api->input->getActionState(plugin->quitAction, &quitActionState));
    if (quitActionState.state) {
        plugin->api->wm->closeWindow(plugin->window);
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
