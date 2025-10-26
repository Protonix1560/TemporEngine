#pragma once

#include "core.hpp"
#include "window_manager.hpp"

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



class Backend {

    public:

        virtual void init(const WindowManager* windowManager, const GlobalServiceLocator* serviceLocator) = 0;
        virtual void destroy() noexcept = 0;
        virtual void beginRenderPass(const Scissor* pScissor = nullptr, const Viewport* pViewport = nullptr) = 0;
        virtual void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices) = 0;
        virtual void endRenderPass() = 0;
        virtual void update() = 0;
        virtual ~Backend() = default;

};