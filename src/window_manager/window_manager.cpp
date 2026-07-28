
#include "window_manager.hpp"
#include "core.hpp"
#include "plugin_core.h"

#include <SDL2/SDL_video.h>
#include <SDL_error.h>
#include <SDL_events.h>
#include <SDL_scancode.h>

#include <cmath>
#include <algorithm>

#include <vulkan/vulkan_core.h>



WindowManager::WindowManager(GraphicsBackend backend, Logger logger, std::atomic<int32_t>& rAliveTokens) : mLogger(logger), mrAliveTokens(rAliveTokens) {

    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to initialze SDL subsystem VIDEO" << "\n";
    }
    
    switch (backend) {
        case GraphicsBackend::None:
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager without a graphics backend\n";
            break;

        case GraphicsBackend::Unknown:
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager with an unknown graphics backend\n";
            break;

        case GraphicsBackend::Vulkan:
            mWindowFlags |= SDL_WINDOW_VULKAN;
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing window manager with Vulkan\n";
            break;
    }

    mKeyMap = {
        {SDL_SCANCODE_A, TPR_KEY_A},
        {SDL_SCANCODE_B, TPR_KEY_B},
        {SDL_SCANCODE_C, TPR_KEY_C},
        {SDL_SCANCODE_D, TPR_KEY_D},
        {SDL_SCANCODE_E, TPR_KEY_E},
        {SDL_SCANCODE_F, TPR_KEY_F},
        {SDL_SCANCODE_G, TPR_KEY_G},
        {SDL_SCANCODE_H, TPR_KEY_H},
        {SDL_SCANCODE_I, TPR_KEY_I},
        {SDL_SCANCODE_J, TPR_KEY_J},
        {SDL_SCANCODE_K, TPR_KEY_K},
        {SDL_SCANCODE_L, TPR_KEY_L},
        {SDL_SCANCODE_M, TPR_KEY_M},
        {SDL_SCANCODE_N, TPR_KEY_N},
        {SDL_SCANCODE_O, TPR_KEY_O},
        {SDL_SCANCODE_P, TPR_KEY_P},
        {SDL_SCANCODE_Q, TPR_KEY_Q},
        {SDL_SCANCODE_R, TPR_KEY_R},
        {SDL_SCANCODE_S, TPR_KEY_S},
        {SDL_SCANCODE_T, TPR_KEY_T},
        {SDL_SCANCODE_U, TPR_KEY_U},
        {SDL_SCANCODE_V, TPR_KEY_V},
        {SDL_SCANCODE_W, TPR_KEY_W},
        {SDL_SCANCODE_X, TPR_KEY_X},
        {SDL_SCANCODE_Y, TPR_KEY_Y},
        {SDL_SCANCODE_Z, TPR_KEY_Z},
        {SDL_SCANCODE_1, TPR_KEY_1},
        {SDL_SCANCODE_2, TPR_KEY_2},
        {SDL_SCANCODE_3, TPR_KEY_3},
        {SDL_SCANCODE_4, TPR_KEY_4},
        {SDL_SCANCODE_5, TPR_KEY_5},
        {SDL_SCANCODE_6, TPR_KEY_6},
        {SDL_SCANCODE_7, TPR_KEY_7},
        {SDL_SCANCODE_8, TPR_KEY_8},
        {SDL_SCANCODE_9, TPR_KEY_9},
        {SDL_SCANCODE_0, TPR_KEY_0},
        {SDL_SCANCODE_MINUS, TPR_KEY_MINUS},
        {SDL_SCANCODE_EQUALS, TPR_KEY_EQUAL},
        {SDL_SCANCODE_BACKSLASH, TPR_KEY_BACKSLASH},
        {SDL_SCANCODE_SLASH, TPR_KEY_SLASH},
        {SDL_SCANCODE_LEFTBRACKET, TPR_KEY_BRACKET_LEFT},
        {SDL_SCANCODE_RIGHTBRACKET, TPR_KEY_BRACKET_RIGHT},
        {SDL_SCANCODE_SEMICOLON, TPR_KEY_SEMICOLON},
        {SDL_SCANCODE_APOSTROPHE, TPR_KEY_QUOTE},
        {SDL_SCANCODE_SPACE, TPR_KEY_SPACE},
        {SDL_SCANCODE_TAB, TPR_KEY_TAB},
        {SDL_SCANCODE_CAPSLOCK, TPR_KEY_CAPS_LOCK},
        {SDL_SCANCODE_GRAVE, TPR_KEY_TILDE},
        {SDL_SCANCODE_ESCAPE, TPR_KEY_ESCAPE},
        {SDL_SCANCODE_RETURN, TPR_KEY_ENTER},
        {SDL_SCANCODE_BACKSPACE, TPR_KEY_BACKSPACE},
        {SDL_SCANCODE_LCTRL, TPR_KEY_LEFT_CTRL},
        {SDL_SCANCODE_LSHIFT, TPR_KEY_LEFT_SHIFT},
        {SDL_SCANCODE_LALT, TPR_KEY_LEFT_ALT},
        {SDL_SCANCODE_LGUI, TPR_KEY_LEFT_SUPER},
        {SDL_SCANCODE_RCTRL, TPR_KEY_RIGHT_CTRL},
        {SDL_SCANCODE_RSHIFT, TPR_KEY_RIGHT_SHIFT},
        {SDL_SCANCODE_RALT, TPR_KEY_RIGHT_ALT},
        {SDL_SCANCODE_RGUI, TPR_KEY_RIGHT_SUPER},
        {SDL_SCANCODE_F1, TPR_KEY_F1},
        {SDL_SCANCODE_F2, TPR_KEY_F2},
        {SDL_SCANCODE_F3, TPR_KEY_F3},
        {SDL_SCANCODE_F4, TPR_KEY_F4},
        {SDL_SCANCODE_F5, TPR_KEY_F5},
        {SDL_SCANCODE_F6, TPR_KEY_F6},
        {SDL_SCANCODE_F7, TPR_KEY_F7},
        {SDL_SCANCODE_F8, TPR_KEY_F8},
        {SDL_SCANCODE_F9, TPR_KEY_F9},
        {SDL_SCANCODE_F10, TPR_KEY_F10},
        {SDL_SCANCODE_F11, TPR_KEY_F11},
        {SDL_SCANCODE_F12, TPR_KEY_F12},
        {SDL_SCANCODE_PRINTSCREEN, TPR_KEY_PRINT_SCREEN},
        {SDL_SCANCODE_SCROLLLOCK, TPR_KEY_SCROLL_LOCK},
        {SDL_SCANCODE_PAUSE, TPR_KEY_PAUSE_BREAK},
        {SDL_SCANCODE_INSERT, TPR_KEY_INSERT},
        {SDL_SCANCODE_DELETE, TPR_KEY_DELETE},
        {SDL_SCANCODE_HOME, TPR_KEY_HOME},
        {SDL_SCANCODE_END, TPR_KEY_END},
        {SDL_SCANCODE_PAGEUP, TPR_KEY_PAGE_UP},
        {SDL_SCANCODE_PAGEDOWN, TPR_KEY_PAGE_DOWN},
        {SDL_SCANCODE_UP, TPR_KEY_ARROW_UP},
        {SDL_SCANCODE_DOWN, TPR_KEY_ARROW_DOWN},
        {SDL_SCANCODE_LEFT, TPR_KEY_ARROW_LEFT},
        {SDL_SCANCODE_RIGHT, TPR_KEY_ARROW_RIGHT},
        {SDL_SCANCODE_NUMLOCKCLEAR, TPR_KEY_NUM_LOCK},
        {SDL_SCANCODE_KP_DIVIDE, TPR_KEY_NUMPAD_SLASH},
        {SDL_SCANCODE_KP_MULTIPLY, TPR_KEY_NUMPAD_STAR},
        {SDL_SCANCODE_KP_MINUS, TPR_KEY_NUMPAD_MINUS},
        {SDL_SCANCODE_KP_PLUS, TPR_KEY_NUMPAD_PLUS},
        {SDL_SCANCODE_KP_EQUALS, TPR_KEY_NUMPAD_EQUAL},
        {SDL_SCANCODE_KP_ENTER, TPR_KEY_NUMPAD_ENTER},
        {SDL_SCANCODE_KP_PERIOD, TPR_KEY_NUMPAD_DOT},
        {SDL_SCANCODE_KP_0, TPR_KEY_NUMPAD_0},
        {SDL_SCANCODE_KP_1, TPR_KEY_NUMPAD_1},
        {SDL_SCANCODE_KP_2, TPR_KEY_NUMPAD_2},
        {SDL_SCANCODE_KP_3, TPR_KEY_NUMPAD_3},
        {SDL_SCANCODE_KP_4, TPR_KEY_NUMPAD_4},
        {SDL_SCANCODE_KP_5, TPR_KEY_NUMPAD_5},
        {SDL_SCANCODE_KP_6, TPR_KEY_NUMPAD_6},
        {SDL_SCANCODE_KP_7, TPR_KEY_NUMPAD_7},
        {SDL_SCANCODE_KP_8, TPR_KEY_NUMPAD_8},
        {SDL_SCANCODE_KP_9, TPR_KEY_NUMPAD_9},
        {SDL_SCANCODE_COMMA, TPR_KEY_COMMA},
        {SDL_SCANCODE_PERIOD, TPR_KEY_DOT}
    };

    mMouseButtonMap = {
        {SDL_BUTTON_LEFT, TPR_MOUSE_BUTTON1},
        {SDL_BUTTON_MIDDLE, TPR_MOUSE_BUTTON2},
        {SDL_BUTTON_RIGHT, TPR_MOUSE_BUTTON3},
        {SDL_BUTTON_X1, TPR_MOUSE_BUTTON4},
        {SDL_BUTTON_X2, TPR_MOUSE_BUTTON5}
    };

}


