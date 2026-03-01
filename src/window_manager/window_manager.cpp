
#include "window_manager.hpp"
#include "core.hpp"
#include "logger.hpp"  // IWYU pragma: keep
#include "plugin_core.h"

#include <SDL2/SDL_video.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_scancode.h>
#include <sstream>



#define SDL_LOG_FAIL(com) do { \
    const char* msg = SDL_GetError(); \
    std::ostringstream os; \
    os << "\033[91mWM: SDL2 fatal error at " << __FILE__ << ":" << __LINE__ << "\n" << msg << "\n" << com << "\033[0m\n"; \
    throw Exception(ErrCode::InternalError, os.str()); \
} while(0)



WindowManager::WindowManager(GraphicsBackend backend, Logger& logger)
    : mrLogger(logger)
{

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) SDL_LOG_FAIL("Failed to initialze SDL subsystem VIDEO");
    
    switch (backend) {

        case GraphicsBackend::None:
            mrLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager without a graphics backend\n";
            break;

        case GraphicsBackend::Unknown:
            mrLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager without a known graphics backend\n";
            break;

        case GraphicsBackend::Vulkan:
            mWindowFlags |= SDL_WINDOW_VULKAN;
            mrLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager with Vulkan\n";
            break;
    }

}


void WindowManager::update() {

    for (auto& window : mWindows) {
        window.resized = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        size_t winIndex = mWindowIDToWindow[event.window.windowID];

        switch (event.type) {

            case SDL_QUIT:
                mLost = true;
                break;

            case SDL_KEYDOWN:
                switch (event.key.keysym.scancode) {
                    case SDL_SCANCODE_ESCAPE:
                        closeWindow(mWindows[winIndex].handle);

                    default: break;
                }
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        mWindows[winIndex].resized = true;
                        break;

                    case SDL_WINDOWEVENT_CLOSE:
                        closeWindow(mWindows[winIndex].handle);
                        break;

                    default: break;
                }

            default: break;

        }
    }

    if (mActualWindowCount == 0) mLost = true;

}


WindowManager::~WindowManager() noexcept {

    for (auto& window : mWindows) {
        SDL_DestroyWindow(window.window);
    }

    SDL_Quit();

}


std::vector<const char*> WindowManager::getExtensionsVk(TprWindow handle) const {
    if ((mWindowFlags & SDL_WINDOW_VULKAN) == 0) throw Exception(ErrCode::NoSupportError, logPrxWinM() + "Was not initialized with Vulkan support");
    if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) throw Exception(ErrCode::WrongValueError, logPrxWinM() + "Wrong handle generation");
    uint32_t count;
    if (!SDL_Vulkan_GetInstanceExtensions(mWindows[get_basic_handle_index(handle)].window, &count, nullptr))
        throw Exception(ErrCode::InternalError, logPrxWinM() + "Failed to get vulkan instance extensions");
    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(mWindows[get_basic_handle_index(handle)].window, &count, extensions.data()))
        throw Exception(ErrCode::InternalError, logPrxWinM() + "Failed to get vulkan instance extensions");
    return std::move(extensions);
}


VkSurfaceKHR WindowManager::createSurfaceVk(TprWindow handle, VkInstance instance) const {
    if ((mWindowFlags & SDL_WINDOW_VULKAN) == 0) throw Exception(ErrCode::NoSupportError, logPrxWinM() + "Was not initialized with Vulkan support");
    if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) throw Exception(ErrCode::WrongValueError, logPrxWinM() + "Wrong handle generation");
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(mWindows[get_basic_handle_index(handle)].window, instance, &surface)) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << SDL_GetError() << "\n";
        throw Exception(ErrCode::InternalError, "Failed to create vulkan surface");
    }
    return surface;
}


bool WindowManager::lost() const {
    return mLost;
}


std::vector<TprWindow> WindowManager::getWindows() {
    std::vector<TprWindow> handles;
    for (auto& window : mWindows) {
        if (window.actual) {
            handles.push_back(window.handle);
        }
    }
    return handles;
}




TprResult WindowManager::openWindow(TprWindow* pHandle, const TprWindowCreateInfo* pCreateInfo) noexcept {

    try {

        Uint32 flags = mWindowFlags;
        if (pCreateInfo->flags & TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        if (!(pCreateInfo->flags & TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT)) {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        SDL_Window* pWindow = SDL_CreateWindow(
            pCreateInfo->name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            pCreateInfo->prefferedWidth, pCreateInfo->prefferedHeight, flags
        );

        if (!pWindow) return TPR_UNKNOWN_ERROR;

        size_t index;
        if (!mFreeWindows.empty()) {
            index = mFreeWindows.back();
            mFreeWindows.pop_back();
        } else {
            index = mWindows.size();
            mWindows.emplace_back();
        }

        Window& window = mWindows[index];

        window.window = pWindow;
        window.actual = true;
        window.generation++;
        window.id = SDL_GetWindowID(pWindow);
        window.flags = flags;

        set_basic_handle_index(pHandle, index);
        set_basic_handle_generation(pHandle, window.generation);
        set_basic_handle_type(pHandle, handle_type::window);

        window.handle = *pHandle;

        mActualWindowCount++;

    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


void WindowManager::closeWindow(TprWindow handle) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return;
        if (get_basic_handle_index(handle) >= mWindows.size()) return;
        if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) return;

        mrLogger.debug() << logPrxWinM() + "Closed window\n";

        SDL_DestroyWindow(mWindows[get_basic_handle_index(handle)].window);
        mWindows[get_basic_handle_index(handle)].actual = false;
        mFreeWindows.push_back(get_basic_handle_index(handle));
        mActualWindowCount--;

    } catch (...) {}
}


TprResult WindowManager::getWindowWidth(TprWindow handle, int32_t* width) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return TPR_INVALID_VALUE;
        if (get_basic_handle_index(handle) >= mWindows.size()) return TPR_INVALID_VALUE;
        if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) return TPR_INVALID_VALUE;
        int w, h;
        SDL_GetWindowSize(mWindows[get_basic_handle_index(handle)].window, &w, &h);
        *width = static_cast<int32_t>(w);
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult WindowManager::getWindowHeight(TprWindow handle, int32_t* height) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return TPR_INVALID_VALUE;
        if (get_basic_handle_index(handle) >= mWindows.size()) return TPR_INVALID_VALUE;
        if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) return TPR_INVALID_VALUE;
        int w, h;
        SDL_GetWindowSize(mWindows[get_basic_handle_index(handle)].window, &w, &h);
        *height = static_cast<int32_t>(h);
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult WindowManager::hasWindowResized(TprWindow handle, TprBool8* pValue) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return TPR_INVALID_VALUE;
        if (get_basic_handle_index(handle) >= mWindows.size()) return TPR_INVALID_VALUE;
        if (mWindows[get_basic_handle_index(handle)].generation != get_basic_handle_generation(handle)) return TPR_INVALID_VALUE;
        *pValue = mWindows[get_basic_handle_index(handle)].resized;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


