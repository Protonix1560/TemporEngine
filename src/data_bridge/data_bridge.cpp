

#include "data_bridge.hpp"
#include "plugin_core.h"



void DataBridge::init() {

}


void DataBridge::shutdown() noexcept {

}


void DataBridge::update() {

}


TprResult DataBridge::growOArray(OArray_T* oarray, uint32_t size, void** start) noexcept {
    if (!oarray) return TPR_INVALID_VALUE;
    if (!start) return TPR_INVALID_VALUE;
    try {
        oarray->resizeHandle(oarray->size + size);
        oarray->size += size;
        *start = oarray->dataHandle();
        oarray->finalized = false;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult DataBridge::endOArray(OArray_T* oarray, uint32_t actualSize) noexcept {
    if (!oarray) return TPR_INVALID_VALUE;
    try {
        oarray->finalized = true;
        oarray->resizeHandle(oarray->prevSize + actualSize);
        oarray->size = oarray->prevSize + actualSize;
        oarray->prevSize = oarray->size;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult DataBridge::sizeofOArray(OArray_T* oarray, uint32_t* size) noexcept {
    if (!oarray) return TPR_INVALID_VALUE;
    if (!size) return TPR_INVALID_VALUE;
    try {
        *size = oarray->size;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    } 
    return TPR_SUCCESS;
}



