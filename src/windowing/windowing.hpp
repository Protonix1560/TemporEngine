
#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_

#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "graphics_common.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <condition_variable>
#include <limits>
#include <mutex>
#include <vector>

#include <vulkan/vulkan.h>


// from "i_graphics_Device.hpp"
class IGraphicsDevice;

// from "scheduler.hpp"
class Scheduler;


class Windowing {

    public:
        Windowing(Logger logger, Scheduler& rSched, std::atomic<TprResult>& rRunResult);
        TprResult init(IGraphicsDevice* pIGD, GraphicsAPI graphics);
        ~Windowing() noexcept;

        TprResult update();

        void eventLoopEnded();

        expected<TprWindow, TprResult> openWindow(const TprWindowCreateInfo& info) noexcept;
        expected<TprWindow, TprResult> createWindowCapability(TprWindow window, TprWindowCapabilityFlags mask) noexcept;
        void closeWindow(TprWindow window) noexcept;
        expected<TprAction, TprResult> createAction(const TprActionCreateInfo& info) noexcept;
        expected<TprAction, TprResult> createActionCapability(TprAction action, TprActionCapabilityFlags mask) noexcept;
        void destroyAction(TprAction action) noexcept;

        expected<uint32_t, TprResult> getActionsHistorySize(uint32_t filterCount, const TprAction* pFilters) noexcept;
        TprResult copyActionsHistory(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) noexcept;
        expected<TprActionState, TprResult> getActionState(TprAction action) noexcept;

        TprJob getInputUpdateJob() noexcept;

        // ========== Graphics Device-specific API ===========
        expected<WindowIdentity, TprResult> getWindowIdentity(TprWindow window);
        expected<uint32_t, TprResult> windowPixelWidth(WindowIdentity id);
        expected<uint32_t, TprResult> windowPixelHeight(WindowIdentity id);

        // ======= Vulkan Graphics Device-specific API =======
        expected<PFN_vkGetInstanceProcAddr, TprResult> getVkGetInstanceProcAddr();
        expected<std::span<const char* const>, TprResult> getVkInstanceExtensions();
        expected<VkSurfaceKHR, TprResult> createVkSurfaceKHR(WindowIdentity id, VkInstance instance, const VkAllocationCallbacks* pAlloc);
        void destroyVkSurfaceKHR(WindowIdentity id, VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAlloc);

    private:
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
        struct GetWindowWidthQuery {
            SDL_Window* window;
            std::optional<uint32_t>& w;
        };
        struct GetWindowHeightQuery {
            SDL_Window* window;
            std::optional<uint32_t>& h;
        };
        using Query = std::variant<CreateWindowQuery, DestroyWindowQuery, GetWindowWidthQuery, GetWindowHeightQuery>;

        
        void processEvents();

        Logger mLogger;
        Scheduler& mrSched;
        IGraphicsDevice* mpGDev;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;
        std::condition_variable mCv;
        GraphicsAPI mGraphics;
        bool mInitialised = false;
        bool mEventLoopInOrder = true;

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

