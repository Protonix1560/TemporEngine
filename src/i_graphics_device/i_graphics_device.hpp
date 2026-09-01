
#ifndef I_GRAPHICS_DEVICE_I_GRAPHICS_DEVICE_HPP_
#define I_GRAPHICS_DEVICE_I_GRAPHICS_DEVICE_HPP_


#include "core.hpp"
#include "graphics_common.hpp"
#include "plugin_core.h"
#include "asset_store/asset_store_common.hpp"

#include <atomic>
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

// from "scheduler.hpp"
class Scheduler;

// from "asset_store.hpp"
class AssetStore;


class IGraphicsDevice {
    public:
        IGraphicsDevice() = default;
        virtual ~IGraphicsDevice() noexcept = default;

        virtual TprResult init() = 0;

        virtual expected<TprDepthDomain, TprResult> createDepthDomain(const TprDepthDomainCreateInfo& info) noexcept = 0;
        virtual expected<TprDepthDomain, TprResult> createDepthDomainCapability(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask) noexcept = 0;
        virtual void destroyDepthDomain(TprDepthDomain domain) noexcept = 0;

        virtual expected<TprRenderTarget, TprResult> createRenderTarget(const TprRenderTargetCreateInfo& info) noexcept = 0;
        virtual expected<TprRenderTarget, TprResult> createRenderTargetCapability(TprRenderTarget target, TprRenderTargetCapabilityFlags mask) noexcept = 0;
        virtual void destroyRenderTarget(TprRenderTarget target) noexcept = 0;

        virtual expected<TprRenderTargetSet, TprResult> createRenderTargetSet(const TprRenderTargetSetCreateInfo& info) noexcept = 0;
        virtual expected<TprRenderTargetSet, TprResult> createRenderTargetSetCapability(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask) noexcept = 0;
        virtual void destroyRenderTargetSet(TprRenderTargetSet set) noexcept = 0;

        virtual expected<TprEntityImage, TprResult> createEntityImage(const TprEntityImageCreateInfo& info) noexcept = 0;
        virtual expected<TprEntityImage, TprResult> createEntityImageCapability(TprEntityImage image, TprEntityImageCapabilityFlags mask) noexcept = 0;
        virtual void destroyEntityImage(TprEntityImage image) noexcept = 0;

        virtual TprJob getRenderJob() noexcept = 0;
        virtual TprJob getRenderSignalJob() noexcept = 0;
        virtual TprComponent getComponentRenderable() noexcept = 0;

        // =========== Windowing-specific API ===========
        virtual TprResult registerWindow(WindowIdentity id) = 0;
        virtual void unregisterWindow(WindowIdentity id) = 0;

        // ========== Asset Store-specific API ==========
        virtual TprResult loadMesh(MeshIdentity id) = 0;
        virtual TprResult unloadMesh(MeshIdentity id) = 0;
};

using PGraphicsDevice = std::unique_ptr<IGraphicsDevice>;

REGISTER_TYPE_NAME_S(PGraphicsDevice, "GDev");


struct GraphicsDeviceBackendInfo {
    std::function<PGraphicsDevice(
        Logger logger, FileRegistry& rResReg, Windowing& rWin, Settings& rSet, SceneGraph& rScGr,
        Scheduler& rSched, AssetStore& rAstr, std::atomic<TprResult>& rRunResult, uint32_t packedEngineVersion
    )> factory;
    std::string name;
    GraphicsAPI graphics;
};


#endif  // I_GRAPHICS_DEVICE_I_GRAPHICS_DEVICE_HPP_
