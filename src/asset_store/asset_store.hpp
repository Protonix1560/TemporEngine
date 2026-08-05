
#ifndef ASSET_STORE_ASSET_STORE_HPP_
#define ASSET_STORE_ASSET_STORE_HPP_


#include "i_graphics_device.hpp"
#include "plugin_core.h"
#include "core.hpp"
#include "asset_store_common.hpp"
#include "logger.hpp"

#include <unordered_map>


// from "logger.hpp"
class Logger;

// from "file_registy.hpp"
class FileRegistry;

// from "hardware_layer_interface.hpp"
class IGraphicsDevice;


class AssetStore {

    public:
        AssetStore(Logger logger, FileRegistry& rRegReg, IGraphicsDevice& rHWLI);
        ~AssetStore() noexcept;

        expected<TprMesh, TprResult> createMesh(const TprMeshCreateInfo* info) noexcept;
        TprResult loadMesh(TprMesh mesh, const TprMeshLoadInfo* info) noexcept;
        void unloadMesh(TprMesh mesh) noexcept;
        void destroyMesh(TprMesh mesh) noexcept;

        expected<const AssetMesh&, TprResult> getMesh(TprMesh mesh);

    private:

        Logger mLogger;
        FileRegistry& mrFileReg;
        IGraphicsDevice& mrHWLI;

        std::unordered_map<uint32_t, AssetMesh> mMeshes;
        uint32_t mMeshCounter = 0;


};

REGISTER_TYPE_NAME_S(AssetStore, "AStr");



#endif  // ASSET_STORE_ASSET_STORE_HPP_

