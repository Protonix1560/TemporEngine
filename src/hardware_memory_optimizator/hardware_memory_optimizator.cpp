
#include "hardware_memory_optimizator.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "hardware_layer_interface.hpp"
#include "asset_store.hpp"



void HardwareMemoryOptimizator::init() {
    mLoc = *gGetServiceLocator();
}


void HardwareMemoryOptimizator::update() {
    
    auto& logger = mLoc.get<Logger>();
    auto& hwl = mLoc.get<HardwareLayer>();
    auto& assetStore = mLoc.get<AssetStore>();

}


void HardwareMemoryOptimizator::shutdown() noexcept {
    
}

