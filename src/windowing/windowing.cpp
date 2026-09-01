
#include "windowing.hpp"
#include "core.hpp"
#include "graphics_common.hpp"
#include "i_graphics_device.hpp"
#include "plugin_core.h"
#include "scheduler.hpp"
#include "log_entry.hpp"
#include "scope_guard.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <cassert>
#include <exception>
#include <mutex>
#include <unordered_set>



Windowing::Windowing(Logger logger, Scheduler& rSched, std::atomic<TprResult>& rRunResult)
    : mLogger(logger), mrSched(rSched), mrRunResult(rRunResult),
    mKeyMap({
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
    }),
    mMouseButtonMap({
        {SDL_BUTTON_LEFT, TPR_MOUSE_BUTTON1},
        {SDL_BUTTON_MIDDLE, TPR_MOUSE_BUTTON2},
        {SDL_BUTTON_RIGHT, TPR_MOUSE_BUTTON3},
        {SDL_BUTTON_X1, TPR_MOUSE_BUTTON4},
        {SDL_BUTTON_X2, TPR_MOUSE_BUTTON5}
    }) {}

TprResult Windowing::init(IGraphicsDevice* pIGD, GraphicsAPI graphics) {
    assert(threadInfo.mainThread);

    std::lock_guard<std::mutex> lock(mMutex);
    assert(!mInitialised);

    mpGDev = pIGD;
    mGraphics = graphics;

    // SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        mLogger.error() << "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: " << SDL_GetError();
        return TPR_ERROR_NOT_LOADED;
    }
    
    switch (mGraphics) {
        case GraphicsAPI::None:
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing Windowing without a graphics backend";
            break;

        case GraphicsAPI::Unknown:
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing Windowing with an unknown graphics backend";
            break;

        case GraphicsAPI::Vulkan: {
            mLogger.debug(TPR_LOG_STYLE_TIMESTAMP1) << "Initializing Windowing with Vulkan";

            if (!SDL_Vulkan_LoadLibrary(nullptr)) {
                mLogger.error() << "SDL_Vulkan_LoadLibrary(nullptr) failed: " << SDL_GetError();
                return TPR_ERROR_NOT_LOADED;
            }

            Uint32 extensionCount;
            auto p = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
            if (!p) {
                mLogger.error() << "SDL_Vulkan_GetInstanceExtensions failed: " << SDL_GetError();
                return TPR_ERROR_NOT_LOADED;
            }
            mVkInstanceExtensions.resize(extensionCount);
            memcpy(mVkInstanceExtensions.data(), p, extensionCount * sizeof(const char*));
            {
                auto l = mLogger.trace();
                l << "Required Vulkan instance extensions:\n";
                for (const auto& ext : mVkInstanceExtensions) {
                    l << "  - " << ext;
                    if (ext != mVkInstanceExtensions.back()) l << ";\n";
                }
            }

            break;
        }
    }

    {
        TprJobCreateInfo info{};
        info.context = this;
        info.duration = TPR_JOB_DURATION_SHORT;
        info.function = [](void* ctx, TprJob job) noexcept {
            reinterpret_cast<Windowing*>(ctx)->processEvents();
        };
        info.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
        auto exp = mrSched.createJob(info);
        if (!exp.has_value()) return exp.error();
        mProcessEventsJob = exp.value();
    }

    // mesuring the offset between the Scheduler's time beginning and SDL's time beginning
    {
        // this all is probably an overkill :)
        constexpr size_t testsCount = 10;
        uint64_t minD = UINT64_MAX;
        for (size_t i = 0; i < testsCount; i++) {
            uint64_t before = mrSched.now();
            Uint64 measure = SDL_GetTicksNS();
            uint64_t after = mrSched.now();
            uint64_t d = after - before;
            if (d < minD) {
                minD = d;
                mTimeBeginOffset = before + d / 2 - measure;
            }
        }
    }

    mInitialised = true;

    return TPR_SUCCESS;
}

Windowing::~Windowing() noexcept {
    assert(threadInfo.mainThread);
    std::lock_guard<std::mutex> lock(mMutex);
    if (mInitialised) {
        for (auto& [window, entry] : mWindowEntryMap) {
            SDL_DestroyWindow(window);
        }
        switch (mGraphics) {
            case GraphicsAPI::Vulkan: {
                SDL_Vulkan_UnloadLibrary();
                break;
            }
            default: break;
        }
        SDL_Quit();
    }
}



