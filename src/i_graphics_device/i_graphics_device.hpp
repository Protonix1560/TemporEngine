

#ifndef HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
#define HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_


#include "core.hpp"
#include "graphics_common.hpp"
#include "plugin_core.h"
#include "asset_store/asset_store_common.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <string>


// from "window_manager.hpp"
class Windowing;

// from "file_registry.hpp"
class FileRegistry;

// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;

// from "scene_graph.hpp"
class SceneGraph;


class IGraphicsDevice {

    public:

        IGraphicsDevice() = default;
        virtual ~IGraphicsDevice() noexcept = default;

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

using PGraphicsDevice = std::unique_ptr<IGraphicsDevice>;

REGISTER_TYPE_NAME_S(PGraphicsDevice, "GDev");


struct GraphicsDeviceBackendInfo {

    GraphicsAPI graphicsAPI;

    std::function<expected<PGraphicsDevice, TprResult>(
        Logger logger, FileRegistry& rResReg, Windowing& rWin, Settings& rSet, SceneGraph& rScGr,
        TprComponent componentRenderable, uint64_t packedVersion
    )> factory;

    std::string name;

};


#endif  // HARDWARE_LAYER_INTERFACE_HARDWARE_LAYER_INTERFACE_HPP_
