

#ifndef IRENDERER_RENDERER_HPP_
#define IRENDERER_RENDERER_HPP_



#include "plugin_core.h"
#include <glm/glm.hpp>



enum class GraphicsBackend {
    None = 0,
    Unknown = 1,
    Vulkan = 2
};



struct DebugLineVertex {
    glm::vec3 pos;
    uint32_t colour;
};



struct Scissor {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
};



struct Viewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};



enum class CameraProject {
    Ortho = 0,
    Perspect = 1
};



struct GUIDrawDesc {
    
    struct Rect {
        float x, y, w, h;
        uint32_t bgColour;
    };

    std::vector<Rect> rects;

};



struct RenderGraph {
    struct WindowConfig {
        Scissor scissor;
        Viewport viewport;
    };
    std::vector<std::pair<TprWindow, WindowConfig>> windows;
    std::vector<TprEntityDrawDesc> entites;
};



#endif  // IRENDERER_RENDERER_HPP_