WindowManager::~WindowManager() noexcept {
    for (auto& [index, window] : mWindows) {
        destroyWindow(window);
    }
    mWindows.clear();
    SDL_Quit();
}


expected<TprWindow, TprResult> WindowManager::openWindow(const TprWindowCreateInfo* pCreateInfo) noexcept {

    if (!pCreateInfo) return unexpected(TPR_ERROR_INVALID_VALUE);

    uint32_t index = mWindowCounter++;
    Window window{};
    window.index = index;

    try {

        Uint32 flags = mWindowFlags;
        if (pCreateInfo->flags & TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        if (!(pCreateInfo->flags & TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT)) {
            flags |= SDL_WINDOW_RESIZABLE;
        }

        window.window = SDL_CreateWindow(
            pCreateInfo->name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
            pCreateInfo->prefferedWidth, pCreateInfo->prefferedHeight, flags
        );

        if (!window.window) {
            const char* msg = SDL_GetError();
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "SDL_CreateWindow failed:\n" << msg << "\n";
            return unexpected(TPR_UNKNOWN_ERROR);
        }

        window.id = SDL_GetWindowID(window.window);
        if (window.id == 0) {
            const char* msg = SDL_GetError();
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "SDL_GetWindowID failed:\n" << msg << "\n";
            return unexpected(TPR_UNKNOWN_ERROR);
        }

        window.handle = construct_basic_handle<TprWindow>(index, 0, handle_type::window);


    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to create window\n";
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    auto it = mWindows.emplace(index, window).first;

    mrAliveTokens++;

    mLogger.trace() << "Opened window " << index << "\n";

    return window.handle;
}


void WindowManager::closeWindow(TprWindow handle) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return;
        auto it = mWindows.find(get_basic_handle_index(handle));
        if (it == mWindows.end()) return;
        destroyWindow(it->second);
        mWindows.erase(it);
    } catch (...) {}
}


