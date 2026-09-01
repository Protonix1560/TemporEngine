
#include "plugin.h"
#include "plugin_core.h"

#include <string>
#include <format>
#include <thread>  // IWYU pragma: keep
#include <mutex>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


using namespace std::string_literals;
using namespace std::chrono_literals;


#define ROF(__expr) do {            \
    auto __r = (__expr);            \
    if (__r < 0) return __r;        \
} while (0)

#define LOF(__expr) do {                                                                                                      \
    auto __r = (__expr);                                                                                                      \
    if (__r < 0) {                                                                                                            \
        plugin->api->out->error(std::format("Test plugin error at {}: {}", __LINE__, static_cast<int32_t>(__r)).c_str());     \
        return;                                                                                                               \
    }                                                                                                                         \
} while (0)


class PluginWrapper {
    public:
        std::mutex mutex;

        const TprEngineAPI* api;
        TprWindow window;

        TprJob frameJob;
        TprJob updateJob;
        TprJob shutdownJob;

        TprAction quitAction;
        TprAction cameraAction;
        TprAction mouseAction;

        bool mousePressed = false;
        bool walkForwardState = false;
        bool walkBackwardState = false;
        bool strafeRightState = false;
        bool strafeLeftState = false;
        bool flyUpwardState = false;
        bool flyDownwardState = false;

        TprAction walkForwardAction;
        TprAction walkBackwardAction;
        TprAction strafeRightAction;
        TprAction strafeLeftAction;
        TprAction flyUpwardAction;
        TprAction flyDownwardAction;

        TprMesh mesh;
        TprEntityImage image;
        TprEntity object;
        TprRenderTarget target;
        TprRenderTargetSet targetSet;
        TprDepthDomain domain;
        glm::vec3 camPos{};
        float camYaw = 0.0f, camPitch = 0.0f;
};