TprResult Windowing::update() {
    assert(threadInfo.mainThread);

    SDL_PumpEvents();
    if (auto r = mrSched.scheduleJob(mProcessEventsJob, mrSched.now()); r != TPR_SUCCESS) return r;

    {
        std::unique_lock<std::mutex> lock(mMutex, std::defer_lock);
        if (lock.try_lock()) {
            for (auto query : mMainThreadQueries) {
                auto r = std::visit(overload{
                    [&](CreateWindowQuery& q) {
                        auto* window = SDL_CreateWindow(q.title, q.w, q.h, q.flags);
                        q.window.emplace(window);
                        if (!window) {
                            mLogger.panic() << "SDL_CreateWindow failed: " << SDL_GetError();
                            mrRunResult.store(TPR_PANIC);
                            return TPR_PANIC;
                        }
                        return TPR_SUCCESS;
                    },
                    [&](DestroyWindowQuery& q) {
                        SDL_DestroyWindow(q.window);
                        return TPR_SUCCESS;
                    },
                    [&](GetWindowWidthQuery& q) {
                        int w;
                        bool r = SDL_GetWindowSizeInPixels(q.window, &w, nullptr);
                        q.w.emplace(static_cast<uint32_t>(w));
                        if (!r) {
                            mLogger.panic() << "SDL_GetWindowSizeInPixel failed: " << SDL_GetError();
                            mrRunResult.store(TPR_PANIC);
                            return TPR_PANIC;
                        }
                        return TPR_SUCCESS;
                    },
                    [&](GetWindowHeightQuery& q) {
                        int h;
                        bool r = SDL_GetWindowSizeInPixels(q.window, nullptr, &h);
                        q.h.emplace(static_cast<uint32_t>(h));
                        if (!r) {
                            mLogger.panic() << "SDL_GetWindowSizeInPixels failed: " << SDL_GetError();
                            mrRunResult.store(TPR_PANIC);
                            return TPR_PANIC;
                        }
                        return TPR_SUCCESS;
                    }
                }, query);
                if (r != TPR_SUCCESS) return r;
            }
            mMainThreadQueries.clear();
        }
    }
    mCv.notify_all();

    return TPR_SUCCESS;
}

void Windowing::eventLoopEnded() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEventLoopInOrder = false;
    }
    mCv.notify_all();
}

void Windowing::processEvents() {
    std::lock_guard<std::mutex> lock(mMutex);

    int eventCount = SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
    if (eventCount == -1) return;
    std::vector<SDL_Event> events(eventCount);
    SDL_PeepEvents(events.data(), events.size(), SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);

    for (auto& [window, entry] : mWindowEntryMap) {
        for (auto action : entry->actions) {
            action->history.clear();
        }
    }

    for (const auto& event : events) {

        auto windowIt = mWindowEntryMap.find(SDL_GetWindowFromEvent(&event));
        if (windowIt == mWindowEntryMap.end()) continue;  // mustn't happen
        std::shared_ptr<WindowEntry> window = windowIt->second;

        TprInputDevice device;
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                auto it = mKeyMap.find(event.key.scancode);
                if (it == mKeyMap.end()) continue;
                device = it->second;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                auto it = mMouseButtonMap.find(event.button.button);
                if (it == mMouseButtonMap.end()) continue;
                device = it->second;
                break;
            }
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                device = TPR_WINDOW_SIZE;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                device = TPR_MOUSE_MOTION;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                device = TPR_MOUSE_WHEEL;
                break;
            default: continue;
        }

        for (auto action : window->actions) {
            if (action->device == device) {
                uint64_t timepoint = event.common.timestamp - mTimeBeginOffset;

                TprVec4 raw{};
                switch (event.type) {
                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                        raw = {1.0f, 0.0f, 0.0f, 0.0f};
                        break;
                    case SDL_EVENT_KEY_UP:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                        raw = {0.0f, 0.0f, 0.0f, 0.0f};
                        break;
                    case SDL_EVENT_MOUSE_WHEEL:
                        raw = {event.wheel.x, event.wheel.y, 0.0f, 0.0f};
                        break;
                    case SDL_EVENT_MOUSE_MOTION:
                        raw = {event.motion.x, event.motion.y, 0.0f, 0.0f};
                        break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        raw = {static_cast<float>(event.window.data1), static_cast<float>(event.window.data2), 0.0f, 0.0f};
                        break;
                }
                if (
                    raw.x == action->currAbsState.vector.x &&
                    raw.y == action->currAbsState.vector.y &&
                    raw.z == action->currAbsState.vector.z &&
                    raw.w == action->currAbsState.vector.w
                ) continue;

                TprVec4 measure{};
                switch (action->measureType) {
                    case TPR_MEASURE_TYPE_ABSOLUTE:
                        measure = raw;
                        break;

                    case TPR_MEASURE_TYPE_DIFFERENCE:
                        measure.x = raw.x - action->currAbsState.vector.x;
                        measure.y = raw.y - action->currAbsState.vector.y;
                        measure.z = raw.z - action->currAbsState.vector.z;
                        measure.w = raw.w - action->currAbsState.vector.w;
                        break;

                    case TPR_MEASURE_TYPE_DERIVATIVE:
                        measure.x = (raw.x - action->currAbsState.vector.x) / (static_cast<float>(timepoint - action->currAbsState.timepoint) * 1e-9);
                        measure.y = (raw.y - action->currAbsState.vector.y) / (static_cast<float>(timepoint - action->currAbsState.timepoint) * 1e-9);
                        measure.z = (raw.z - action->currAbsState.vector.z) / (static_cast<float>(timepoint - action->currAbsState.timepoint) * 1e-9);
                        measure.w = (raw.w - action->currAbsState.vector.w) / (static_cast<float>(timepoint - action->currAbsState.timepoint) * 1e-9);
                        break;

                    default: break;
                }

                action->history.push_back({measure, timepoint});
                action->currAbsState = {raw, timepoint};
            }

            if (!action->history.empty()) {
                action->currState = action->history.back();
            }
        }

    }
}



