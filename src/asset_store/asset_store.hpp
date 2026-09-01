
#ifndef ASSET_STORE_ASSET_STORE_HPP_
#define ASSET_STORE_ASSET_STORE_HPP_

#include "plugin_core.h"
#include "core.hpp"
#include "asset_store_common.hpp"
#include "logger.hpp"

#include <atomic>
#include <limits>
#include <mutex>
#include <unordered_map>
#include <memory>


// from "file_registy.hpp"
class FileRegistry;

// from "i_graphics_device.hpp"
class IGraphicsDevice;


struct MeshEntry {
    MeshData data;
    MeshIdentity id;
    uint32_t requiredLoadedCounter = 0;
};

struct MeshHandle {
    TprMeshCapabilityFlags capability = std::numeric_limits<TprMeshCapabilityFlags>::max();
    std::shared_ptr<MeshEntry> entry;
};


class AssetStore {
    public:
        AssetStore(Logger logger, FileRegistry& rFileReg, std::atomic<TprResult>& rRunResult);
        TprResult init(IGraphicsDevice* pGDev);
        ~AssetStore() noexcept;

        expected<TprMesh, TprResult> createMesh(const TprMeshCreateInfo& info) noexcept;
        expected<TprMesh, TprResult> createMeshCapability(TprMesh mesh, TprMeshCapabilityFlags mask) noexcept;
        void destroyMesh(TprMesh mesh) noexcept;

        TprResult requireMeshLoaded(TprMesh mesh) noexcept;
        TprResult unrequireMeshLoaded(TprMesh mesh) noexcept;

        // ======== IGraphicsDevice-specific API ========
        expected<MeshIdentity, TprResult> getMeshIdentity(TprMesh mesh);
        expected<const MeshData*, TprResult> getMesh(MeshIdentity id);

    private:
        TprResult unloadMesh(MeshEntry& entry);

        Logger mLogger;
        FileRegistry& mrFileReg;
        IGraphicsDevice* mpGDev;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;
        bool mInitialised = false;

        std::unordered_map<uint32_t, MeshHandle> mMeshes;
        std::unordered_map<MeshIdentity, std::shared_ptr<MeshEntry>> mMeshEntryMap;
        uint32_t mMeshHandleCounter = 0;
        uint32_t mMeshEntryCounter = 0;
};

REGISTER_TYPE_NAME_S(AssetStore, "AStr");


#endif  // ASSET_STORE_ASSET_STORE_HPP_
