
#ifndef ASSET_STORE_ASSET_STORE_HPP_
#define ASSET_STORE_ASSET_STORE_HPP_


#include "hardware_layer_interface.hpp"
#include "plugin_core.h"
#include "core.hpp"
#include "common.hpp"

#include <unordered_map>


// from "logger.hpp"
class Logger;

// from "resource_registy.hpp"
class ResourceRegistry;

// from "hardware_layer_interface.hpp"
class HardwareLayer;


class AssetStore {

    public:
        AssetStore(Logger& rLogger, ResourceRegistry& rRegReg, HardwareLayer& rHWLI);
        ~AssetStore() noexcept;

        expected<TprMesh, TprResult> createMesh(const TprMeshCreateInfo* info) noexcept;
        TprResult loadMesh(TprMesh mesh, const TprMeshLoadInfo* info) noexcept;
        void unloadMesh(TprMesh mesh) noexcept;
        void destroyMesh(TprMesh mesh) noexcept;

        expected<const AssetMesh&, TprResult> getMesh(TprMesh mesh);

    private:

        Logger& mrLogger;
        ResourceRegistry& mrResReg;
        HardwareLayer& mrHWLI;

        std::unordered_map<uint32_t, AssetMesh> mMeshes;
        uint32_t mMeshCounter = 0;


};

REGISTER_TYPE_NAME_S(AssetStore, "AStr");



#endif  // ASSET_STORE_ASSET_STORE_HPP_

