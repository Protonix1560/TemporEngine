

#ifndef HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
#define HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_


#include "core.hpp"
#include "hardware_common_structs.hpp"
#include "plugin_core.h"
#include "asset_store/common.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>


// from "window_manager.hpp"
class WindowManager;

// from "file_registry.hpp"
class FileRegistry;

// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;

// from "scene_graph.hpp"
class SceneGraph;


class HardwareLayer {

    public:

        HardwareLayer() = default;
        virtual ~HardwareLayer() noexcept = default;

        virtual TprResult update() = 0;

        virtual uint32_t getFrameWidth(TprWindow handle) const = 0;
        virtual uint32_t getFrameHeight(TprWindow handle) const = 0;
        
        virtual TprResult registerWindow(TprWindow handle) noexcept = 0;
        virtual void unregisterWindow(TprWindow handle) noexcept = 0;

        virtual TprResult render() = 0;

        virtual TprResult loadMesh(const AssetMesh& mesh) noexcept = 0;
        virtual void unloadMesh(TprMesh mesh) noexcept = 0;

        virtual expected<TprDepthDomain, TprResult> createDepthDomain(const TprDepthDomainCreateInfo* pInfo) noexcept = 0;
        virtual void destroyDepthDomain(TprDepthDomain domain) noexcept = 0;

        virtual expected<TprRenderTarget, TprResult> createRenderTarget(const TprRenderTargetCreateInfo* pInfo) noexcept = 0;
        virtual void destroyRenderTarget(TprRenderTarget target) noexcept = 0;

        virtual expected<TprObjectImage, TprResult> createObjectImage(const TprObjectImageCreateInfo* pInfo) noexcept = 0;
        virtual void destroyObjectImage(TprObjectImage image) noexcept = 0;

};

using PHardwareLayer = std::unique_ptr<HardwareLayer>;

REGISTER_TYPE_NAME_S(HardwareLayer, "HWLI");
REGISTER_TYPE_NAME_S(PHardwareLayer, "PHWL");


struct HardwareLayerManifest {

    GraphicsBackend graphicsBackend;
    std::function<expected<PHardwareLayer, TprResult>(
        Logger logger, FileRegistry& rResReg, WindowManager& rWinMan, Settings& rSet, SceneGraph& rScGr, TprComponent componentRenderable,
        uint8_t engineVersionVariant, uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
    )> factory;
    std::string name;

};


#endif  // HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
