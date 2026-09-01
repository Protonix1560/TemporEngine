
#ifndef I_GRAPHICS_DEVICE_BACKENDS_VULKAN_BACKEND_HPP_
#define I_GRAPHICS_DEVICE_BACKENDS_VULKAN_BACKEND_HPP_

#include "core.hpp"
#include "graphics_common.hpp"
#include "plugin_core.h"
#include "i_graphics_device.hpp"
#include "logger.hpp"
#include "interval_union.hpp"
#include "hash.hpp"
#include "scope_guard.hpp"
#include "linalg_packed.hpp"

#include <limits>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <array>
#include <source_location>

#include <glm/fwd.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


// from "file_registry.hpp"
class FileRegistry;

// from "window_manager.hpp"
class Windowing;

// from "settings.hpp"
class Settings;

// from "scene_graph.hpp"
class SceneGraph;


/*
struct Allocator {
    private:
        Logger mLogger;
        VulkanSymbols& mrSym;
        std::vector<VkMemoryType> mMemoryTypes;
        VkPhysicalDevice mPhysicalDevice;
        VkDevice mDevice;
        uint32_t mAllocCount = 0;
        uint32_t mMaxAllocCount;
        
        struct Memory {
            void* map = nullptr;
        };
        std::unordered_map<VkDeviceMemory, Memory> mMemories;

    public:
        Allocator(Logger logger, VulkanSymbols& rSym, VkPhysicalDevice physicalDevice, VkDevice device);

        expected<uint32_t, TprResult> findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags property);

        expected<Allocation, TprResult> allocate(VkMemoryRequirements requirements, VkMemoryPropertyFlags property);
        void free(Allocation allocation) noexcept;

        expected<void*, TprResult> map(Allocation alloc);
        bool mapped(Allocation alloc) const noexcept;
        void unmap(Allocation alloc) noexcept;
};


struct BufferResources {
    Logger mLogger;
    VulkanSymbols& mrSym;
    Allocator& mrAlloc;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;

    VkBuffer mBuffer = VK_NULL_HANDLE;
    Allocation mAllocation;
    VkDeviceSize mSize;

    void* mMap = nullptr;
    VkDeviceSize mMapOffset;
    VkDeviceSize mMapSize;
    VkMemoryMapFlags mMapFlags;

    VkBufferUsageFlags mUsage;
    VkMemoryPropertyFlags mProperty;
    VkSharingMode mSharing;
    std::vector<uint32_t> mQueueFamilyIndices;
};

struct Buffer : private BufferResources {
    private:
        bool freeResources = true;

    public:
        void* mapping() const { return mMap; }
        bool mapped() const { return mMap != nullptr; }
        uint32_t size() const { return mSize; }
        bool empty() const { return mSize == 0; }

        Buffer(
            Logger logger, VulkanSymbols& rSym, Allocator& rAlloc, VkPhysicalDevice physicalDevice, VkDevice device,
            VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, std::span<uint32_t> queueFamilyIndices = {}
        );

        Buffer(const Buffer& other) = delete;

        Buffer(Buffer&& other) noexcept : BufferResources(std::move(other)) {
            other.freeResources = false;
        }

        TprResult mapMemory();
        void unmapMemory() noexcept;

        TprResult reallocate(VkDeviceSize newSize);

        VkBuffer handle() const;

        ~Buffer() noexcept;
};
*/


class VulkanBackend : public IGraphicsDevice {
    public:
        VulkanBackend(
            Logger logger, FileRegistry& rResReg, Windowing& rWinMan, Settings& rSettings, SceneGraph& rScGr,
            Scheduler& rSched, AssetStore& rAstr, std::atomic<TprResult>& rRunResult, uint32_t packedEngineVersion
        );
        ~VulkanBackend() noexcept;

        TprResult init() override;

