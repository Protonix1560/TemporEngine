#pragma once

#include "../tempor.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>




#define TOF(call) do { \
    auto _r = (call); \
    if (_r) throw Exception(ErrCode::InternalError, __FILE__ + ":"s + std::to_string(__LINE__) + " "s + #call + ": returned "s + std::to_string(_r)); \
} while(0)




class Swapchain {

    public:
        void construct(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const WindowManager* windowManager);
        void destroy() noexcept;

        inline const VkSwapchainKHR& swapchain() const noexcept { return mSwapchain; }
        inline const VkFormat& format() const noexcept { return mFormat; }
        inline const VkSemaphore& getSemaphore(uint32_t index) const { return mSemaphores[index]; }
        inline const VkImageView& getImageView(uint32_t index) const { return mImageViews[index]; }
        inline const VkImage& getImage(uint32_t index) const { return mImages[index]; }
        inline const VkExtent2D& extent() const { return mExtent; }
        inline const uint32_t& imageCount() const { return mImageCount; }

    private:
        VkDevice mDevice;
        VkSurfaceKHR mSurface;
        const WindowManager* mpWindowManager;

        bool mConstructed = false;

        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> mSemaphores;
        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        VkFormat mFormat;
        VkExtent2D mExtent;
        uint32_t mImageCount = 0;

};


struct Frame {

    public:
        void construct(VkDevice device, uint32_t queueFamilyIndex);
        void destroy() noexcept;

        inline VkCommandBuffer& renderCommandBuffer() noexcept { return mCommandBuffers[0]; }
        inline VkCommandBuffer& presentCommandBuffer() noexcept { return mCommandBuffers[1]; }

        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;

    private:
        VkDevice mDevice;
        VkCommandBuffer mCommandBuffers[2] = {};
};


class FramebuffersHolder {

    public:
        void construct(Swapchain& swapchain, VkDevice device, VkRenderPass renderPass);
        void destroy() noexcept;

        inline const VkFramebuffer& getFramebuffer(uint32_t index) const { return mFramebuffers[index]; };

    private:
        std::vector<VkFramebuffer> mFramebuffers;
        VkDevice mDevice;

};


class BackendVk : public Backend {

    public:
        void init(const WindowManager *windowManager, const GlobalServiceLocator* serviceLocator) override;
        void destroy() noexcept override;
        void update() override;
        void render() override;

    private:
        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        VkRenderPass mRenderPass = VK_NULL_HANDLE;
        
        VkPhysicalDevice mPhysicalDevice;
        VkQueue mRenderQueue;

        const WindowManager* mpWindowManager;
        const GlobalServiceLocator* mpServiceLocator;

        Swapchain mSwapchain;
        FramebuffersHolder mFramebuffers;
        std::vector<Frame> mFrames;
        uint32_t mFrameCounter = 0;
        uint32_t mMaxFramesInFlight;

};

