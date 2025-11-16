

#ifndef RENDERER_RENDERER_HPP_
#define RENDERER_RENDERER_HPP_



#include <glm/glm.hpp>


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



#endif  // RENDERER_RENDERER_HPP_
