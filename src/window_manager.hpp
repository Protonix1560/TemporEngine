#pragma once

#include "core.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

#include <vector>



class WindowManager {

    public:
        void init(uint32_t width, uint32_t height, const char* name, BackendRealization backend);
        void showWindow();
        void destroy() noexcept;
        bool shouldClose();
        void update();
        void setTitle(const char* title);
        uint32_t getWidth() const;
        uint32_t getHeight() const;

        std::vector<const char*> getExtensionsVk() const;
        VkSurfaceKHR createSurfaceVk(VkInstance instance) const;

    private:
        SDL_Window* mWindow;
        bool mRunning = true;

};
