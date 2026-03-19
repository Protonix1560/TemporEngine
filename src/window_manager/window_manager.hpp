

#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_



#include "core.hpp"
#include "hardware_layer_interface.hpp"
#include "plugin_core.h"
#include "hardware_common_structs.hpp"
#include "logger.hpp"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <unordered_map>
#include <vector>
#include <atomic>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>



// from "hardware_layer_interface.hpp"
class HardwareLayer;



class WindowManager {

    public:
        WindowManager(GraphicsBackend graphicsBackend, Logger& logger, std::atomic<int32_t>& rAliveTokens);
        ~WindowManager() noexcept;
        WindowManager(const WindowManager& other) = delete;
        void update();
        void setHWLI(HardwareLayer* pHWLI);

        std::vector<TprWindow> getWindows();

        // vulkan-specific functional
        std::vector<const char*> getExtensionsVk(TprWindow handle) const;
        VkSurfaceKHR createSurfaceVk(TprWindow handle, VkInstance instance) const;

        // plugin API
        expected<TprWindow, TprResult> openWindow(const TprWindowCreateInfo* pCreateInfo) noexcept;
        void closeWindow(TprWindow window) noexcept;
        expected<int32_t, TprResult> getWindowWidth(TprWindow window) noexcept;
        expected<int32_t, TprResult> getWindowHeight(TprWindow window) noexcept;
        expected<TprBool8, TprResult> hasWindowResized(TprWindow window) noexcept;

        expected<TprAction, TprResult> createAction(TprWindow window, const TprActionCreateInfo* pCreateInfo) noexcept;
        void destroyAction(TprAction action) noexcept;
        TprResult getActionState(TprAction action, TprActionState* pState) noexcept;
        TprResult getInputElementVector(TprWindow window, TprInputElement inputElement, TprInputElementVector* pVector) noexcept;

    private:

        struct Action {
            TprInputElement element;
            float highThreshold;
            float lowThreshold;
            uint32_t windowIndex;
            TprBool8 state = false;
            uint32_t frames = 0;
        };

        struct Window {
            SDL_Window* window;
            bool resized = false;
            Uint32 id;
            TprWindow handle;
            uint32_t index;

            std::unordered_map<TprInputElement, TprInputElementVector> elements;
            std::unordered_map<uint32_t, Action> actions;
        };

        Logger& mrLogger;
        std::atomic<int32_t>& mrAliveTokens;
        HardwareLayer* mpHWLI = nullptr;

        Uint32 mWindowFlags = 0;
        uint32_t mWindowCounter = 0;
        std::unordered_map<uint32_t, Window> mWindows;

        Window mSentinelWindow;

        uint32_t mActionCounter = 0;
        std::unordered_map<uint32_t, uint32_t> mActionMap;

        std::unordered_map<SDL_Scancode, TprInputElement> mKeyMap;
        std::unordered_map<uint32_t, TprInputElement> mMouseButtonMap;

        void destroyWindow(Window& window) noexcept;
        void updateWindowActions(Window& window, TprInputElement element, const TprInputElementVector& vector);

};

REGISTER_TYPE_NAME_S(WindowManager, "WinM");



#endif  // WINDOW_MANAGER_WINDOW_MANAGER_HPP_