expected<TprWindow, TprResult> Windowing::openWindow(const TprWindowCreateInfo& info) noexcept {
    std::unique_lock<std::mutex> lock(mMutex);
    assert(mInitialised);
    if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
    try {
        Uint32 flags = 0;
        switch (mGraphics) {
            case GraphicsAPI::Vulkan: flags |= SDL_WINDOW_VULKAN;
            default: break;
        }
        if (info.flags & TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT) flags |= SDL_WINDOW_HIDDEN;
        if (!(info.flags & TPR_CREATE_WINDOW_UNRESIZEABLE_FLAG_BIT)) flags |= SDL_WINDOW_RESIZABLE;

        std::optional<SDL_Window*> windowOpt;
        mMainThreadQueries.push_back(CreateWindowQuery{
            windowOpt, info.name,
            static_cast<int>(info.width), static_cast<int>(info.height), flags
        });
        mCv.wait(lock, [&]() { return windowOpt.has_value(); });
        if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
        SDL_Window* window = windowOpt.value();
        if (!window) return unexpected(TPR_PANIC);

        {
            unlock_guard unlock(lock);
            if (auto r = mpGDev->registerWindow(WindowIdentity(window)); r != TPR_SUCCESS) {
                mMainThreadQueries.push_back(DestroyWindowQuery{window});
                return unexpected(r);
            }
        }
        
        auto handle = mWindowHandles.insert_or_assign(mWindowCounter, WindowHandle{std::make_shared<WindowEntry>(window)}).first->second;
        mWindowEntryMap.insert_or_assign(window, handle.entry);
        TprWindow h = construct_basic_handle<TprWindow>(mWindowCounter, 0, handle_type::window);
        mLogger.debug() << "Created Window " << mWindowCounter;
        mWindowCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprWindow, TprResult> Windowing::createWindowCapability(TprWindow window, TprWindowCapabilityFlags flags) noexcept {
    if (get_basic_handle_type(window) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mWindowHandles.find(get_basic_handle_index(window));
        if (it == mWindowHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mWindowHandles.insert_or_assign(mWindowCounter, WindowHandle{it->second.entry, it->second.capability & flags});
        TprWindow h = construct_basic_handle<TprWindow>(mWindowCounter, 0, handle_type::window);
        mLogger.debug() << "Created Window capability " << mWindowCounter << " for Window " << get_basic_handle_index(window);
        mWindowCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void Windowing::closeWindow(TprWindow window) noexcept {
    if (get_basic_handle_type(window) != handle_type::window) return;
    std::unique_lock<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mWindowHandles.find(get_basic_handle_index(window));
        if (it == mWindowHandles.end()) return;
        auto entry = it->second.entry;
        mWindowHandles.erase(it);
        if (entry.use_count() <= 2) {
            for (auto action : entry->actions) {
                for (auto handle : action->handles) {
                    mActions.erase(handle);
                }
            }
            mWindowEntryMap.erase(entry->window);
            mMainThreadQueries.push_back(DestroyWindowQuery{entry->window});
            mCv.notify_all();
            {
                unlock_guard unlock(lock);
                mpGDev->unregisterWindow(WindowIdentity(entry->window));
            }
        }
        mLogger.debug() << "Closed Window " << get_basic_handle_index(window);

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}



expected<TprAction, TprResult> Windowing::createAction(const TprActionCreateInfo& info) noexcept {
    if (get_basic_handle_type(info.window) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
    switch (info.measureType) {
        case TPR_MEASURE_TYPE_ABSOLUTE: case TPR_MEASURE_TYPE_DIFFERENCE: case TPR_MEASURE_TYPE_DERIVATIVE: break;
        default: return unexpected(TPR_ERROR_INVALID_VALUE);
    }
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mWindowHandles.find(get_basic_handle_index(info.window));
        if (it == mWindowHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto window = it->second.entry;
        auto& action = mActions.insert_or_assign(mActionCounter, ActionHandle{
            std::make_shared<ActionEntry>(*window.get(), info.device, info.measureType)
        }).first->second;
        action.entry->handles.push_back(mActionCounter);
        window->actions.emplace_back(action.entry);
        TprAction h = construct_basic_handle<TprAction>(mActionCounter, 0, handle_type::action);
        mLogger.debug() << "Created Action " << mActionCounter << " for Window " << get_basic_handle_index(info.window);
        mActionCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprAction, TprResult> Windowing::createActionCapability(TprAction action, TprActionCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(action) != handle_type::action) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mActions.find(get_basic_handle_index(action));
        if (it == mActions.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto handle = it->second;
        mActions.insert_or_assign(mActionCounter, ActionHandle{handle.entry, mask & handle.capability});
        handle.entry->handles.push_back(mActionCounter);
        TprAction h = construct_basic_handle<TprAction>(mActionCounter, 0, handle_type::action);
        mLogger.debug() << "Created Action capability " << mActionCounter << " for Action " << get_basic_handle_index(action);
        mActionCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void Windowing::destroyAction(TprAction action) noexcept {
    if (get_basic_handle_type(action) != handle_type::action) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mActions.find(get_basic_handle_index(action));
        if (it == mActions.end()) return;
        auto entry = it->second.entry;
        mActions.erase(it);
        if (entry.use_count() <= 2) {
            // uses must be here in the local variable 'entry' and in the window entry
            auto it = std::ranges::find(entry->window.actions, entry);
            if (it != entry->window.actions.end()) entry->window.actions.erase(it);
        }
        mLogger.debug() << "Destroyed Action " << get_basic_handle_index(action);

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}



expected<uint32_t, TprResult> Windowing::getActionsHistorySize(uint32_t filterCount, const TprAction* pFilters) noexcept {
    if (!pFilters) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (filterCount == 0) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        uint32_t size = 0;
        for (uint32_t i = 0; i < filterCount; i++) {
            TprAction filter = pFilters[i];
            if (get_basic_handle_type(filter) != handle_type::action) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto it = mActions.find(get_basic_handle_index(filter));
            if (it == mActions.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto entry = it->second.entry;
            size += entry->history.size();
        }
        return size;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

TprResult Windowing::copyActionsHistory(TprActionHistoryEntry* pEntries, uint32_t filterCount, const TprAction* pFilters) noexcept {
    if (!pFilters) return TPR_ERROR_INVALID_VALUE;
    if (filterCount == 0) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        uint32_t size = 0;
        std::unordered_map<uint32_t, std::pair<
            decltype(ActionEntry::history)::iterator,
            decltype(ActionEntry::history)::iterator
        >> iterators;
        std::unordered_set<std::shared_ptr<ActionEntry>> entries;
        for (uint32_t i = 0; i < filterCount; i++) {
            TprAction filter = pFilters[i];
            if (get_basic_handle_type(filter) != handle_type::action) return TPR_ERROR_INVALID_VALUE;
            auto it = mActions.find(get_basic_handle_index(filter));
            if (it == mActions.end()) return TPR_ERROR_INVALID_VALUE;
            auto entry = it->second.entry;
            if (entries.contains(entry)) continue;
            entries.insert(entry);
            if (iterators.contains(get_basic_handle_index(filter))) return TPR_ERROR_INVALID_VALUE;
            if (!entry->history.empty()) {
                iterators.try_emplace(get_basic_handle_index(filter), entry->history.begin(), entry->history.end());
            }
            size += entry->history.size();
        }
        if (size == 0) return TPR_SUCCESS;
        for (uint32_t i = 0; i < size; i++) {
            auto it = std::ranges::min_element(iterators, [](const auto& a, const auto& b) {
                return a.second.first->timepoint < b.second.first->timepoint;
            });
            pEntries[i] = {construct_basic_handle<TprAction>(it->first, 0, handle_type::action), *it->second.first};
            it->second.first++;
            if (it->second.first == it->second.second) {
                iterators.erase(it);
            }
        }
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

expected<TprActionState, TprResult> Windowing::getActionState(TprAction action) noexcept {
    if (get_basic_handle_type(action) != handle_type::action) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mActions.find(get_basic_handle_index(action));
        if (it == mActions.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        return it->second.entry->currState;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}


TprJob Windowing::getInputUpdateJob() noexcept {
    return mProcessEventsJob;  // TODO: create a new capability every time
}


expected<WindowIdentity, TprResult> Windowing::getWindowIdentity(TprWindow window) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (get_basic_handle_type(window) != handle_type::window) return unexpected(TPR_ERROR_INVALID_VALUE);
    auto it = mWindowHandles.find(get_basic_handle_index(window));
    if (it == mWindowHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    return WindowIdentity{it->second.entry->window};
}

expected<uint32_t, TprResult> Windowing::windowPixelWidth(WindowIdentity id) {
    if (!id.ptr) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::unique_lock<std::mutex> lock(mMutex);
    if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
    assert(mInitialised);
    std::optional<uint32_t> w;
    mMainThreadQueries.push_back(GetWindowWidthQuery{id.ptr, w});
    mCv.wait(lock, [&]() { return w.has_value(); });
    if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
    if (!w.has_value()) return unexpected(TPR_PANIC);
    return w.value();
}

expected<uint32_t, TprResult> Windowing::windowPixelHeight(WindowIdentity id) {
    if (!id.ptr) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::unique_lock<std::mutex> lock(mMutex);
    if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
    assert(mInitialised);
    std::optional<uint32_t> h;
    mMainThreadQueries.push_back(GetWindowHeightQuery{id.ptr, h});
    mCv.wait(lock, [&]() { return h.has_value(); });
    if (!mEventLoopInOrder) return unexpected(TPR_ERROR_NOT_LOADED);
    if (!h.has_value()) return unexpected(TPR_PANIC);
    return h.value();
}



expected<PFN_vkGetInstanceProcAddr, TprResult> Windowing::getVkGetInstanceProcAddr() {
    if (mGraphics != GraphicsAPI::Vulkan) return unexpected(TPR_ERROR_INVALID_OPERATION);
    auto ptr =  reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
    if (!ptr) {
        mLogger.panic() << "SDL_Vulkan_GetVkGetInstanceProcAddr failed: " << SDL_GetError();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
    return ptr;
}

expected<std::span<const char* const>, TprResult> Windowing::getVkInstanceExtensions() {
    if (mGraphics != GraphicsAPI::Vulkan) return unexpected(TPR_ERROR_INVALID_OPERATION);
    return std::span<const char* const>(mVkInstanceExtensions.begin(), mVkInstanceExtensions.size());
}

expected<VkSurfaceKHR, TprResult> Windowing::createVkSurfaceKHR(WindowIdentity id, VkInstance instance, const VkAllocationCallbacks* pAlloc) {
    if (mGraphics != GraphicsAPI::Vulkan) return unexpected(TPR_ERROR_INVALID_OPERATION);
    if (!id.ptr) return unexpected(TPR_ERROR_INVALID_VALUE);
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(reinterpret_cast<SDL_Window*>(id.ptr), instance, pAlloc, &surface)) {
        mLogger.panic() << "SDL_Vulkan_CreateSurface failed: " << SDL_GetError();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
    return surface;
}

void Windowing::destroyVkSurfaceKHR(WindowIdentity id, VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* pAlloc) {
    // WindowIdentity required for future-proofing
    if (mGraphics != GraphicsAPI::Vulkan) return;
    if (!id.ptr) return;
    SDL_Vulkan_DestroySurface(instance, surface, pAlloc);
}