int32_t testScheduler(PluginWrapper* plugin) {

    // #define TEST_SCHEDULER_SCENARIO_1

    #ifdef TEST_SCHEDULER_SCENARIO_1
    {
        TprResult result;

        TprJob jobA;
        TprJobCreateInfo jobAInfo{};
        jobAInfo.context = plugin;
        jobAInfo.duration = TPR_JOB_DURATION_SHORT;
        jobAInfo.function = [](void* ctx, TprJob job) noexcept -> void {
            PluginWrapper* plugin = reinterpret_cast<PluginWrapper*>(ctx);
            plugin->api->out->info("\e[91mJOB A");
            plugin->api->sched->scheduleJob(job, plugin->api->sched->now() + 100'000'000);
        };
        jobAInfo.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
        plugin->api->out->infoStyled(TPR_LOG_STYLE_TIMESTAMP1, "\e[41m ======== CREATING JOB A ======== \e[0m");
        ROF(plugin->api->sched->createJob(&jobAInfo, &jobA));
        ROF(plugin->api->sched->scheduleJob(jobA, plugin->api->sched->now() + 1'000'000'000));

        std::this_thread::sleep_for(1500ms);

        TprJob jobB;
        TprJobCreateInfo jobBInfo{};
        jobBInfo.context = plugin;
        jobBInfo.duration = TPR_JOB_DURATION_SHORT;
        jobBInfo.pDependencies = &jobA;
        jobBInfo.dependencyCount = 1;
        jobBInfo.function = [](void* ctx, TprJob job) noexcept -> void {
            PluginWrapper* plugin = reinterpret_cast<PluginWrapper*>(ctx);
            plugin->api->out->info("\e[92mJOB B");
        };
        jobBInfo.triggerType = TPR_JOB_TRIGGER_TYPE_DEPENDENCIES;
        plugin->api->out->infoStyled(TPR_LOG_STYLE_TIMESTAMP1, "\e[42m ======== CREATING JOB B ======== \e[0m");
        ROF(plugin->api->sched->createJob(&jobBInfo, &jobB));

        std::this_thread::sleep_for(500ms);

        plugin->api->out->infoStyled(TPR_LOG_STYLE_TIMESTAMP1, "\e[42m ======== DESTROYING JOB B ======== \e[0m");
        plugin->api->sched->pendJobDestruction(jobB);

        std::this_thread::sleep_for(1000ms);

        plugin->api->out->infoStyled(TPR_LOG_STYLE_TIMESTAMP1, "\e[41m ======== DESTROYING JOB A ======== \e[0m");
        plugin->api->sched->pendJobDestruction(jobA);

        std::this_thread::sleep_for(200ms);
    }
    #endif  // TEST_SCHEDULER_SCENARIO_1

    return 0;
}


void update(void* ctx, TprJob job) noexcept {
    PluginWrapper* plugin = reinterpret_cast<PluginWrapper*>(ctx);

    std::lock_guard<std::mutex> lock(plugin->mutex);

    // quit action
    {
        uint32_t size;
        LOF(plugin->api->win->getActionsHistorySize(1, &plugin->quitAction, &size));
        std::vector<TprActionHistoryEntry> history(size);
        LOF(plugin->api->win->copyActionsHistory(history.data(), 1, &plugin->quitAction));
        for (const auto& entry : history) {
            if (entry.state.vector.x > 0.0f) {
                plugin->api->win->closeWindow(plugin->window);
            }
        }
    }

    // camera movement actions
    {
        TprAction actions[] = {
            plugin->cameraAction,
            plugin->mouseAction,
            plugin->walkForwardAction,
            plugin->walkBackwardAction,
            plugin->strafeLeftAction,
            plugin->strafeRightAction,
            plugin->flyUpwardAction,
            plugin->flyDownwardAction
        };

        uint32_t size;
        LOF(plugin->api->win->getActionsHistorySize(std::size(actions), actions, &size));
        std::vector<TprActionHistoryEntry> history(size);
        LOF(plugin->api->win->copyActionsHistory(history.data(), std::size(actions), actions));
        for (const auto& entry : history) {
            if (entry.action._d == plugin->mouseAction._d) {
                if (entry.state.vector.x > 0.0f) {
                    plugin->mousePressed = true;
                } else {
                    plugin->mousePressed = false;
                }
            } else if (entry.action._d == plugin->cameraAction._d) {
                if (plugin->mousePressed) {
                    plugin->camYaw += entry.state.vector.x * 0.01f;
                    plugin->camPitch += entry.state.vector.y * 0.01f;
                    plugin->camPitch = std::clamp(plugin->camPitch, -1.55f, 1.55f);
                }
            } else {
                if (entry.action._d == plugin->walkForwardAction._d) {
                    plugin->walkForwardState = (entry.state.vector.x != 0.0f);
                } else if (entry.action._d == plugin->walkBackwardAction._d) {
                    plugin->walkBackwardState = (entry.state.vector.x != 0.0f);
                } else if (entry.action._d == plugin->strafeRightAction._d) {
                    plugin->strafeRightState = (entry.state.vector.x != 0.0f);
                } else if (entry.action._d == plugin->strafeLeftAction._d) {
                    plugin->strafeLeftState = (entry.state.vector.x != 0.0f);
                } else if (entry.action._d == plugin->flyUpwardAction._d) {
                    plugin->flyUpwardState = (entry.state.vector.x != 0.0f);
                } else if (entry.action._d == plugin->flyDownwardAction._d) {
                    plugin->flyDownwardState = (entry.state.vector.x != 0.0f);
                }
            }
        }
    }
}


void frame(void* ctx, TprJob job) noexcept {
    PluginWrapper* plugin = reinterpret_cast<PluginWrapper*>(ctx);

    std::lock_guard<std::mutex> lock(plugin->mutex);

    glm::vec3 front;
    front.x = std::cos(plugin->camYaw) * std::cos(plugin->camPitch);
    front.y = std::sin(plugin->camPitch);
    front.z = std::sin(plugin->camYaw) * std::cos(plugin->camPitch);
    front = glm::normalize(front);
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(front, up);

    if (plugin->walkForwardState) {
        plugin->camPos += front * 0.03f;
    }
    if (plugin->walkBackwardState) {
        plugin->camPos -= front * 0.03f;
    }
    if (plugin->strafeRightState) {
        plugin->camPos += right * 0.03f;
    }
    if (plugin->strafeLeftState) {
        plugin->camPos -= right * 0.03f;
    }
    if (plugin->flyDownwardState) {
        plugin->camPos += up * 0.03f;
    }
    if (plugin->flyUpwardState) {
        plugin->camPos -= up * 0.03f;
    }

    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat[1][1] = -1.0f;
    glm::mat4 viewMat = glm::lookAt(plugin->camPos, plugin->camPos + front, up);
    glm::mat4 projMat = glm::perspective(1.7f, 1300.0f / 800.0f, 0.01f, 1000.0f);
    glm::mat4 mvpMat = projMat * viewMat * modelMat;

    TprComponentRenderable renderable{};
    renderable.entityImage = plugin->image;
    renderable.renderTargetSet = plugin->targetSet;
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
    LOF(plugin->api->scene->writeEntityComponentData(
        plugin->object, plugin->api->render->getComponentRenderable(),
        reinterpret_cast<const char*>(&renderable), 0, 0
    ));
}


extern "C" {

    int32_t pluginInit(const TprEngineAPI* api) noexcept {

        static PluginWrapper pluginStorage{};
        auto* plugin = &pluginStorage;

        plugin->api = api;

        ROF(testScheduler(plugin));

        TprWindowCreateInfo windowCreateInfo{};
        windowCreateInfo.name = "Tempor Testing Initiative";
        windowCreateInfo.width = 1300;
        windowCreateInfo.height = 800;
        ROF(plugin->api->win->openWindow(&windowCreateInfo, &plugin->window));

        TprActionCreateInfo quitActionInfo{};
        quitActionInfo.device = TPR_KEY_ESCAPE;
        quitActionInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&quitActionInfo, &plugin->quitAction));

        TprActionCreateInfo cameraActionInfo{};
        cameraActionInfo.device = TPR_MOUSE_MOTION;
        cameraActionInfo.measureType = TPR_MEASURE_TYPE_DIFFERENCE;
        cameraActionInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&cameraActionInfo, &plugin->cameraAction));

        TprActionCreateInfo mouseActionInfo{};
        mouseActionInfo.device = TPR_MOUSE_BUTTON1;
        mouseActionInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&mouseActionInfo, &plugin->mouseAction));

        TprActionCreateInfo walkForwardInfo{};
        walkForwardInfo.device = TPR_KEY_W;
        walkForwardInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        walkForwardInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&walkForwardInfo, &plugin->walkForwardAction));

        TprActionCreateInfo walkBackwardInfo{};
        walkBackwardInfo.device = TPR_KEY_S;
        walkBackwardInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        walkBackwardInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&walkBackwardInfo, &plugin->walkBackwardAction));

        TprActionCreateInfo strafeRightInfo{};
        strafeRightInfo.device = TPR_KEY_D;
        strafeRightInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        strafeRightInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&strafeRightInfo, &plugin->strafeRightAction));
        
        TprActionCreateInfo strafeLeftInfo{};
        strafeLeftInfo.device = TPR_KEY_A;
        strafeLeftInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        strafeLeftInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&strafeLeftInfo, &plugin->strafeLeftAction));

        TprActionCreateInfo flyUpwardInfo{};
        flyUpwardInfo.device = TPR_KEY_E;
        flyUpwardInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        flyUpwardInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&flyUpwardInfo, &plugin->flyUpwardAction));
        
        TprActionCreateInfo flyDownwardInfo{};
        flyDownwardInfo.device = TPR_KEY_Q;
        flyDownwardInfo.measureType = TPR_MEASURE_TYPE_ABSOLUTE;
        flyDownwardInfo.window = plugin->window;
        ROF(plugin->api->win->createAction(&flyDownwardInfo, &plugin->flyDownwardAction));

        TprFile modelFile;
        ROF(plugin->api->fs->openFile("plugins/test/model.glb", 0, &modelFile));
        TprMeshCreateInfo parseInfo{};
        parseInfo.data = modelFile;
        parseInfo.index = 0;
        ROF(plugin->api->geo->createMesh(&parseInfo, &plugin->mesh));
        plugin->api->fs->closeFile(modelFile);

        ROF(plugin->api->geo->requireMeshLoaded(plugin->mesh));

        TprDepthDomainCreateInfo domainInfo{};
        ROF(plugin->api->render->createDepthDomain(&domainInfo, &plugin->domain));

        TprRenderTargetCreateInfo targetInfo{};
        targetInfo.depthDomain = plugin->domain;
        targetInfo.scissor = {0, 0, 1300, 800};
        targetInfo.viewport = {0.0f, 0.0f, 1300.0f, 800.0f, 0.0f, 1.0f};
        targetInfo.window = plugin->window;
        ROF(plugin->api->render->createRenderTarget(&targetInfo, &plugin->target));

        TprRenderTargetSetCreateInfo targetSetInfo{};
        targetSetInfo.targetCount = 1;
        targetSetInfo.pTargets = &plugin->target;
        ROF(plugin->api->render->createRenderTargetSet(&targetSetInfo, &plugin->targetSet));

        TprEntityImageCreateInfo imageInfo{};
        imageInfo.mesh = plugin->mesh;
        ROF(plugin->api->render->createEntityImage(&imageInfo, &plugin->image));

        TprComponent componentRenderable = plugin->api->render->getComponentRenderable();
        ROF(plugin->api->scene->spawnEntity(&componentRenderable, 1, &plugin->object));
        TprComponentRenderable renderableData{};
        renderableData.entityImage = plugin->image;
        renderableData.renderTargetSet = plugin->targetSet;
        ROF(plugin->api->scene->writeEntityComponentData(
            plugin->object, componentRenderable, reinterpret_cast<const char*>(&renderableData), 0, 0
        ));

        TprJob renderJobs[2] = {
            plugin->api->render->getRenderJob(),
            plugin->api->render->getRenderSignalJob()
        };

        TprJobCreateInfo frameInfo{};
        frameInfo.context = plugin;
        frameInfo.triggerType = TPR_JOB_TRIGGER_TYPE_DEPENDENCIES;
        frameInfo.dependencyCount = std::size(renderJobs);
        frameInfo.pDependencies = renderJobs;
        frameInfo.duration = TPR_JOB_DURATION_SHORT;
        frameInfo.function = frame;
        ROF(plugin->api->sched->createJob(&frameInfo, &plugin->frameJob));

        TprJob updateJob = plugin->api->win->getInputUpdateJob();

        TprJobCreateInfo updateInfo{};
        updateInfo.context = plugin;
        updateInfo.triggerType = TPR_JOB_TRIGGER_TYPE_DEPENDENCIES;
        updateInfo.dependencyCount = 1;
        updateInfo.pDependencies = &updateJob;
        updateInfo.duration = TPR_JOB_DURATION_SHORT;
        updateInfo.function = update;
        ROF(plugin->api->sched->createJob(&updateInfo, &plugin->updateJob));

        TprJob shutdownJob = plugin->api->sched->getShutdownJob();

        TprJobCreateInfo shutdownInfo{};
        shutdownInfo.context = plugin;
        shutdownInfo.triggerType = TPR_JOB_TRIGGER_TYPE_DEPENDENCIES;
        shutdownInfo.dependencyCount = 1;
        shutdownInfo.pDependencies = &shutdownJob;
        shutdownInfo.duration = TPR_JOB_DURATION_SHORT;
        shutdownInfo.function = [](void* ctx, TprJob job) noexcept {
            auto plugin = reinterpret_cast<PluginWrapper*>(ctx);
            plugin->api->out->error("Test plugin shutdown");
        };
        ROF(plugin->api->sched->createJob(&shutdownInfo, &plugin->shutdownJob));

        return 0;
    }

} // extern "C"
