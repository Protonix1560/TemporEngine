

#ifndef IRENDERER_COMMON_TYPES_HPP_
#define IRENDERER_COMMON_TYPES_HPP_



#include "common_types.hpp"
#include "plugin_core.h"

#include <glm/glm.hpp>



class IRenderer {

    public:

        virtual void init() = 0;
        virtual void shutdown() noexcept = 0;
        virtual void update() = 0;

        virtual GraphicsBackend getGraphicsBackend() const = 0;
        virtual uint32_t getFrameWidth(TprWindow handle) const = 0;
        virtual uint32_t getFrameHeight(TprWindow handle) const = 0;
        
        virtual TprResult registerWindow(TprWindow handle) noexcept = 0;
        virtual void unregisterWindow(TprWindow handle) noexcept = 0;
        
        // virtual void beginRenderPass(const Scissor* pScissor = nullptr, const Viewport* pViewport = nullptr) = 0;
        // virtual void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices, CameraProject cameraProject = CameraProject::Ortho) = 0;
        // virtual void renderGUI(const GUIDrawDesc& desc) = 0;
        // virtual void endRenderPass() = 0;
        // virtual void nextSubpass() = 0;

        virtual void render(const RenderGraph& renderGraph) = 0;

        virtual ~IRenderer() = default;

};




#endif  // IRENDERER_COMMON_TYPES_HPP_

