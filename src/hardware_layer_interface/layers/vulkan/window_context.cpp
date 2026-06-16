
#include "core.hpp"
#include "plugin_core.h"
#include "hardware_layer.hpp"
#include "logger.hpp"
#include "resource_registry.hpp"
#include "window_manager.hpp"

#include <algorithm>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


WindowContext::WindowContext(
    Logger logger, Allocator& rAlloc, WindowManager& rWinMan, ResourceRegistry& rResReg, 
    VulkanSymbols& rSym, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
    uint32_t queueFamilyIndex, uint32_t maxFramesInFlight, TprWindow window, VkPipelineLayout layout,
    VkDescriptorSetLayout objectSetLayout
) : WindowContextResources{
    .mLogger = logger, .mrAlloc = rAlloc, .mrWinMan = rWinMan, .mrResReg = rResReg, .mrSym = rSym, .mInstance = instance,
    .mPhysicalDevice = physicalDevice, .mDevice = device, .mBasicPipelineLayout = layout, .mObjectSetLayout = objectSetLayout,
    .mWindowHandle = window, .mMaxFramesInFlight = maxFramesInFlight, .mPoolQueueFamily = queueFamilyIndex
} {
    
    TprResult r;

    r = constructPersistent();
    if (r != TPR_SUCCESS) throw r;

    r = constructInvalidatable();
    if (r != TPR_SUCCESS) throw r;
}


