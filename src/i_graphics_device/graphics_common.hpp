
#ifndef I_GRAPHICS_DEVICE_GRAPHICS_COMMON_HPP_
#define I_GRAPHICS_DEVICE_GRAPHICS_COMMON_HPP_

#include <compare>

#include <glm/glm.hpp>
#include <ostream>


enum class GraphicsAPI {
    None = 0,
    Unknown = 1,
    Vulkan = 2
};

constexpr const char* kGraphicsBackendName[] = {
    "None",
    "Unknown",
    "Vulkan"
};


class SDL_Window;

struct WindowIdentity {
    private:
        SDL_Window* ptr;
        WindowIdentity(SDL_Window* ptr) noexcept : ptr(ptr) {}
        friend class Windowing;
        friend class std::hash<WindowIdentity>;

    public:
        WindowIdentity() noexcept : ptr(nullptr) {}

        std::strong_ordering operator<=>(const WindowIdentity& other) const noexcept {
            return ptr <=> other.ptr;
        }
        bool operator==(const WindowIdentity& other) const noexcept = default;

        friend std::ostream& operator<<(std::ostream& stream, const WindowIdentity& id) {
            stream << id.ptr;
            return stream;
        }
};

template <>
class std::hash<WindowIdentity> {
    public:
        size_t operator()(const WindowIdentity& id) const noexcept {
            return std::hash<SDL_Window*>{}(id.ptr);
        }
};


#endif  // I_GRAPHICS_DEVICE_GRAPHICS_COMMON_HPP_
