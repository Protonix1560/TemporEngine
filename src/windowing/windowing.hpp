
#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_

#include "core.hpp"
#include "logger.hpp"
#include "tempor.h"
#include "graphics_common.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
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
        void eventLoopStarted();
        void eventLoopEnded();
        uint32_t openWindowCount() const;

        expected<TprWindow, TprResult> openWindow(const TprWindowCreateInfo& info) noexcept;
        expected<TprWindow, TprResult> createWindowCapability(TprWindow window, TprWindowCapabilityFlags mask) noexcept;
        void closeWindow(TprWindow window) noexcept;

        expected<TprAction, TprResult> createAction(const TprActionCreateInfo& info) noexcept;
        TprResult bindActionWindow(TprAction action, TprWindow window) noexcept;
        TprResult unbindActionWindow(TprAction action, TprWindow window) noexcept;
        TprResult setActionProfile(TprAction action, const TprActionProfile& profile) noexcept;
        expected<TprAction, TprResult> forkAction(TprAction action) noexcept;
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
            TprActionProfile profile;
            std::set<std::weak_ptr<WindowEntry>, std::owner_less<std::weak_ptr<WindowEntry>>> windows;
            TprActionState currState{{0.0f, 0.0f, 0.0f, 0.0f}, 0};
            TprActionState currAbsState{{0.0f, 0.0f, 0.0f, 0.0f}, 0};
            std::vector<TprActionState> history;

            ActionEntry(const TprActionProfile& profile) : profile(profile) {}
        };

        struct ActionHandle {
            std::shared_ptr<ActionEntry> entry;
            TprActionCapabilityFlags capability = std::numeric_limits<TprActionCapabilityFlags>::max();
        };

        struct WindowEntry {
            SDL_Window* window = nullptr;
            std::set<std::weak_ptr<ActionEntry>, std::owner_less<std::weak_ptr<ActionEntry>>> actions;

            bool valid() const { return window; }

            WindowEntry() = default;
            WindowEntry(SDL_Window* window) : window(window) {}
            WindowEntry(WindowEntry&& other) : window(other.window), actions(other.actions) {
                other.window = nullptr;
                other.actions.clear();
            }
            WindowEntry& operator=(WindowEntry&& other) {
                window = other.window;
                actions = other.actions;
                other.window = nullptr;
                other.actions.clear();
                return *this;
            }
            WindowEntry(const WindowEntry& other) = delete;
            WindowEntry& operator=(const WindowEntry& other) = delete;
        };

        struct WindowHandle {
            std::shared_ptr<WindowEntry> entry;
            TprWindowCapabilityFlags capability = std::numeric_limits<TprWindowCapabilityFlags>::max();
        };


        struct CreateWindowQuery {
            std::optional<WindowEntry>& window;
            std::string_view title;
            uint32_t width;
            uint32_t height;
            bool hidden;
            bool unresizable;
        };
        struct DestroyWindowQuery {
            WindowEntry window;
        };
        struct GetWindowWidthQuery {
            SDL_Window* window;
            std::optional<uint32_t>& width;
        };
        struct GetWindowHeightQuery {
            SDL_Window* window;
            std::optional<uint32_t>& height;
        };
        using Query = std::variant<CreateWindowQuery, DestroyWindowQuery, GetWindowWidthQuery, GetWindowHeightQuery>;

        
        void processEvents();

        Logger mLogger;
        Scheduler& mrSched;
        IGraphicsDevice* mpGDev;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;
        GraphicsAPI mGraphics = GraphicsAPI::None;
        bool mInitialised = false;

        TprJob mProcessEventsJob;
        uint64_t mTimeBeginOffset;
        std::vector<const char*> mVkInstanceExtensions;

        std::mutex mQueryMutex;
        std::condition_variable mQueryCv;
        bool mEventLoopInOrder = false;
        std::vector<Query> mMainThreadQueries;

        std::unordered_map<uint32_t, WindowHandle> mWindows;
        uint32_t mWindowCounter = 0;
        std::unordered_map<SDL_Window*, std::weak_ptr<WindowEntry>> mWindowEntries;

        std::unordered_map<uint32_t, ActionHandle> mActions;
        uint32_t mActionCounter = 0;
        std::unordered_map<uint32_t, std::weak_ptr<ActionEntry>> mActionEntries;

        const std::unordered_map<SDL_Scancode, TprInputDevice> mKeyMap;
        const std::unordered_map<uint32_t, TprInputDevice> mMouseButtonMap;

};

REGISTER_TYPE_NAME_S(Windowing, "Wndw");



#endif  // WINDOW_MANAGER_WINDOW_MANAGER_HPP_