TprResult WindowContext::constructPersistent() {
    VkResult vkResult;

    auto surfaceExp = mrWinMan.createSurfaceVk(mWindowHandle, mInstance);
    if (!surfaceExp.has_value()) throw surfaceExp.error();
    mSurface = surfaceExp.value();

    mFrames.resize(mMaxFramesInFlight);
    for (auto& frame : mFrames) {
        VkSemaphoreCreateInfo imageAvailableSemaphoreCreateInfo{};
        imageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkResult = mrSym.vkCreateSemaphore(mDevice, &imageAvailableSemaphoreCreateInfo, nullptr, &frame.imageAvailableSemaphore);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateSemaphore failed [" << vkResult << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkResult = mrSym.vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.inFlightFence);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateFence failed [" << vkResult << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        VkCommandPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreateInfo.queueFamilyIndex = mPoolQueueFamily;
        vkResult = mrSym.vkCreateCommandPool(mDevice, &poolCreateInfo, nullptr, &frame.commandPool);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateCommandPool failed [" << vkResult << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        VkCommandBufferAllocateInfo commandAllocInfo{};
        commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAllocInfo.commandBufferCount = std::size(frame.commandBuffers);
        commandAllocInfo.commandPool = frame.commandPool;
        commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        vkResult = mrSym.vkAllocateCommandBuffers(mDevice, &commandAllocInfo, frame.commandBuffers);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkAllocateCommandBuffers failed [" << vkResult << "]\n";
            return TPR_UNKNOWN_ERROR;
        }
    }

    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * mMaxFramesInFlight}
    };
    VkDescriptorPoolCreateInfo descPoolInfo{};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.poolSizeCount = std::size(poolSizes);
    descPoolInfo.pPoolSizes = poolSizes;
    descPoolInfo.maxSets = mMaxFramesInFlight;
    vkResult = mrSym.vkCreateDescriptorPool(mDevice, &descPoolInfo, nullptr, &mDescPool);
    if (vkResult != VK_SUCCESS) {
        mLogger.error(TPR_LOG_STYLE_ERROR1)
            << "WindowContext: vkCreateDescriptorPool failed [" << vkResult << "]\n";
        throw TPR_UNKNOWN_ERROR;
    }

    mObjectSets.resize(mMaxFramesInFlight);
    std::vector<VkDescriptorSetLayout> objectSetLayouts(mMaxFramesInFlight, mObjectSetLayout);
    VkDescriptorSetAllocateInfo objectSetsInfo{};
    objectSetsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objectSetsInfo.descriptorPool = mDescPool;
    objectSetsInfo.descriptorSetCount = mMaxFramesInFlight;
    objectSetsInfo.pSetLayouts = objectSetLayouts.data();
    vkResult = mrSym.vkAllocateDescriptorSets(mDevice, &objectSetsInfo, mObjectSets.data());
    if (vkResult != VK_SUCCESS) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkAllocateDescriptorSets failed [" << vkResult << "]\n";
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult WindowContext::constructInvalidatable() {
    VkResult result;

    VkSurfaceCapabilitiesKHR surfaceCaps;
    result = mrSym.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCaps);
    if (result != VK_SUCCESS) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }

    if (surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) {
        int32_t width = mrWinMan.getWindowWidth(mWindowHandle).value();
        int32_t height = mrWinMan.getWindowHeight(mWindowHandle).value();
        mExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        mExtent.width = std::clamp(
            mExtent.width,
            surfaceCaps.minImageExtent.width,
            surfaceCaps.maxImageExtent.width
        );
        mExtent.height = std::clamp(
            mExtent.height,
            surfaceCaps.minImageExtent.height,
            surfaceCaps.maxImageExtent.height
        );
    } else {
        mExtent.width = surfaceCaps.currentExtent.width;
        mExtent.height = surfaceCaps.currentExtent.height;
    }

    if (mExtent.width != 0 && mExtent.height != 0) {

        uint32_t formatCount;
        result = mrSym.vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        result = mrSym.vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, surfaceFormats.data());
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        auto scoreFormat = []( const VkSurfaceFormatKHR& format) -> uint32_t {
            uint32_t score = 0;

            switch (format.format) {
                case VK_FORMAT_B8G8R8A8_UNORM:
                    score += 100;
                case VK_FORMAT_R8G8B8A8_UNORM:
                    score += 1500;
                    break;

                case VK_FORMAT_B8G8R8_UNORM:
                    score += 100;
                case VK_FORMAT_R8G8B8_UNORM:
                    score += 1200;
                    break;
                case VK_FORMAT_B8G8R8A8_SRGB:
                    score += 100;
                case VK_FORMAT_R8G8B8A8_SRGB:
                    score += 2000;
                    break;

                case VK_FORMAT_B8G8R8_SRGB:
                    score += 100;
                case VK_FORMAT_R8G8B8_SRGB:
                    score += 1700;
                    break;

                default: break;
            }

            switch (format.colorSpace) {
                case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
                    score += 1000;
                    break;

                default: break;
            }

            return score;
        };

        VkSurfaceFormatKHR surfaceFormat = *std::max_element(
            surfaceFormats.begin(), surfaceFormats.end(), [&scoreFormat](const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b) -> bool {
                return scoreFormat(a) < scoreFormat(b);
            }
        );

        mChainImageFormat = surfaceFormat.format;

        uint32_t imageCount = surfaceCaps.minImageCount + 1;
        if (surfaceCaps.maxImageCount != 0 && surfaceCaps.maxImageCount < imageCount) {
            imageCount = surfaceCaps.maxImageCount;
        }

        uint32_t presentCount;
        result = mrSym.vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentCount, nullptr);
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetPhysicalDeviceSurfacePresentModesKHR at count retreival failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }
        std::vector<VkPresentModeKHR> presentModes(presentCount);
        result = mrSym.vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentCount, presentModes.data());
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetPhysicalDeviceSurfacePresentModesKHR at modes retreival failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (VkPresentModeKHR mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }

        VkSwapchainKHR oldSwapchain = mSwapchain;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.imageArrayLayers = 1;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageExtent = mExtent;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.minImageCount = imageCount;
        createInfo.oldSwapchain = oldSwapchain;
        createInfo.preTransform = surfaceCaps.currentTransform;
        createInfo.surface = mSurface;
        createInfo.presentMode = presentMode;

        result = mrSym.vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain);
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateSwapchainKHR failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        auto l = mLogger.debug();
        l << "Created swapchain " << mExtent.width << "x" << mExtent.height << " with format: ";
        switch (mChainImageFormat) {
            case VK_FORMAT_B8G8R8A8_UNORM: l << "VK_FORMAT_B8G8R8A8_UNORM"; break;
                case VK_FORMAT_R8G8B8A8_UNORM: l << "VK_FORMAT_R8G8B8A8_UNORM"; break;
                case VK_FORMAT_B8G8R8_UNORM: l << "VK_FORMAT_B8G8R8_UNORM"; break;
                case VK_FORMAT_R8G8B8_UNORM: l << "VK_FORMAT_R8G8B8_UNORM"; break;
                case VK_FORMAT_B8G8R8A8_SRGB: l << "VK_FORMAT_B8G8R8A8_SRGB"; break;
                case VK_FORMAT_R8G8B8A8_SRGB: l << "VK_FORMAT_R8G8B8A8_SRGB"; break;
                case VK_FORMAT_B8G8R8_SRGB: l << "VK_FORMAT_B8G8R8_SRGB"; break;
                case VK_FORMAT_R8G8B8_SRGB: l << "VK_FORMAT_R8G8B8_SRGB"; break;
            default: l << mChainImageFormat;
        }
        l << "\n";
        l.flush();

        if (oldSwapchain != VK_NULL_HANDLE) {
            mrSym.vkDestroySwapchainKHR(mDevice, oldSwapchain, nullptr);
        }

        mDepthImageFormat = VK_FORMAT_D32_SFLOAT;

        result = mrSym.vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, nullptr);
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetSwapchainImagesKHR at count retrieval failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        mChainImages.assign(mImageCount, VK_NULL_HANDLE);
        mChainImageViews.assign(mImageCount, VK_NULL_HANDLE);

        mDepthImages.assign(mImageCount, VK_NULL_HANDLE);
        mDepthImageViews.assign(mImageCount, VK_NULL_HANDLE);
        mDepthImageMemories.assign(mImageCount, {});

        mSemaphores.assign(mImageCount, VK_NULL_HANDLE);
        mFramebuffers.assign(mImageCount, VK_NULL_HANDLE);

        result = mrSym.vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mImageCount, mChainImages.data());
        if (result != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkGetSwapchainImagesKHR at image retrieval failed [" << result << "]\n";
            return TPR_UNKNOWN_ERROR;
        }

        for (uint32_t i = 0; i < mImageCount; i++) {

            // chain image view
            {
                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.format = mChainImageFormat;
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.image = mChainImages[i];
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                
                result = mrSym.vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &mChainImageViews[i]);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateImageView failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
            }

            // depth image
            {

                VkImageCreateInfo imageCreateInfo{};
                imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
                imageCreateInfo.arrayLayers = 1;
                imageCreateInfo.extent.width = mExtent.width;
                imageCreateInfo.extent.height = mExtent.height;
                imageCreateInfo.extent.depth = 1;
                imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageCreateInfo.mipLevels = 1;
                imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                imageCreateInfo.format = mDepthImageFormat;
                result = mrSym.vkCreateImage(mDevice, &imageCreateInfo, nullptr, &mDepthImages[i]);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateImage failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }

                VkMemoryRequirements req;
                mrSym.vkGetImageMemoryRequirements(mDevice, mDepthImages[i], &req);

                auto allocExp = mrAlloc.allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                if (!allocExp.has_value()) return allocExp.error();
                mDepthImageMemories[i] = allocExp.value();

                result = mrSym.vkBindImageMemory(mDevice, mDepthImages[i], mDepthImageMemories[i].memory, 0);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkBindMemory failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }

                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.image = mDepthImages[i];
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.format = mDepthImageFormat;
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                viewCreateInfo.image = mDepthImages[i];
                result = mrSym.vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &mDepthImageViews[i]);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateImageView failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }

            }

            // semaphore
            {
                VkSemaphoreCreateInfo semaphoreCreateInfo{};
                semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                result = mrSym.vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mSemaphores[i]);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateSemaphore failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
            }
        }
        
        // render pass
        {

            VkAttachmentDescription attachments[2] = {};
            VkAttachmentDescription& swapchainAttachment = attachments[0];
            swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            swapchainAttachment.format = mChainImageFormat;

            VkAttachmentDescription& depthAttachment = attachments[1];
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.format = mDepthImageFormat;

            VkSubpassDescription subpasses[1] = {};
            VkSubpassDescription& subpass = subpasses[0];
            VkAttachmentReference swapchainReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &swapchainReference;
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &depthReference;

            VkSubpassDependency dependencies[1] = {};
            VkSubpassDependency& dependency = dependencies[0];
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

            VkRenderPassCreateInfo renderPassCreateInfo{};
            renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassCreateInfo.attachmentCount = std::size(attachments);
            renderPassCreateInfo.pAttachments = attachments;
            renderPassCreateInfo.subpassCount = std::size(subpasses);
            renderPassCreateInfo.pSubpasses = subpasses;
            renderPassCreateInfo.dependencyCount = std::size(dependencies);
            renderPassCreateInfo.pDependencies = dependencies;

            result = mrSym.vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass.renderPass);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateRenderPass failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            mLogger.debug() << "WindowContext: Created render pass\n";

        }

        // basic pipeline
        {
            VkPipelineColorBlendAttachmentState colourBlendAttach{};
            colourBlendAttach.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colourBlendAttach.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colourBlend{};
            colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colourBlend.attachmentCount = 1;
            colourBlend.pAttachments = &colourBlendAttach;

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
            raster.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            
            VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.pDynamicStates = dynamics;
            dynamicState.dynamicStateCount = std::size(dynamics);
            
            VkPipelineDepthStencilStateCreateInfo depthState{};
            depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthState.depthBoundsTestEnable = VK_FALSE;
            depthState.depthTestEnable = VK_TRUE;
            depthState.depthWriteEnable = VK_TRUE;
            depthState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthState.stencilTestEnable = VK_FALSE;
            depthState.minDepthBounds = 0.0f;
            depthState.maxDepthBounds = 1.0f;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkPipelineShaderStageCreateInfo stages[2] = {};

            VkShaderModule fragShader;
            TprResource fragRes = mrResReg.openResource("shaders/vulkan/basic.frag.spv").value();
            if (mrResReg.sizeofResource(fragRes).value() > UINT32_MAX) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: shaders/vulkan/basic.frag.spv shader size is greater that 4GiB\n";
                return TPR_UNKNOWN_ERROR;
            }
            VkShaderModuleCreateInfo fragModuleCreateInfo{};
            fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            fragModuleCreateInfo.codeSize = static_cast<uint32_t>(mrResReg.sizeofResource(fragRes).value());
            fragModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(mrResReg.getResourceConstPointer(fragRes).value());
            result = mrSym.vkCreateShaderModule(mDevice, &fragModuleCreateInfo, nullptr, &fragShader);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1)
                    << "WindowContext: vkCreateShaderModule at debug basic pipeline's fragment shader creation failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }
            
            VkPipelineShaderStageCreateInfo& fragStage = stages[1];
            fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragStage.module = fragShader;
            fragStage.pName = "main";

            VkShaderModule vertShader;
            TprResource vertRes = mrResReg.openResource("shaders/vulkan/basic.vert.spv").value();
            if (mrResReg.sizeofResource(vertRes).value() > UINT32_MAX) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: shaders/vulkan/basic.frag.spv shader size is greater that 4GiB\n";
                return TPR_UNKNOWN_ERROR;
            }
            VkShaderModuleCreateInfo vertModuleCreateInfo{};
            vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vertModuleCreateInfo.codeSize = static_cast<uint32_t>(mrResReg.sizeofResource(vertRes).value());
            vertModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(mrResReg.getResourceConstPointer(vertRes).value());
            result = mrSym.vkCreateShaderModule(mDevice, &vertModuleCreateInfo, nullptr, &vertShader);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1)
                    << "WindowContext: vkCreateShaderModule at basic pipeline's vertex shader creation failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            VkPipelineShaderStageCreateInfo& vertStage = stages[0];
            vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertStage.module = vertShader;
            vertStage.pName = "main";

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
            viewportState.pViewports = nullptr;
            viewportState.pScissors = nullptr;

            VkVertexInputBindingDescription vertBindings[] = {
                {0, sizeof(VertexPosition), VK_VERTEX_INPUT_RATE_VERTEX}
            };
            VkVertexInputAttributeDescription vertDescs[] = {
                {0, 0, VertexPosition::format, 0}
            };

            VkPipelineVertexInputStateCreateInfo vertexInput{};
            vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInput.pVertexBindingDescriptions = vertBindings;
            vertexInput.vertexBindingDescriptionCount = std::size(vertBindings);
            vertexInput.pVertexAttributeDescriptions = vertDescs;
            vertexInput.vertexAttributeDescriptionCount = std::size(vertDescs);

            VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
            pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineCreateInfo.layout = mBasicPipelineLayout;
            pipelineCreateInfo.pColorBlendState = &colourBlend;
            pipelineCreateInfo.pRasterizationState = &raster;
            pipelineCreateInfo.pMultisampleState = &multisampling;
            pipelineCreateInfo.pDynamicState = &dynamicState;
            pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
            pipelineCreateInfo.renderPass = mRenderPass.renderPass;
            pipelineCreateInfo.subpass = 0;
            pipelineCreateInfo.pStages = stages;
            pipelineCreateInfo.stageCount = std::size(stages);
            pipelineCreateInfo.pViewportState = &viewportState;
            pipelineCreateInfo.pVertexInputState = &vertexInput;
            pipelineCreateInfo.pDepthStencilState = &depthState;

            result = mrSym.vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mRenderPass.basicPipeline);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1)
                    << "WindowContext: vkCreateGraphicsPipelines at basic pipeline creation failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            mrSym.vkDestroyShaderModule(mDevice, fragShader, nullptr);
            mrSym.vkDestroyShaderModule(mDevice, vertShader, nullptr);

            mLogger.debug() << "WindowContext: Created basic pipeline\n";
        }

        for (uint32_t i = 0; i < mImageCount; i++) {
            // framebuffer
            {
                VkImageView attachments[] = {
                    mChainImageViews[i],
                    mDepthImageViews[i]
                };

                VkFramebufferCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                createInfo.width = mExtent.width;
                createInfo.height = mExtent.height;
                createInfo.attachmentCount = std::size(attachments);
                createInfo.pAttachments = attachments;
                createInfo.layers = 1;
                createInfo.renderPass = mRenderPass.renderPass;

                result = mrSym.vkCreateFramebuffer(mDevice, &createInfo, nullptr, &mFramebuffers[i]);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "WindowContext: vkCreateFramebuffer failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
            }
        }
    }
    return TPR_SUCCESS;
}


