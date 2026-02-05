

#ifndef HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
#define HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_



#include "hardware_common_structs.hpp"
#include "plugin_core.h"

#include <glm/glm.hpp>



class HardwareLayer {

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

        virtual ~HardwareLayer() = default;

};




#endif  // HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_

