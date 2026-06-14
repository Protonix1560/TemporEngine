
#include "plugin.h"
#include "plugin_core.h"

#include <string>  // IWYU pragma: keep
#include <format>  // IWYU pragma: keep

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


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
        TprAction cameraAction;
        TprAction mouseAction;

        TprAction walkForwardAction;
        TprAction walkBackwardAction;
        TprAction strafeRightAction;
        TprAction strafeLeftAction;
        TprAction flyUpwardAction;
        TprAction flyDownwardAction;

        TprMesh mesh;
        TprObjectImage image;
        TprEntity object;
        TprRenderTarget target;
        TprDepthDomain domain;
        glm::vec3 camPos{};
        float camYaw = 0.0f, camPitch = 0.0f;
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
    ROF(plugin->api->win->openWindow(&windowCreateInfo, &plugin->window));

    TprActionCreateInfo quitActionInfo{};
    quitActionInfo.element = TPR_KEY_ESCAPE;
    quitActionInfo.lowThreshold = 0.3f;
    quitActionInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &quitActionInfo, &plugin->quitAction));

    TprActionCreateInfo cameraActionInfo{};
    cameraActionInfo.element = TPR_MOUSE_MOTION;
    cameraActionInfo.lowThreshold = 0.0f;
    cameraActionInfo.highThreshold = 0.0f;
    ROF(plugin->api->input->createAction(plugin->window, &cameraActionInfo, &plugin->cameraAction));

    TprActionCreateInfo mouseActionInfo{};
    mouseActionInfo.element = TPR_MOUSE_BUTTON1;
    mouseActionInfo.lowThreshold = 0.3f;
    mouseActionInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &mouseActionInfo, &plugin->mouseAction));

    TprActionCreateInfo walkForwardInfo{};
    walkForwardInfo.element = TPR_KEY_W;
    walkForwardInfo.lowThreshold = 0.3f;
    walkForwardInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &walkForwardInfo, &plugin->walkForwardAction));

    TprActionCreateInfo walkBackwardInfo{};
    walkBackwardInfo.element = TPR_KEY_S;
    walkBackwardInfo.lowThreshold = 0.3f;
    walkBackwardInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &walkBackwardInfo, &plugin->walkBackwardAction));

    TprActionCreateInfo strafeRightInfo{};
    strafeRightInfo.element = TPR_KEY_D;
    strafeRightInfo.lowThreshold = 0.3f;
    strafeRightInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &strafeRightInfo, &plugin->strafeRightAction));
    
    TprActionCreateInfo strafeLeftInfo{};
    strafeLeftInfo.element = TPR_KEY_A;
    strafeLeftInfo.lowThreshold = 0.3f;
    strafeLeftInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &strafeLeftInfo, &plugin->strafeLeftAction));

    TprActionCreateInfo flyUpwardInfo{};
    flyUpwardInfo.element = TPR_KEY_E;
    flyUpwardInfo.lowThreshold = 0.3f;
    flyUpwardInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &flyUpwardInfo, &plugin->flyUpwardAction));
    
    TprActionCreateInfo flyDownwardInfo{};
    flyDownwardInfo.element = TPR_KEY_Q;
    flyDownwardInfo.lowThreshold = 0.3f;
    flyDownwardInfo.highThreshold = 0.7f;
    ROF(plugin->api->input->createAction(plugin->window, &flyDownwardInfo, &plugin->flyDownwardAction));

    TprResource modelResource;
    ROF(plugin->api->vfs->openPathResource("plugins/test/model.glb", 0, &modelResource));
    TprMeshCreateInfo parseInfo{};
    parseInfo.resource = modelResource;
    parseInfo.index = 0;
    ROF(plugin->api->geo->createMesh(&parseInfo, &plugin->mesh));
    plugin->api->vfs->closeResource(modelResource);

    TprMeshLoadInfo loadInfo{};
    ROF(plugin->api->geo->loadMesh(plugin->mesh, &loadInfo));

    TprDepthDomainCreateInfo domainInfo{};
    ROF(plugin->api->render->createDepthDomain(&domainInfo, &plugin->domain));

    TprRenderTargetCreateInfo targetInfo{};
    targetInfo.depthDomain = plugin->domain;
    targetInfo.scissor = {0, 0, 1300, 800};
    targetInfo.viewport = {0.0f, 0.0f, 1300.0f, 800.0f, 0.0f, 1.0f};
    targetInfo.window = plugin->window;
    ROF(plugin->api->render->createRenderTarget(&targetInfo, &plugin->target));

    TprObjectImageCreateInfo imageInfo{};
    imageInfo.mesh = plugin->mesh;
    imageInfo.pRenderTargets = &plugin->target;
    imageInfo.renderTargetCount = 1;
    ROF(plugin->api->render->createObjectImage(&imageInfo, &plugin->image));

    TprComponent components[] = {
        plugin->api->render->getComponentRenderable()
    };
    ROF(plugin->api->scene->spawnEntity(components, std::size(components), &plugin->object));

    TprComponentRenderable renderable{};
    renderable.image = plugin->image;
    renderable.transform.x0 = 1.0f;
    renderable.transform.y1 = 1.0f;
    renderable.transform.z2 = 1.0f;
    renderable.transform.w3 = 1.0f;
    renderable.transform.z0 = 3.0f;
    renderable.transform.x0 = 0.5f;
    ROF(plugin->api->scene->writeEntityComponentData(
        plugin->object, plugin->api->render->getComponentRenderable(), reinterpret_cast<const char*>(&renderable), 0, 0
    ));

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
        plugin->api->win->closeWindow(plugin->window);
    }

    TprActionState cameraActionState;
    TprActionState mouseActionState;
    ROF(plugin->api->input->getActionState(plugin->cameraAction, &cameraActionState));
    ROF(plugin->api->input->getActionState(plugin->mouseAction, &mouseActionState));

    TprActionState walkForwardState;
    TprActionState walkBackwardState;
    TprActionState strafeRightState;
    TprActionState strafeLeftState;
    TprActionState flyUpwardState;
    TprActionState flyDownwardState;
    ROF(plugin->api->input->getActionState(plugin->walkForwardAction, &walkForwardState));
    ROF(plugin->api->input->getActionState(plugin->walkBackwardAction, &walkBackwardState));
    ROF(plugin->api->input->getActionState(plugin->strafeRightAction, &strafeRightState));
    ROF(plugin->api->input->getActionState(plugin->strafeLeftAction, &strafeLeftState));
    ROF(plugin->api->input->getActionState(plugin->flyUpwardAction, &flyUpwardState));
    ROF(plugin->api->input->getActionState(plugin->flyDownwardAction, &flyDownwardState));

    bool update = false;

    if (mouseActionState.state) {
        plugin->camYaw += cameraActionState.vector.x * 0.01f;
        plugin->camPitch += cameraActionState.vector.y * 0.01f;
        plugin->camPitch = std::clamp(plugin->camPitch, -1.55f, 1.55f);
        update = true;
    }

    glm::vec3 front;
    front.x = std::cos(plugin->camYaw) * std::cos(plugin->camPitch);
    front.y = std::sin(plugin->camPitch);
    front.z = std::sin(plugin->camYaw) * std::cos(plugin->camPitch);
    front = glm::normalize(front);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(front, up);

    if (walkForwardState.state) {
        plugin->camPos += front * 0.03f;
        update = true;
    }
    if (walkBackwardState.state) {
        plugin->camPos -= front * 0.03f;
        update = true;
    }
    if (strafeRightState.state) {
        plugin->camPos += right * 0.03f;
        update = true;
    }
    if (strafeLeftState.state) {
        plugin->camPos -= right * 0.03f;
        update = true;
    }
    if (flyUpwardState.state) {
        plugin->camPos -= up * 0.03f;
        update = true;
    }
    if (flyDownwardState.state) {
        plugin->camPos += up * 0.03f;
        update = true;
    }

    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat[1][1] = -1.0f;
    glm::mat4 viewMat = glm::lookAt(plugin->camPos, plugin->camPos + front, up);
    glm::mat4 projMat = glm::perspective(1.7f, 1300.0f / 800.0f, 0.01f, 1000.0f);
    glm::mat4 mvpMat = projMat * viewMat * modelMat;

    if (update) {
        TprComponentRenderable renderable{};
        renderable.image = plugin->image;
        renderable.transform.x0 = mvpMat[0][0];
        renderable.transform.x1 = mvpMat[1][0];
        renderable.transform.x2 = mvpMat[2][0];
        renderable.transform.x3 = mvpMat[3][0];
        renderable.transform.y0 = mvpMat[0][1];
        renderable.transform.y1 = mvpMat[1][1];
        renderable.transform.y2 = mvpMat[2][1];
        renderable.transform.y3 = mvpMat[3][1];
        renderable.transform.z0 = mvpMat[0][2];
        renderable.transform.z1 = mvpMat[1][2];
        renderable.transform.z2 = mvpMat[2][2];
        renderable.transform.z3 = mvpMat[3][2];
        renderable.transform.w0 = mvpMat[0][3];
        renderable.transform.w1 = mvpMat[1][3];
        renderable.transform.w2 = mvpMat[2][3];
        renderable.transform.w3 = mvpMat[3][3];
        ROF(plugin->api->scene->writeEntityComponentData(
            plugin->object, plugin->api->render->getComponentRenderable(),
            reinterpret_cast<const char*>(&renderable), 0, 0
        ));
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