TprResult WindowContext::recreate() {

    for (uint32_t i = 0; i < mChainImages.size(); i++) {
        if (mFramebuffers[i]) {
            if (mFreeResources) mrSym.vkDestroyFramebuffer(mDevice, mFramebuffers[i], nullptr);
            mFramebuffers[i] = VK_NULL_HANDLE;
        }
    }

    if (mRenderPass.basicPipeline) {
        if (mFreeResources) mrSym.vkDestroyPipeline(mDevice, mRenderPass.basicPipeline, nullptr);
        mRenderPass.basicPipeline = VK_NULL_HANDLE;
    }
    if (mRenderPass.renderPass) {
        if (mFreeResources) mrSym.vkDestroyRenderPass(mDevice, mRenderPass.renderPass, nullptr);
        mRenderPass.renderPass = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < mChainImages.size(); i++) {
        if (mFreeResources) if (mChainImageViews[i]) mrSym.vkDestroyImageView(mDevice, mChainImageViews[i], nullptr);

        if (mFreeResources) if (mDepthImageViews[i]) mrSym.vkDestroyImageView(mDevice, mDepthImageViews[i], nullptr);
        if (mFreeResources) if (mDepthImageMemories[i].memory) mrSym.vkFreeMemory(mDevice, mDepthImageMemories[i].memory, nullptr);
        if (mFreeResources) if (mDepthImages[i]) mrSym.vkDestroyImage(mDevice, mDepthImages[i], nullptr);

        if (mFreeResources) if (mSemaphores[i]) mrSym.vkDestroySemaphore(mDevice, mSemaphores[i], nullptr);
    }

    return constructInvalidatable();
}


