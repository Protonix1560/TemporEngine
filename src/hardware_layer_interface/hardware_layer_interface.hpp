

#ifndef HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
#define HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_



#include "core.hpp"
#include "hardware_common_structs.hpp"
#include "plugin_core.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>


// from "window_manager.hpp"
class WindowManager;

// from "resource_registry.hpp"
class ResourceRegistry;

// from "logger.hpp"
class Logger;



class HardwareLayer {

    public:

        virtual void init(WindowManager* pWinMan) = 0;

        virtual ~HardwareLayer() noexcept = default;

        virtual void update() = 0;

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

};

REGISTER_TYPE_NAME_S(HardwareLayer, "HWLI");

template <>
struct type_name<std::unique_ptr<HardwareLayer>> {
    static constexpr comptime_string<sizeof("PHardwareLayer")> value{"PHardwareLayer"};
    static constexpr comptime_string<sizeof("PHWL")> value_short{"PHWL"};
};



struct HardwareLayerManifest {

    GraphicsBackend graphicsBackend;
    std::function<std::unique_ptr<HardwareLayer>(Logger& rLogger, ResourceRegistry& rResReg)> factory;
    std::string name;

};




#endif  // HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_

