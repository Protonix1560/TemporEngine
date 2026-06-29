
#include "core.hpp"
#include "hardware_layer.hpp"
#include "plugin_core.h"


expected<TprDepthDomain, TprResult> HardwareLayerVulkan::createDepthDomain(const TprDepthDomainCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);

    TprDepthDomain handle;

    try {
        if (pInfo->pAnchor) {
            if (!pInfo->pAnchor) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto anchorIt = std::ranges::find(mDepthDomainOrder, get_basic_handle_index(*pInfo->pAnchor));
            if (anchorIt == mDepthDomainOrder.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto insertIt = anchorIt;
            if (!(pInfo->flags & TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT)) insertIt = std::next(insertIt);
            auto& domain = mDepthDomains.try_emplace(mDepthDomainCounter).first->second;
            mDepthDomainOrder.insert(insertIt, mDepthDomainCounter);
            handle = construct_basic_handle<TprDepthDomain>(mDepthDomainCounter, 0, handle_type::depth_domain);
            mDepthDomainCounter++;

        } else {
            auto& domain = mDepthDomains.try_emplace(mDepthDomainCounter).first->second;
            if (pInfo->flags & TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT) {
                mDepthDomainOrder.insert(mDepthDomainOrder.begin(), mDepthDomainCounter);
            } else {
                mDepthDomainOrder.push_back(mDepthDomainCounter);
            }
            handle = construct_basic_handle<TprDepthDomain>(mDepthDomainCounter, 0, handle_type::depth_domain);
            mDepthDomainCounter++;
        }

    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


void HardwareLayerVulkan::destroyDepthDomain(TprDepthDomain domain) noexcept {
    try {
        auto storageIt = mDepthDomains.find(get_basic_handle_index(domain));
        if (storageIt == mDepthDomains.end()) return;
        auto orderIt = std::ranges::find(mDepthDomainOrder, get_basic_handle_index(domain));
        if (orderIt == mDepthDomainOrder.end()) return;
        for (auto it = mRenderTargets.begin(); it != mRenderTargets.end(); it++) {
            if (it->second.domain == get_basic_handle_index(domain)) {
                destroyRenderTarget(construct_basic_handle<TprRenderTarget>(it->first, 0, handle_type::render_target));
            }
        }
        mDepthDomains.erase(storageIt);
        mDepthDomainOrder.erase(orderIt);
    } catch (...) {
        return;
    }
}


expected<TprRenderTarget, TprResult> HardwareLayerVulkan::createRenderTarget(const TprRenderTargetCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);

    TprRenderTarget handle;

    try {
        auto& target = mRenderTargets.try_emplace(mRenderTargetCounter).first->second;
        target.domain = get_basic_handle_index(pInfo->depthDomain);
        target.scissor = pInfo->scissor;
        target.viewport = pInfo->viewport;
        target.window = pInfo->window;
        handle = construct_basic_handle<TprRenderTarget>(mRenderTargetCounter, 0, handle_type::render_target);
        mRenderTargetCounter++;

    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


void HardwareLayerVulkan::destroyRenderTarget(TprRenderTarget target) noexcept {
    try {
        auto it = mRenderTargets.find(get_basic_handle_index(target));
        if (it == mRenderTargets.end()) return;
        for (uint32_t objectImage : it->second.objectImages) {
            destroyObjectImage(construct_basic_handle<TprObjectImage>(it->first, 0, handle_type::object_image));
        }
        mRenderTargets.erase(it);
    } catch (...) {
        return;
    }
}


expected<TprObjectImage, TprResult> HardwareLayerVulkan::createObjectImage(const TprObjectImageCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (pInfo->renderTargetCount > 0 && !pInfo->pRenderTargets) return unexpected(TPR_ERROR_INVALID_VALUE);

    TprObjectImage handle;

    try {
        for (const TprRenderTarget* ptr = pInfo->pRenderTargets; ptr < pInfo->pRenderTargets + pInfo->renderTargetCount; ptr++) {
            TprRenderTarget target = *ptr;
            auto it = mRenderTargets.find(get_basic_handle_index(target));
            if (it == mRenderTargets.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        }
        auto& rImage = mObjectImages.try_emplace(mObjectImageCounter).first->second;
        for (const TprRenderTarget* ptr = pInfo->pRenderTargets; ptr < pInfo->pRenderTargets + pInfo->renderTargetCount; ptr++) {
            rImage.renderTargets.push_back(get_basic_handle_index(*ptr));
        }
        rImage.mesh = pInfo->mesh;
        handle = construct_basic_handle<TprObjectImage>(mObjectImageCounter, 0, handle_type::object_image);
        for (const TprRenderTarget* ptr = pInfo->pRenderTargets; ptr < pInfo->pRenderTargets + pInfo->renderTargetCount; ptr++) {
            TprRenderTarget target = *ptr;
            auto& rTarget = mRenderTargets.at(get_basic_handle_index(target));
            rTarget.objectImages.push_back(mObjectImageCounter);
        }
        mObjectImageCounter++;

    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


void HardwareLayerVulkan::destroyObjectImage(TprObjectImage image) noexcept {
    try {
        auto it = mObjectImages.find(get_basic_handle_index(image));
        if (it == mObjectImages.end()) return;
        mObjectImages.erase(it);
    } catch (...) {
        return;
    }
}