WindowContext::~WindowContext() {

    if (!mFreeResources) return;

    if (mDescPool) {
        mrSym.vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < mFramebuffers.size(); i++) {
        if (mFramebuffers[i]) {
            mrSym.vkDestroyFramebuffer(mDevice, mFramebuffers[i], nullptr);
            mFramebuffers[i] = VK_NULL_HANDLE;
        }
    }

    if (mRenderPass.basicPipeline) {
        mrSym.vkDestroyPipeline(mDevice, mRenderPass.basicPipeline, nullptr);
        mRenderPass.basicPipeline = VK_NULL_HANDLE;
    }
    if (mRenderPass.renderPass) {
        mrSym.vkDestroyRenderPass(mDevice, mRenderPass.renderPass, nullptr);
        mRenderPass.renderPass = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < mChainImages.size(); i++) {
        if (mSemaphores[i]) {
            mrSym.vkDestroySemaphore(mDevice, mSemaphores[i], nullptr);
            mSemaphores[i] = VK_NULL_HANDLE;
        }

        if (mDepthImageViews[i]) {
            mrSym.vkDestroyImageView(mDevice, mDepthImageViews[i], nullptr);
            mDepthImageViews[i] = VK_NULL_HANDLE;
        }

        if (mDepthImageMemories[i].memory) {
            mrSym.vkFreeMemory(mDevice, mDepthImageMemories[i].memory, nullptr);
            mDepthImageMemories[i] = {};
        }

        if (mDepthImages[i]) {
            mrSym.vkDestroyImage(mDevice, mDepthImages[i], nullptr);
            mDepthImages[i] = VK_NULL_HANDLE;
        }

        if (mChainImageViews[i]) {
            mrSym.vkDestroyImageView(mDevice, mChainImageViews[i], nullptr);
            mChainImageViews[i] = VK_NULL_HANDLE;
        }
    }

    if (mSwapchain) {
        mrSym.vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }

    for (auto& frame : mFrames) {
        if (frame.commandPool) {
            mrSym.vkDestroyCommandPool(mDevice, frame.commandPool, nullptr);
            frame.commandPool = VK_NULL_HANDLE;
        }

        if (frame.inFlightFence) {
            mrSym.vkDestroyFence(mDevice, frame.inFlightFence, nullptr);
            frame.inFlightFence = VK_NULL_HANDLE;
        }

        if (frame.imageAvailableSemaphore) {
            mrSym.vkDestroySemaphore(mDevice, frame.imageAvailableSemaphore, nullptr);
            frame.imageAvailableSemaphore = VK_NULL_HANDLE;
        }
    }

    if (mSurface) {
        mrSym.vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
        mSurface = VK_NULL_HANDLE;
    }
}


