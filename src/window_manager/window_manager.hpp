

#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_



#include "plugin_core.h"
#include "hardware_common_structs.hpp"

#include <unordered_map>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>



class WindowManager {

    public:
        void init(GraphicsBackend graphicsBackend);
        void shutdown() noexcept;
        void update();

        bool lost() const;
        std::vector<TprWindow> getWindows();

        // vulkan-specific functional
        std::vector<const char*> getExtensionsVk(TprWindow handle) const;
        VkSurfaceKHR createSurfaceVk(TprWindow handle, VkInstance instance) const;

        // plugin API
        TprResult openWindow(TprWindow* pHandle, const TprWindowCreateInfo* createInfo) noexcept;
        void closeWindow(TprWindow handle) noexcept;
        TprResult getWindowWidth(TprWindow handle, int32_t* pWidth) noexcept;
        TprResult getWindowHeight(TprWindow handle, int32_t* pHeight) noexcept;
        TprResult hasWindowResized(TprWindow handle, TprBool8* pValue) noexcept;

    private:

        struct Window {
            SDL_Window* window;
            Uint32 id;
            uint32_t generation;
            bool actual = true;
            Uint8 flags;
            TprWindow handle;
            bool resized = false;
        };

        Uint32 mWindowFlags = 0;
        uint32_t mActualWindowCount = 0;
        bool mLost = false;
        std::vector<Window> mWindows;
        std::vector<size_t> mFreeWindows;
        std::unordered_map<Uint32, size_t> mWindowIDToWindow;

};



#endif  // WINDOW_MANAGER_WINDOW_MANAGER_HPP_