void WindowManager::destroyWindow(Window& window) noexcept {
    SDL_DestroyWindow(window.window);
    for (auto& [index, action] : window.actions) {
        mActionMap.erase(index);
    }
    mLogger.trace() << "Closed window " << window.index << "\n";
    mrAliveTokens--;
}


void WindowManager::updateWindowActions(Window& window, TprInputElement element, const TprInputElementVector& vector) {
    for (auto& [actionIndex, action] : window.actions) {
        if (action.element == element) {
            float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
            if (action.state == TPR_TRUE && length < action.lowThreshold) {
                action.state = TPR_FALSE;
                action.frames = 0;
            } else if (action.state == TPR_FALSE && length > action.highThreshold) {
                action.state = TPR_TRUE;
                action.frames = 0;
            }
        }
    }
}


void WindowManager::update() {

    for (auto& [windowIndex, window] : mWindows) {
        window.resized = false;
        for (auto& [actionIndex, action] : window.actions) {
            switch (action.element) {

                case TPR_MOUSE_MOTION:
                case TPR_MOUSE_WHEEL_DOWN:
                case TPR_MOUSE_WHEEL_UP: {
                    // setting those events to null
                    // needed because SDL2 doesn't send event line KEYUP, so WindowMananger has no idea when a sequence of those events ends
                    TprInputElementVector& vector = window.elements[action.element];
                    vector = {0.0f, 0.0f, 0.0f};
                    updateWindowActions(window, action.element, vector);
                    break;
                }

                default: 
                    action.frames++;
                    break;
            }
        }
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        // NOTICE:
        // SDL2 doesn't have a singular windowID field in SDL_Event
        // and for every different type of event there's it's own union member
        // so for example in event.type == SDL_KEYDOWN the windowID is in event.key.windowID
        // but for event.type == SDL_WINDOWEVENT the windowID is in event.window.windowID
        Uint32* pWindowId = nullptr;
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                pWindowId = &event.key.windowID;
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                pWindowId = &event.button.windowID;
                break;
            case SDL_MOUSEMOTION:
                pWindowId = &event.motion.windowID;
                break;
            case SDL_WINDOWEVENT:
                pWindowId = &event.window.windowID;
                break;
            case SDL_MOUSEWHEEL:
                pWindowId = &event.wheel.windowID;
                break;
        }
        Window* window = &mSentinelWindow;
        auto windowIt = mWindows.end();
        if (pWindowId) {
            windowIt = std::find_if(mWindows.begin(), mWindows.end(), [pWindowId](const auto& pair) {
                return pair.second.id == *pWindowId;
            });
            if (windowIt != mWindows.end()) {
                window = &windowIt->second;
            }
        }

        switch (event.type) {

            case SDL_QUIT:
                mLogger.debug() << "Got SDL_QUIT\n";
                for (auto& [index, window] : mWindows) {
                    destroyWindow(window);
                }
                mWindows.clear();
                break;

            case SDL_KEYDOWN: {
                TprInputElement element = mKeyMap.at(event.key.keysym.scancode);
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {0, 0, 1};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_KEYUP: {
                TprInputElement element = mKeyMap.at(event.key.keysym.scancode);
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {0, 0, 0};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_MOUSEBUTTONDOWN: {
                TprInputElement element = mMouseButtonMap.at(event.button.button);
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {0, 0, 1};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_MOUSEBUTTONUP: {
                TprInputElement element = mMouseButtonMap.at(event.button.button);
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {0, 0, 0};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_MOUSEWHEEL: {
                TprInputElement element = event.wheel.y > 0 ? TPR_MOUSE_WHEEL_UP : TPR_MOUSE_WHEEL_DOWN;
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {static_cast<float>(event.wheel.x), static_cast<float>(event.wheel.y), 0};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_MOUSEMOTION: {
                TprInputElement element = TPR_MOUSE_MOTION;
                TprInputElementVector& elementVector = window->elements[element];
                elementVector = {static_cast<float>(event.motion.xrel), static_cast<float>(event.motion.yrel), 0};
                updateWindowActions(*window, element, elementVector);
                break;
            }

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        window->resized = true;
                        break;

                    case SDL_WINDOWEVENT_CLOSE:
                        if (windowIt != mWindows.end() && window != &mSentinelWindow) {
                            destroyWindow(*window);
                            mWindows.erase(windowIt);
                        }
                        break;

                    default: break;
                }

            default: break;

        }
    }

}


expected<std::vector<const char*>, TprResult> WindowManager::getExtensionsVk(TprWindow handle) const {
    if ((mWindowFlags & SDL_WINDOW_VULKAN) == 0) return unexpected(TPR_NOT_SUPPORTED);
    auto it = mWindows.find(get_basic_handle_index(handle));
    if (it == mWindows.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    const Window& window = it->second;
    uint32_t count;
    if (!SDL_Vulkan_GetInstanceExtensions(window.window, &count, nullptr)) return unexpected(TPR_UNKNOWN_ERROR);
    std::vector<const char*> extensions(count);
    if (!SDL_Vulkan_GetInstanceExtensions(window.window, &count, extensions.data())) return unexpected(TPR_UNKNOWN_ERROR);
    return extensions;
}


expected<VkSurfaceKHR, TprResult> WindowManager::createSurfaceVk(TprWindow handle, VkInstance instance) const {
    if ((mWindowFlags & SDL_WINDOW_VULKAN) == 0) return unexpected(TPR_NOT_SUPPORTED);
    auto it = mWindows.find(get_basic_handle_index(handle));
    if (it == mWindows.end()) return unexpected(TPR_UNKNOWN_ERROR);;
    const Window& window = it->second;
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window.window, instance, &surface)) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << SDL_GetError() << "\n";
        return unexpected(TPR_UNKNOWN_ERROR);;
    }
    return surface;
}


std::vector<TprWindow> WindowManager::getWindows() {
    std::vector<TprWindow> handles;
    for (auto& [index, window] : mWindows) {
        handles.push_back(window.handle);
    }
    return handles;
}


expected<int32_t, TprResult> WindowManager::getWindowWidth(TprWindow handle) noexcept {
    int32_t width;
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mWindows.find(get_basic_handle_index(handle));
        if (it == mWindows.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        Window& window = it->second;
        int w, h;
        SDL_GetWindowSize(window.window, &w, &h);
        width = static_cast<int32_t>(w);
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
    return width;
}


expected<int32_t, TprResult> WindowManager::getWindowHeight(TprWindow handle) noexcept {
    int32_t height;
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mWindows.find(get_basic_handle_index(handle));
        if (it == mWindows.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        Window& window = it->second;
        int w, h;
        SDL_GetWindowSize(window.window, &w, &h);
        height = static_cast<int32_t>(h);
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
    return height;
}


expected<TprBool8, TprResult> WindowManager::hasWindowResized(TprWindow handle) noexcept {
    TprBool8 value;
    try {
        if (get_basic_handle_type(handle) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto it = mWindows.find(get_basic_handle_index(handle));
        if (it == mWindows.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        Window& window = it->second;
        value = window.resized;
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
    return value;
}


expected<TprAction, TprResult> WindowManager::createAction(TprWindow windowHandle, const TprActionCreateInfo* pCreateInfo) noexcept {

    if (!pCreateInfo) return unexpected(TPR_ERROR_INVALID_VALUE);

    if (get_basic_handle_type(windowHandle) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
    auto it = mWindows.find(get_basic_handle_index(windowHandle));
    if (it == mWindows.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    Window& window = it->second;

    uint32_t index = mActionCounter++;
    Action action{};
    TprAction actionHandle;

    action.element = pCreateInfo->element;
    action.highThreshold = pCreateInfo->highThreshold;
    action.lowThreshold = pCreateInfo->lowThreshold;
    action.windowIndex = window.index;

    actionHandle = construct_basic_handle<TprAction>(index, 0, handle_type::action);

    window.actions.emplace(index, action);
    mActionMap.emplace(index, get_basic_handle_index(windowHandle));
    
    return actionHandle;
}


void WindowManager::destroyAction(TprAction handle) noexcept {
    if (get_basic_handle_type(handle) != handle_type::action) return;
    auto it = mActionMap.find(get_basic_handle_index(handle));
    if (it == mActionMap.end()) return;
    mWindows.at(it->second).actions.erase(get_basic_handle_index(handle));
    mActionMap.erase(it);
}


TprResult WindowManager::getActionState(TprAction handle, TprActionState* pState) noexcept {
    if (!pState) return TPR_ERROR_INVALID_VALUE;
    if (get_basic_handle_type(handle) != handle_type::action) return TPR_ERROR_INVALID_VALUE;
    auto it = mActionMap.find(get_basic_handle_index(handle));
    if (it == mActionMap.end()) return TPR_ERROR_INVALID_VALUE;
    Window& window = mWindows.at(it->second);
    Action& action = window.actions.at(get_basic_handle_index(handle));
    pState->vector = window.elements[action.element];
    pState->framesActive = action.frames;
    pState->state = action.state;
    return TPR_SUCCESS;
}


TprResult WindowManager::getInputElementVector(TprWindow handle, TprInputElement inputElement, TprInputElementVector* pVector) noexcept {
    if (!pVector) return TPR_ERROR_INVALID_VALUE;
    if (get_basic_handle_type(handle) != handle_type::window) return TPR_ERROR_INVALID_VALUE;
    auto windowIt = mWindows.find(get_basic_handle_index(handle));
    if (windowIt == mWindows.end()) return TPR_ERROR_INVALID_VALUE;
    Window& window = windowIt->second;
    *pVector = window.elements[inputElement];
    return TPR_SUCCESS;
}


