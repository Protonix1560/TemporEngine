
#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_

#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "i_graphics_device/graphics_common.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>


// from "hardware_layer_interface.hpp"
class IGraphicsDevice;

// from "scheduler.hpp"
class Scheduler;


struct WindowEntry;

struct ActionEntry {
    TprInputDevice device;
    TprActionMeasureType measureType;
    WindowEntry& window;
    TprActionState currState{{0.0f, 0.0f, 0.0f, 0.0f}, 0};
    TprActionState currAbsState{{0.0f, 0.0f, 0.0f, 0.0f}, 0};
    std::vector<TprActionState> history;
    std::vector<uint32_t> handles;

    ActionEntry(WindowEntry& window, TprInputDevice device, TprActionMeasureType measure)
        : window(window), device(device), measureType(measure) {}
};

struct ActionHandle {
    std::shared_ptr<ActionEntry> entry;
    TprActionCapabilityFlags capability = std::numeric_limits<TprActionCapabilityFlags>::max();
};

struct WindowEntry {
    SDL_Window* window;
    std::vector<std::shared_ptr<ActionEntry>> actions;
};

struct WindowHandle {
    std::shared_ptr<WindowEntry> entry;
    TprWindowCapabilityFlags capability = std::numeric_limits<TprWindowCapabilityFlags>::max();
};


struct CreateWindowQuery {
    std::optional<SDL_Window*>& window;
    const char* title;
    int w;
    int h;
    Uint32 flags;
};

struct DestroyWindowQuery {
    SDL_Window* window;
};

using Query = std::variant<CreateWindowQuery, DestroyWindowQuery>;


class Windowing {

    public:
        Windowing(Logger logger, Scheduler& rSched, GraphicsAPI graphics);
        TprResult init();
        ~Windowing() noexcept;

        TprResult update();

        expected<TprWindow, TprResult> openWindow(const TprWindowCreateInfo* pInfo) noexcept;
        expected<TprWindow, TprResult> createWindowCapability(TprWindow window, TprWindowCapabilityFlags mask) noexcept;
        void closeWindow(TprWindow window) noexcept;
        expected<TprAction, TprResult> createAction(const TprActionCreateInfo* pInfo) noexcept;
        expected<TprAction, TprResult> createActionCapability(TprAction action, TprActionCapabilityFlags mask) noexcept;
        void destroyAction(TprAction action) noexcept;

        expected<uint32_t, TprResult> getActionsHistorySize(uint32_t filterCount, const TprAction* pFilters) noexcept;
        TprResult copyActionsHistory(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) noexcept;
        expected<TprActionState, TprResult> getActionState(TprAction action) noexcept;

        TprJob getInputUpdateJob() noexcept;

        // ========== Graphics Device-specific API ===========
        expected<uint32_t, TprResult> windowPixelWidth(TprWindow window);
        expected<uint32_t, TprResult> windowPixelHeight(TprWindow window);

        // ======= Vulkan Graphics Device-specific API =======
        expected<PFN_vkGetInstanceProcAddr, TprResult> getVkGetInstanceProcAddr();
        expected<std::span<const char* const>, TprResult> getVkInstanceExtensions();
        expected<VkSurfaceKHR, TprResult> createVkSurfaceKHR(TprWindow window, VkInstance instance, const VkAllocationCallbacks* pAlloc);
        void destroyVkSurfaceKHR(TprWindow window, VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAlloc);

    private:

        void processEvents();

        Logger mLogger;
        Scheduler& mrSched;
        std::mutex mMutex;
        std::condition_variable mCv;
        GraphicsAPI mGraphics;
        bool mInitialized = false;

        TprJob mProcessEventsJob;
        uint64_t mTimeBeginOffset;
        std::vector<Query> mMainThreadQueries;
        std::vector<const char*> mVkInstanceExtensions;

        std::unordered_map<uint32_t, WindowHandle> mWindowHandles;
        uint32_t mWindowCounter = 0;
        std::unordered_map<SDL_Window*, std::shared_ptr<WindowEntry>> mWindowEntryMap;

        std::unordered_map<uint32_t, ActionHandle> mActions;
        uint32_t mActionCounter = 0;

        const std::unordered_map<SDL_Scancode, TprInputDevice> mKeyMap;
        const std::unordered_map<uint32_t, TprInputDevice> mMouseButtonMap;

};

REGISTER_TYPE_NAME_S(Windowing, "Wndw");



#endif  // WINDOW_MANAGER_WINDOW_MANAGER_HPP_