        expected<TprDepthDomain, TprResult> createDepthDomain(const TprDepthDomainCreateInfo& info) noexcept override;
        expected<TprDepthDomain, TprResult> createDepthDomainCapability(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask) noexcept override;
        void destroyDepthDomain(TprDepthDomain domain) noexcept override;
        expected<TprRenderTarget, TprResult> createRenderTarget(const TprRenderTargetCreateInfo& info) noexcept override;
        expected<TprRenderTarget, TprResult> createRenderTargetCapability(TprRenderTarget target, TprRenderTargetCapabilityFlags mask) noexcept override;
        void destroyRenderTarget(TprRenderTarget target) noexcept override;
        expected<TprRenderTargetSet, TprResult> createRenderTargetSet(const TprRenderTargetSetCreateInfo& info) noexcept override;
        expected<TprRenderTargetSet, TprResult> createRenderTargetSetCapability(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask) noexcept override;
        void destroyRenderTargetSet(TprRenderTargetSet set) noexcept override;
        expected<TprEntityImage, TprResult> createEntityImage(const TprEntityImageCreateInfo& info) noexcept override;
        expected<TprEntityImage, TprResult> createEntityImageCapability(TprEntityImage image, TprEntityImageCapabilityFlags mask) noexcept override;
        void destroyEntityImage(TprEntityImage image) noexcept override;
        TprJob getRenderJob() noexcept override;
        TprJob getRenderSignalJob() noexcept override;
        TprComponent getComponentRenderable() noexcept override;
        TprResult registerWindow(WindowIdentity id) override;
        void unregisterWindow(WindowIdentity id) override;
        TprResult loadMesh(MeshIdentity id) override;
        TprResult unloadMesh(MeshIdentity id) override;

    private:
        #pragma region structs
            struct Vertex {
                static VkVertexInputBindingDescription bindDesc() {
                    VkVertexInputBindingDescription bindingDesc{};
                    bindingDesc.binding = 0;
                    bindingDesc.stride = sizeof(Vertex);
                    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    return bindingDesc;
                }
                static std::array<VkVertexInputAttributeDescription, 2> attributeDesc() {
                    std::array<VkVertexInputAttributeDescription, 2> attribs{};
                    attribs[0].binding = 0;
                    attribs[0].location = 0;
                    attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
                    attribs[0].offset = 0;
                    attribs[1].binding = 0;
                    attribs[1].location = 1;
                    attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
                    attribs[1].offset = 12;
                    return attribs;
                }
                glm::vec3 pos;
                uint32_t colour;
                static constexpr size_t perfectPackSize = 16;
            };

            #define GEN_FUNC(name, type, loader) \
                private: std::optional<PFN_##name> m##name; \
                public: PFN_##name name() { \
                    assert(loader); \
                    if (!m##name.has_value()) m##name.emplace(reinterpret_cast<PFN_##name>(type(loader, #name))); \
                    return m##name.value(); \
                }

            #define GEN_FUNC_NONE(name) GEN_FUNC(name, mvkGetInstanceProcAddr, nullptr)

            #define GEN_FUNC_INSTANCE(name) GEN_FUNC(name, mvkGetInstanceProcAddr, mInstance)

            #define GEN_FUNC_DEVICE(name) GEN_FUNC(name, vkGetDeviceProcAddr(), mDevice)

            struct SymLoader {
                private:
                    PFN_vkGetInstanceProcAddr mvkGetInstanceProcAddr = nullptr;
                    VkInstance mInstance = VK_NULL_HANDLE;
                    VkDevice mDevice = VK_NULL_HANDLE;

                    GEN_FUNC_NONE(vkEnumerateInstanceVersion);
                    GEN_FUNC_NONE(vkEnumerateInstanceLayerProperties);
                    GEN_FUNC_NONE(vkEnumerateInstanceExtensionProperties);
                    GEN_FUNC_NONE(vkCreateInstance);

