
#include "window_manager.hpp"

#include <SDL2/SDL_video.h>
#include <sstream>




#define SDL_LOG_FAIL(com) do { \
    const char* msg = SDL_GetError(); \
    std::ostringstream os; \
    os << "\033[91mSDL FATAL ERROR at " << __FILE__ << ":" << __LINE__ << "\n" << msg << "\n" << com << "\033[0m\n"; \
    throw Exception(ErrCode::InternalError, os.str()); \
} while(0)



void WindowManager::init(uint32_t width, uint32_t height, const char* name, BackendRealization backend) {

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) SDL_LOG_FAIL("Failed to initialze SDL subsystem VIDEO");

    Uint32 flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    switch (backend) {
        case BackendRealization::VULKAN: flags |= SDL_WINDOW_VULKAN;
    }

    mWindow = SDL_CreateWindow(name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);

    if (!mWindow) {
        SDL_Quit();
        SDL_LOG_FAIL("Failed to create SDL window");
    }
}



void WindowManager::showWindow() {
    SDL_ShowWindow(mWindow);
}



bool WindowManager::shouldClose() {
    return !mRunning;
}



void WindowManager::update() {

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            mRunning = false;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                mRunning = false;
            }
        }
    }

}



void WindowManager::shutdown() noexcept {
    
    SDL_DestroyWindow(mWindow);

    SDL_Quit();

}



std::vector<const char*> WindowManager::getExtensionsVk() const {

    uint32_t count;

    if (!SDL_Vulkan_GetInstanceExtensions(mWindow, &count, nullptr))
        throw Exception(ErrCode::InternalError, "Failed to get vulkan instance extensions");

    std::vector<const char*> extensions(count);

    if (!SDL_Vulkan_GetInstanceExtensions(mWindow, &count, extensions.data()))
        throw Exception(ErrCode::InternalError, "Failed to get vulkan instance extensions");

    return std::move(extensions);
}



VkSurfaceKHR WindowManager::createSurfaceVk(VkInstance instance) const {
    VkSurfaceKHR surface;
    if(!SDL_Vulkan_CreateSurface(mWindow, instance, &surface)) throw Exception(ErrCode::InternalError, "Failed to create vulkan surface");
    return surface;
}



uint32_t WindowManager::getWidth() const {
    int width, height;
    SDL_GetWindowSizeInPixels(mWindow, &width, &height);
    return static_cast<uint32_t>(width);
}



uint32_t WindowManager::getHeight() const {
    int width, height;
    SDL_GetWindowSizeInPixels(mWindow, &width, &height);
    return static_cast<uint32_t>(height);
}



void WindowManager::setTitle(const char* title) {
    SDL_SetWindowTitle(mWindow, title);
}
