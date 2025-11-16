

#ifndef RENDERER_COMMON_TYPES_HPP_
#define RENDERER_COMMON_TYPES_HPP_



#include "core.hpp"
#include "common_types.hpp"
#include "window_manager.hpp"

#include <glm/glm.hpp>



class Renderer {

    public:

        virtual void init(const WindowManager* windowManager, const GlobalServiceLocator* serviceLocator) = 0;
        virtual void shutdown() noexcept = 0;
        virtual void beginRenderPass(const Scissor* pScissor = nullptr, const Viewport* pViewport = nullptr) = 0;
        virtual void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices, CameraProject cameraProject = CameraProject::Ortho) = 0;
        virtual void renderGUI(const GUIDrawDesc& desc) = 0;
        virtual void endRenderPass() = 0;
        virtual void nextSubpass() = 0;
        virtual void update() = 0;
        virtual ~Renderer() = default;

};




#endif  // RENDERER_COMMON_TYPES_HPP_