                    GEN_FUNC_INSTANCE(vkCreateDebugUtilsMessengerEXT);
                    GEN_FUNC_INSTANCE(vkDestroyDebugUtilsMessengerEXT);
                    GEN_FUNC_INSTANCE(vkEnumeratePhysicalDevices);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceProperties);
                    GEN_FUNC_INSTANCE(vkEnumerateDeviceExtensionProperties);
                    GEN_FUNC_INSTANCE(vkDestroyInstance);
                    GEN_FUNC_INSTANCE(vkCreateDevice);
                    GEN_FUNC_INSTANCE(vkDestroyDevice);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
                    GEN_FUNC_INSTANCE(vkDestroySurfaceKHR);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
                    GEN_FUNC_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);

                    GEN_FUNC_INSTANCE(vkGetDeviceProcAddr);

                    GEN_FUNC_DEVICE(vkGetDeviceQueue);
                    GEN_FUNC_DEVICE(vkCreateBuffer);
                    GEN_FUNC_DEVICE(vkGetBufferMemoryRequirements);
                    GEN_FUNC_DEVICE(vkAllocateMemory);
                    GEN_FUNC_DEVICE(vkBindBufferMemory);
                    GEN_FUNC_DEVICE(vkMapMemory);
                    GEN_FUNC_DEVICE(vkUnmapMemory);
                    GEN_FUNC_DEVICE(vkFreeMemory);
                    GEN_FUNC_DEVICE(vkDestroyBuffer);
                    GEN_FUNC_DEVICE(vkCreateSemaphore);
                    GEN_FUNC_DEVICE(vkCreateFence);
                    GEN_FUNC_DEVICE(vkCreateCommandPool);
                    GEN_FUNC_DEVICE(vkAllocateCommandBuffers);
                    GEN_FUNC_DEVICE(vkDestroyCommandPool);
                    GEN_FUNC_DEVICE(vkDestroyFence);
                    GEN_FUNC_DEVICE(vkDestroySemaphore);
                    GEN_FUNC_DEVICE(vkDestroySwapchainKHR);
                    GEN_FUNC_DEVICE(vkCreateSwapchainKHR);
                    GEN_FUNC_DEVICE(vkGetSwapchainImagesKHR);
                    GEN_FUNC_DEVICE(vkCreateImageView);
                    GEN_FUNC_DEVICE(vkCreateImage);
                    GEN_FUNC_DEVICE(vkGetImageMemoryRequirements);
                    GEN_FUNC_DEVICE(vkBindImageMemory);
                    GEN_FUNC_DEVICE(vkDestroyImageView);
                    GEN_FUNC_DEVICE(vkDestroyImage);
                    GEN_FUNC_DEVICE(vkCreateFramebuffer);
                    GEN_FUNC_DEVICE(vkDestroyFramebuffer);
                    GEN_FUNC_DEVICE(vkCreateRenderPass);
                    GEN_FUNC_DEVICE(vkCreatePipelineLayout);
                    GEN_FUNC_DEVICE(vkCreateShaderModule);
                    GEN_FUNC_DEVICE(vkCreateGraphicsPipelines);
                    GEN_FUNC_DEVICE(vkResetCommandBuffer);
                    GEN_FUNC_DEVICE(vkBeginCommandBuffer);
                    GEN_FUNC_DEVICE(vkEndCommandBuffer);
                    GEN_FUNC_DEVICE(vkWaitForFences);
                    GEN_FUNC_DEVICE(vkResetCommandPool);
                    GEN_FUNC_DEVICE(vkAcquireNextImageKHR);
                    GEN_FUNC_DEVICE(vkCmdBeginRenderPass);
                    GEN_FUNC_DEVICE(vkCmdSetScissor);
                    GEN_FUNC_DEVICE(vkCmdSetViewport);
                    GEN_FUNC_DEVICE(vkCmdEndRenderPass);
                    GEN_FUNC_DEVICE(vkResetFences);
                    GEN_FUNC_DEVICE(vkQueueSubmit);
                    GEN_FUNC_DEVICE(vkQueuePresentKHR);
                    GEN_FUNC_DEVICE(vkDeviceWaitIdle);
                    GEN_FUNC_DEVICE(vkCmdCopyBuffer);
                    GEN_FUNC_DEVICE(vkCreateDescriptorSetLayout);
                    GEN_FUNC_DEVICE(vkDestroyPipelineLayout);
                    GEN_FUNC_DEVICE(vkDestroyPipeline);
                    GEN_FUNC_DEVICE(vkDestroyRenderPass);
                    GEN_FUNC_DEVICE(vkDestroyDescriptorSetLayout);
                    GEN_FUNC_DEVICE(vkDestroyShaderModule);
                    GEN_FUNC_DEVICE(vkCreateDescriptorPool);
                    GEN_FUNC_DEVICE(vkDestroyDescriptorPool);
                    GEN_FUNC_DEVICE(vkAllocateDescriptorSets);
                    GEN_FUNC_DEVICE(vkUpdateDescriptorSets);
                    GEN_FUNC_DEVICE(vkCmdBindDescriptorSets);
                    GEN_FUNC_DEVICE(vkCmdBindPipeline);
                    GEN_FUNC_DEVICE(vkCmdDrawIndexed);
                    GEN_FUNC_DEVICE(vkCmdBindVertexBuffers);
                    GEN_FUNC_DEVICE(vkCmdBindIndexBuffer);
                    GEN_FUNC_DEVICE(vkCmdDrawIndexedIndirect);

                public:
                    void setLoadPtr(PFN_vkGetInstanceProcAddr ptr) { mvkGetInstanceProcAddr = ptr; }
                    void setInstance(VkInstance instance) { mInstance = instance; }
                    void setDevice(VkDevice device) { mDevice = device; }
            };

            struct Allocation {
                VkDeviceMemory memory = VK_NULL_HANDLE;
                VkDeviceSize offset = 0;
            };

            struct PartialBuffer {
                VkBuffer buffer = VK_NULL_HANDLE;
                uint64_t byteSize;
                Allocation alloc;
                interval_union<uint64_t> free;
                scope_guard guard;
            };

            struct FullBuffer {
                VkBuffer buffer = VK_NULL_HANDLE;
                uint64_t byteSize;
                Allocation alloc;
                scope_guard guard;
            };

            struct RenderTargetEntry;

            struct RenderTargetSetEntry {
                std::vector<std::weak_ptr<RenderTargetEntry>> renderTargets;
            };

            struct RenderTargetSetHandle {
                TprRenderTargetSetCapabilityFlags capability = std::numeric_limits<TprRenderTargetSetCapabilityFlags>::max();
                std::shared_ptr<RenderTargetSetEntry> entry;
            };

            struct Image {
                VkImage image = VK_NULL_HANDLE;
                VkImageView view = VK_NULL_HANDLE;
                Allocation alloc;
            };

            struct Swapchain {
                struct Link {
                    struct SwapchainImage {
                        VkImage image = VK_NULL_HANDLE;
                        VkImageView view = VK_NULL_HANDLE;
                    } swap;
                    Image depth;
                    VkFramebuffer framebuffer = VK_NULL_HANDLE;
                    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
                };
                std::vector<Link> links;
                VkFormat swapFormat;
                VkFormat depthFormat;
                VkSwapchainKHR swapchain = VK_NULL_HANDLE;
                uint32_t currentLinkIndex;
            };

            struct RenderPass {
                VkRenderPass renderPass = VK_NULL_HANDLE;
                VkPipeline basicPipeline = VK_NULL_HANDLE;
            };

            struct Frame {
                VkCommandPool commandPool = VK_NULL_HANDLE;
                VkCommandBuffer renderCommandBuffer = VK_NULL_HANDLE;
                VkCommandBuffer presentCommandBuffer = VK_NULL_HANDLE;
                VkFence inFlightFence = VK_NULL_HANDLE;
                std::vector<std::shared_ptr<PartialBuffer>> buffersHeld;
                FullBuffer entityChunksBuffer;
                VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
                VkDescriptorSet entityDataSet = VK_NULL_HANDLE;
            };

            struct WindowEntry {
                WindowIdentity id;
                VkSurfaceKHR surface = VK_NULL_HANDLE;
                Swapchain swapchain;
                VkExtent2D extent;
                RenderPass renderPass;
                std::vector<VkSemaphore> imageAvailableSemaphores;
                std::vector<std::weak_ptr<RenderTargetEntry>> renderTargets;
            };

            struct DepthDomainEntry {
                std::vector<std::weak_ptr<RenderTargetEntry>> renderTargets;
            };

            struct DepthDomainHandle {
                TprDepthDomainCapabilityFlags capability = std::numeric_limits<TprDepthDomainCapabilityFlags>::max();
                std::shared_ptr<DepthDomainEntry> entry;
            };

            struct MeshEntry;

            struct EntityDataRange {
                MeshEntry* mesh;
                uint32_t offset;
                uint32_t count;
            };

            struct IndirectDraws {
                MeshEntry* mesh;
                uint32_t offset;
                uint32_t count;
            };

            struct RenderTargetEntry {
                std::vector<std::weak_ptr<RenderTargetSetEntry>> sets;
                std::weak_ptr<DepthDomainEntry> depthDomain;
                WindowEntry* windowContext;
                std::vector<IndirectDraws> draws;
                FullBuffer indirectDrawBuffer;
                TprViewport viewport;
                TprScissor scissor;

                RenderTargetEntry() = default;
                RenderTargetEntry(
                    std::weak_ptr<DepthDomainEntry> depthDomain, WindowEntry* windowContext, FullBuffer&& indirectDrawBuffer,
                    TprViewport viewport, TprScissor scissor
                ) : depthDomain(depthDomain), windowContext(windowContext), indirectDrawBuffer(std::move(indirectDrawBuffer)),
                    viewport(viewport), scissor(scissor) {}
            };

            struct RenderTargetHandle {
                TprRenderTargetCapabilityFlags capability = std::numeric_limits<TprRenderTargetCapabilityFlags>::max();
                std::shared_ptr<RenderTargetEntry> entry;
            };

            struct EntityImageEntry {
                MeshEntry* mesh;
            };

            struct EntityImageHandle {
                TprEntityImageCapabilityFlags capability = std::numeric_limits<TprEntityImageCapabilityFlags>::max();
                std::shared_ptr<EntityImageEntry> entry;
            };

            struct ChunkConfig {
                MeshEntry* mesh;
                std::weak_ptr<RenderTargetSetEntry> renderTargetSet;
                struct hash {
                    size_t operator()(const ChunkConfig& conf) const {
                        if constexpr (sizeof(size_t) == sizeof(XXH64_hash_t)) {
                            thread_local static xxhash64_holder state;
                            XXH64_reset(state.state(), 0);
                            auto meshHash = std::hash<MeshEntry*>{}(conf.mesh);
                            XXH64_update(state.state(), &meshHash, sizeof(meshHash));
                            auto renderTargetSetHash = std::hash<RenderTargetSetEntry*>{}(conf.renderTargetSet.lock().get());
                            XXH64_update(state.state(), &renderTargetSetHash, sizeof(renderTargetSetHash));
                            return XXH64_digest(state.state());

                        } else if constexpr (sizeof(size_t) == sizeof(XXH32_hash_t)) {
                            thread_local static xxhash32_holder state;
                            XXH32_reset(state.state(), 0);
                            auto meshHash = std::hash<MeshEntry*>{}(conf.mesh);
                            XXH32_update(state.state(), &meshHash, sizeof(meshHash));
                            auto renderTargetSetHash = std::hash<RenderTargetSetEntry*>{}(conf.renderTargetSet.lock().get());
                            XXH32_update(state.state(), &renderTargetSetHash, sizeof(renderTargetSetHash));
                            return XXH32_digest(state.state());
                            
                        } else {
                            throw "Unsupported architecture";
                        }
                    }
                };
                bool operator==(const ChunkConfig& other) const noexcept {
                    return mesh == other.mesh && renderTargetSet.lock() == other.renderTargetSet.lock();
                }
            };

            struct EntityData {
                glm::mat4 transform;
                static constexpr size_t perfectPackSize = sizeof(packed_mat4::value_type) * packed_mat4::size;
                bool operator==(const EntityData& other) const noexcept = default;
            };

            struct EntityConfig {
                ChunkConfig chunk;
                EntityData data;
                struct hash {
                    size_t operator()(const EntityConfig& conf) const {
                        if constexpr (sizeof(size_t) == sizeof(XXH64_hash_t)) {
                            thread_local static xxhash64_holder state;
                            XXH64_reset(state.state(), 0);
                            auto meshHash = std::hash<MeshEntry*>{}(conf.chunk.mesh);
                            XXH64_update(state.state(), &meshHash, sizeof(meshHash));
                            auto renderTargetSetHash = std::hash<RenderTargetSetEntry*>{}(conf.chunk.renderTargetSet.lock().get());
                            XXH64_update(state.state(), &renderTargetSetHash, sizeof(renderTargetSetHash));
                            XXH64_update(state.state(), &conf.data.transform[0][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[0][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[0][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[0][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[1][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[1][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[1][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[1][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[2][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[2][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[2][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[2][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[3][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[3][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[3][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH64_update(state.state(), &conf.data.transform[3][3], sizeof(decltype(conf.data.transform)::value_type));
                            return XXH64_digest(state.state());

                        } else if constexpr (sizeof(size_t) == sizeof(XXH32_hash_t)) {
                            thread_local static xxhash32_holder state;
                            XXH32_reset(state.state(), 0);
                            auto meshHash = std::hash<MeshEntry*>{}(conf.chunk.mesh);
                            XXH32_update(state.state(), &meshHash, sizeof(meshHash));
                            auto renderTargetSetHash = std::hash<RenderTargetSetEntry*>{}(conf.chunk.renderTargetSet.lock().get());
                            XXH32_update(state.state(), &renderTargetSetHash, sizeof(renderTargetSetHash));
                            XXH32_update(state.state(), &conf.data.transform[0][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[0][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[0][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[0][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[1][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[1][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[1][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[1][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[2][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[2][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[2][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[2][3], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[3][0], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[3][1], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[3][2], sizeof(decltype(conf.data.transform)::value_type));
                            XXH32_update(state.state(), &conf.data.transform[3][3], sizeof(decltype(conf.data.transform)::value_type));
                            return XXH32_digest(state.state());
                            
                        } else {
                            throw "Unsupported architecture";
                        }
                    }
                };
                bool operator==(const EntityConfig& other) const noexcept = default;
            };

            struct MeshEntry {
                std::shared_ptr<PartialBuffer> indexBuffer;
                interval<uint64_t> indexSpace;
                std::shared_ptr<PartialBuffer> vertexBuffer;
                interval<uint64_t> vertexSpace;
                std::vector<std::weak_ptr<EntityImageEntry>> entityImages;
            };
        #pragma endregion  // structs


        TprResult ensureSwapchain(WindowEntry& ctx);
        void freeSwapchain(WindowEntry& ctx);

        expected<uint32_t, TprResult> findMemoryType(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property);
        expected<Allocation, TprResult> allocateMemory(
            const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        expected<Allocation, TprResult> allocateExclusiveMemory(
            const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        void freeMemory(Allocation allocation, std::source_location loc = std::source_location::current());

        expected<PartialBuffer, TprResult> createPartialBuffer(
            uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        expected<PartialBuffer, TprResult> createExclusivePartialBuffer(
            uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        void freePartialBuffer(PartialBuffer& buffer, std::source_location loc = std::source_location::current());

        expected<FullBuffer, TprResult> createFullBuffer(
            uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        expected<FullBuffer, TprResult> createExclusiveFullBuffer(
            uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            std::source_location loc = std::source_location::current()
        );
        void freeFullBuffer(FullBuffer& buffer, std::source_location loc = std::source_location::current());

        void freeWindowEntry(WindowEntry& ctx);

        TprResult render() noexcept;

        Logger mLogger;
        FileRegistry& mrFileReg;
        Windowing& mrWin;
        Scheduler& mrSched;
        Settings& mrSett;
        SceneGraph& mrScGr;
        AssetStore& mrAstr;
        std::atomic<TprResult>& mrRunResult;
        uint64_t mPackedEngineVersion;

        std::mutex mMutex;
        bool mInitialised = false;

        SymLoader mLoader;

        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        uint32_t mRenderQueueFamily;
        VkQueue mRenderQueue = VK_NULL_HANDLE;
        uint32_t mTransferQueueFamily;
        VkQueue mTransferQueue = VK_NULL_HANDLE;
        VkCommandPool mCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer mImmidiateCopyBuffer = VK_NULL_HANDLE;
        VkFence mImmidiateCopyFence = VK_NULL_HANDLE;
        VkPipelineLayout mBasicPipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout mEntityDataSetLayout = VK_NULL_HANDLE;

        std::vector<VkMemoryType> mAllocMemoryTypes;
        uint32_t mAllocMaxAllocCount;

        uint32_t mMaxFramesInFlight;
        std::vector<Frame> mFrames;

        uint64_t mIndexBufferSize;
        uint64_t mVertexBufferSize;

        TprComponent mComponentRenderable;
        TprFile mFetchRenderableFile;
        uint64_t mRenderLaunchTime;
        TprJob mRenderSignalJob;
        TprJob mRenderJob;

        uint32_t mFrameCounter = 0;

        std::unordered_map<MeshIdentity, MeshEntry> mMeshes;

        std::set<std::weak_ptr<PartialBuffer>, std::owner_less<std::weak_ptr<PartialBuffer>>> mIndexBuffers;
        std::set<std::weak_ptr<PartialBuffer>, std::owner_less<std::weak_ptr<PartialBuffer>>> mVertexBuffers;

        std::unordered_map<WindowIdentity, WindowEntry> mWindowContexts;

        std::unordered_map<uint32_t, DepthDomainHandle> mDepthDomains;
        std::vector<std::weak_ptr<DepthDomainEntry>> mDepthDomainOrder;
        uint32_t mDepthDomainCounter = 0;
        std::unordered_map<uint32_t, RenderTargetHandle> mRenderTargets;
        uint32_t mRenderTargetCounter = 0;
        std::unordered_map<uint32_t, RenderTargetSetHandle> mRenderTargetSets;
        uint32_t mRenderTargetSetCounter = 0;
        std::unordered_map<uint32_t, EntityImageHandle> mEntityImages;
        uint32_t mEntityImageCounter = 0;
};


#endif  // I_GRAPHICS_DEVICE_BACKENDS_VULKAN_BACKEND_HPP_

