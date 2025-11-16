

#ifndef WINDOW_MANAGER_WINDOW_MANAGER_HPP_
#define WINDOW_MANAGER_WINDOW_MANAGER_HPP_



#include "core.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vector>



class WindowManager {

    public:
        void init(uint32_t width, uint32_t height, const char* name, BackendRealization backend);
        void showWindow();
        void shutdown() noexcept;
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



#endif  // WINDOW_MANAGER_WINDOW_MANAGER_HPP_

