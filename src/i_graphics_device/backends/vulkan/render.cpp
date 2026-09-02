
#include "asset_store_common.hpp"
#include "scope_guard.hpp"
#include "core.hpp"
#include "backend.hpp"
#include "scheduler.hpp"
#include "plugin_core.h"
#include "asset_store.hpp"
#include "scene_graph.hpp"
#include "file_registry.hpp"
#include "linalg_packed.hpp"
#include "windowing.hpp"
#include "log_entry.hpp"

#include <unordered_map>
#include <vulkan/vulkan.h>

#include <cassert>
#include <cstdint>
#include <mutex>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>


TprResult VulkanBackend::loadMesh(MeshIdentity id) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);

    if (auto r = mLoader.vkDeviceWaitIdle()(mDevice); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkDeviceWaitIdle failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }

    MeshEntry mesh{};
    auto dataExp = mrAstr.getMesh(id);
    if (!dataExp.has_value()) return dataExp.error();
    auto data = dataExp.value();

    uint64_t indexByteSize = data->header.indexCount * sizeof(uint32_t);
    uint64_t vertexByteSize = data->header.vertexCount * sizeof(Vertex);

    for (auto bufferWeak : mIndexBuffers) {
        auto buffer = bufferWeak.lock();
        if (!buffer) continue;  // must not happen
        for (const auto& mem : buffer->free.data()) {
            if (mem.size() >= indexByteSize && mem.first_included() && !mem.last_included()) {
                mesh.indexSpace = {mem.begin(), mem.begin() + indexByteSize};
                mesh.indexBuffer = buffer;
                break;
            }
        }
    }
    bool newIndexBuffer = false;
    if (!mesh.indexBuffer) {
        newIndexBuffer = true;
        uint64_t size = std::max(indexByteSize, mVertexBufferSize * sizeof(uint32_t));
        auto exp = createExclusivePartialBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (!exp.has_value()) return exp.error();
        mesh.indexBuffer = std::make_shared<PartialBuffer>(std::move(exp.value()));
        mesh.indexBuffer->guard = [this, buffer = mesh.indexBuffer.get(), weak = std::weak_ptr(mesh.indexBuffer)]() {
            mIndexBuffers.erase(weak);
            freePartialBuffer(*buffer);
        };
        mesh.indexSpace = {0, indexByteSize};
    }

    for (auto bufferWeak : mVertexBuffers) {
        auto buffer = bufferWeak.lock();
        if (!buffer) continue;  // must not happen
        for (const auto& mem : buffer->free.data()) {
            if (mem.size() >= vertexByteSize && mem.first_included() && !mem.last_included()) {
                mesh.vertexSpace = {mem.begin(), mem.begin() + vertexByteSize};
                mesh.vertexBuffer = buffer;
                break;
            }
        }
    }
    bool newVertexBuffer = false;
    if (!mesh.vertexBuffer) {
        newVertexBuffer = true;
        uint64_t size = std::max(vertexByteSize, mVertexBufferSize * sizeof(uint32_t));
        auto exp = createExclusivePartialBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (!exp.has_value()) return exp.error();
        mesh.vertexBuffer = std::make_shared<PartialBuffer>(std::move(exp.value()));
        mesh.vertexBuffer->guard = [this, buffer = mesh.vertexBuffer.get(), weak = std::weak_ptr(mesh.vertexBuffer)]() {
            mVertexBuffers.erase(weak);
            freePartialBuffer(*buffer);
        };
        mesh.vertexSpace = {0, vertexByteSize};
    }

    void* indexData = nullptr;
    if (auto r = mLoader.vkMapMemory()(mDevice, mesh.indexBuffer->alloc.memory, 0, VK_WHOLE_SIZE, 0, &indexData); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkMapMemory failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    for (uint32_t i = 0; i < data->header.indexCount; i++) {
        MeshData::Index src;
        std::memcpy(&src, data->data.data() + data->header.indexOffset + i * sizeof(src), sizeof(src));
        uint32_t idx = static_cast<uint32_t>(src);
        std::memcpy(reinterpret_cast<std::byte*>(indexData) + mesh.indexSpace.begin() + i * sizeof(idx), &idx, sizeof(idx));
    }
    if (indexData) mLoader.vkUnmapMemory()(mDevice, mesh.indexBuffer->alloc.memory);

    void* vertexData = nullptr;
    if (auto r = mLoader.vkMapMemory()(mDevice, mesh.vertexBuffer->alloc.memory, 0, VK_WHOLE_SIZE, 0, &vertexData); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkMapMemory failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    for (uint32_t i = 0; i < data->header.vertexCount; i++) {
        MeshData::Vertex src;
        std::memcpy(&src, data->data.data() + data->header.vertexOffset + i * sizeof(src), sizeof(src));
        packed_vec3 pos{src.x, src.y, src.z};
        std::memcpy(
            reinterpret_cast<std::byte*>(vertexData) + mesh.vertexSpace.begin() + i * Vertex::perfectPackSize,
            pos.data(), packed_vec3::size * sizeof(packed_vec3::value_type)
        );
    }
    if (vertexData) mLoader.vkUnmapMemory()(mDevice, mesh.vertexBuffer->alloc.memory);

    mMeshes.insert_or_assign(id, mesh);
    mesh.indexBuffer->free -= mesh.indexSpace;
    mesh.vertexBuffer->free -= mesh.vertexSpace;
    if (newIndexBuffer) mIndexBuffers.insert(mesh.indexBuffer);
    if (newVertexBuffer) mVertexBuffers.insert(mesh.vertexBuffer);

    mLogger.trace() << "Loaded mesh identity " << id;

    return TPR_SUCCESS;
}

TprResult VulkanBackend::unloadMesh(MeshIdentity id) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    auto it = mMeshes.find(id);
    if (it == mMeshes.end()) return TPR_ERROR_INVALID_VALUE;
    {
        auto& mesh = it->second;
        for (auto imageWeak : mesh.entityImages) {
            auto image = imageWeak.lock();
            if (image) image->mesh = nullptr;
        }
        mesh.indexBuffer->free += mesh.indexSpace;
        if (mesh.indexBuffer->free.data().size() == 1 && mesh.indexBuffer->free.data().back().size() == mesh.indexBuffer->byteSize) {
            mIndexBuffers.erase(mesh.indexBuffer);
        }
        mesh.vertexBuffer->free += mesh.indexSpace;
        if (mesh.vertexBuffer->free.data().size() == 1 && mesh.vertexBuffer->free.data().back().size() == mesh.vertexBuffer->byteSize) {
            mVertexBuffers.erase(mesh.vertexBuffer);
        }
    }
    mMeshes.erase(it);
    return TPR_SUCCESS;
}


TprResult VulkanBackend::render() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);

    auto successExitLambda = [&]() {
        mRenderLaunchTime += 16'666'667;
        if (auto r = mrSched.scheduleJob(mRenderJob, mRenderLaunchTime); r != TPR_SUCCESS) return r;
        if (auto r = mrSched.scheduleJob(mRenderSignalJob, mRenderLaunchTime); r != TPR_SUCCESS) return r;
        return TPR_SUCCESS;
    };

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;
    Frame& frame = mFrames[mFrameCounter];

    std::vector<EntityConfig> entites;
    std::unordered_map<ChunkConfig, uint32_t, ChunkConfig::hash> chunkSizes;
    uint32_t totalSize = 0;
    
    if (auto r = mrScGr.getComponentChunkHandles(mComponentRenderable, mFetchRenderableFile); r != TPR_SUCCESS) return r;
    if (auto r = mrFileReg.seek(mFetchRenderableFile, 0, TPR_SEEK_WHENCE_END); r != TPR_SUCCESS) return r;
    auto chunkCountExp = mrFileReg.tell(mFetchRenderableFile);
    if (!chunkCountExp.has_value()) return chunkCountExp.error();
    for (size_t readPos = 0; readPos < chunkCountExp.value(); readPos += sizeof(TprComponentChunk)) {
        TprComponentChunk renderableChunk;
        if (auto r = mrFileReg.readAt(
            mFetchRenderableFile, readPos, sizeof(TprComponentChunk), reinterpret_cast<std::byte*>(&renderableChunk)
        ); r != TPR_SUCCESS) return r;
        auto sizeExp = mrScGr.getComponentChunkElementCount(renderableChunk);
        if (!sizeExp.has_value() && sizeExp.error() != TPR_ERROR_INVALID_VALUE) return sizeExp.error();
        std::vector<TprComponentRenderable> renderables(mrScGr.getComponentChunkMaxElementCount());
        if (auto r = mrScGr.copyComponentChunkData(
            renderableChunk, 0, 0, reinterpret_cast<char*>(renderables.data())
        ); r != TPR_SUCCESS) {
            if (r == TPR_ERROR_INVALID_VALUE) continue;
            else return r;
        }
        for (size_t i = 0; i < sizeExp.value(); i++) {
            auto& renderable = renderables[i];
            if (get_basic_handle_type(renderable.renderTargetSet) != handle_type::render_target_set) continue;
            auto targetSetIt = mRenderTargetSets.find(get_basic_handle_index(renderable.renderTargetSet));
            if (targetSetIt == mRenderTargetSets.end()) continue;
            auto targetSet = targetSetIt->second.entry;

            if (get_basic_handle_type(renderable.entityImage) != handle_type::entity_image) continue;
            auto entityImageIt = mEntityImages.find(get_basic_handle_index(renderable.entityImage));
            if (entityImageIt == mEntityImages.end()) continue;
            auto entityImage = entityImageIt->second.entry;

            if (!entityImage->mesh) continue;
            auto mesh = entityImage->mesh;

            EntityConfig config{};
            config.chunk.mesh = mesh;
            config.chunk.renderTargetSet = targetSet;
            config.data.transform[0][0] = renderable.transform.x0;
            config.data.transform[0][1] = renderable.transform.y0;
            config.data.transform[0][2] = renderable.transform.z0;
            config.data.transform[0][3] = renderable.transform.w0;
            config.data.transform[1][0] = renderable.transform.x1;
            config.data.transform[1][1] = renderable.transform.y1;
            config.data.transform[1][2] = renderable.transform.z1;
            config.data.transform[1][3] = renderable.transform.w1;
            config.data.transform[2][0] = renderable.transform.x2;
            config.data.transform[2][1] = renderable.transform.y2;
            config.data.transform[2][2] = renderable.transform.z2;
            config.data.transform[2][3] = renderable.transform.w2;
            config.data.transform[3][0] = renderable.transform.x3;
            config.data.transform[3][1] = renderable.transform.y3;
            config.data.transform[3][2] = renderable.transform.z3;
            config.data.transform[3][3] = renderable.transform.w3;
            entites.emplace_back(config);

            chunkSizes[{mesh, targetSet}]++;
            totalSize++;
        }
    }

    if (auto r = mLoader.vkWaitForFences()(mDevice, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkWaitForFences failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }

    frame.buffersHeld.clear();

    if (frame.entityChunksBuffer.byteSize != totalSize * EntityData::perfectPackSize) {
        freeFullBuffer(frame.entityChunksBuffer);
        auto entityChunksBufferExp = createExclusiveFullBuffer(
            totalSize * EntityData::perfectPackSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        if (!entityChunksBufferExp.has_value()) return entityChunksBufferExp.error();
        frame.entityChunksBuffer = std::move(entityChunksBufferExp.value());

        if (frame.entityChunksBuffer.byteSize != 0) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = frame.entityChunksBuffer.buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &bufferInfo;
            write.dstBinding = 0;
            write.dstArrayElement = 0;
            write.dstSet = frame.entityDataSet;
            mLoader.vkUpdateDescriptorSets()(mDevice, 1, &write, 0, nullptr);
        }
    }

    std::unordered_map<ChunkConfig, uint32_t, ChunkConfig::hash> chunkBegins;
    uint32_t currChunkBegin = 0;
    for (const auto& [chunk, size] : chunkSizes) {
        chunkBegins.try_emplace(chunk, currChunkBegin);
        currChunkBegin += size;
    }
    std::unordered_map<std::shared_ptr<RenderTargetEntry>, std::vector<EntityDataRange>> targetsDraws;
    for (const auto& [chunk, begin] : chunkBegins) {
        for (auto& target : chunk.renderTargetSet.lock()->renderTargets) {
            targetsDraws[target.lock()].push_back({chunk.mesh, begin, chunkSizes.at(chunk)});
        }
    }
    for (auto& [target, draws] : targetsDraws) {
        if (target->indirectDrawBuffer.byteSize != draws.size() * sizeof(VkDrawIndexedIndirectCommand)) {
            auto bufferExp = createExclusiveFullBuffer(
                draws.size() * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            if (!bufferExp.has_value()) return bufferExp.error();
            freeFullBuffer(target->indirectDrawBuffer);
            target->indirectDrawBuffer.guard.release();
            target->indirectDrawBuffer = std::move(bufferExp.value());
        }
        if (!draws.empty()) {
            void* map = nullptr;
            if (auto r = mLoader.vkMapMemory()(mDevice, target->indirectDrawBuffer.alloc.memory, 0, VK_WHOLE_SIZE, 0, &map); r != VK_SUCCESS) {
                mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkMapMemory failed [" << r << "]";
                mrRunResult.store(TPR_PANIC);
                return TPR_PANIC;
            }
            target->draws.clear();
            uint32_t i = 0;
            for (const auto& draw : draws) {
                VkDrawIndexedIndirectCommand command{};
                command.firstIndex = draw.mesh->indexSpace.begin() / 4;
                command.indexCount = draw.mesh->indexSpace.size() / 4;
                command.vertexOffset = draw.mesh->vertexSpace.begin() / Vertex::perfectPackSize;
                command.firstInstance = draw.offset;
                command.instanceCount = draw.count;
                memcpy(reinterpret_cast<std::byte*>(map) + i * sizeof(VkDrawIndexedIndirectCommand), &command, sizeof(VkDrawIndexedIndirectCommand));
                target->draws.push_back({draw.mesh, i, 1});
                i++;
            }
            map = nullptr;
            mLoader.vkUnmapMemory()(mDevice, target->indirectDrawBuffer.alloc.memory);
        }
    }
    if (frame.entityChunksBuffer.byteSize != 0) {
        void* map = nullptr;
        scope_guard unmapGuard = [&]() {
            if (map) mLoader.vkUnmapMemory()(mDevice, frame.entityChunksBuffer.alloc.memory);
            map = nullptr;
        };
        if (auto r = mLoader.vkMapMemory()(mDevice, frame.entityChunksBuffer.alloc.memory, 0, VK_WHOLE_SIZE, 0, &map); r != VK_SUCCESS) {
            mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkMapMemory failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        for (const auto& entity : entites) {
            auto& chunkBeginEl = chunkBegins.at(entity.chunk);
            auto begin = reinterpret_cast<std::byte*>(map) + chunkBeginEl * EntityData::perfectPackSize;
            memcpy(begin, packed_mat4(entity.data.transform).data(), sizeof(packed_mat4::value_type) * packed_mat4::size);
            chunkBeginEl++;
        }
    }

    if (auto r = mLoader.vkResetCommandPool()(mDevice, frame.commandPool, 0); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkResetCommandPool failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    VkCommandBufferBeginInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (auto r = mLoader.vkBeginCommandBuffer()(frame.renderCommandBuffer, &renderInfo); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkBeginCommandBuffer failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    for (auto& [id, window] : mWindowContexts) {
        if (auto r = mLoader.vkAcquireNextImageKHR()(
            mDevice, window.swapchain.swapchain, UINT64_MAX, window.imageAvailableSemaphores[mFrameCounter], VK_NULL_HANDLE, &window.swapchain.currentLinkIndex
        ); r != VK_SUCCESS) {
            switch (r) {
                case VK_ERROR_OUT_OF_DATE_KHR: {
                    if (auto r = mLoader.vkDeviceWaitIdle()(mDevice); r != VK_SUCCESS) {
                        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkDeviceWaitIdle failed [" << r << "]";
                        mrRunResult.store(TPR_PANIC);
                        return TPR_PANIC;
                    }
                    ensureSwapchain(window);
                    return successExitLambda();
                }
                case VK_SUBOPTIMAL_KHR: break;
                default:
                    mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkAcquireNextImageKHR failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
            }
        }
        imageAvailableSemaphores.push_back(window.imageAvailableSemaphores[mFrameCounter]);
        renderFinishedSemaphores.push_back(window.swapchain.links[window.swapchain.currentLinkIndex].renderFinishedSemaphore);
        auto windowWidthExp = mrWin.windowPixelWidth(id);
        if (!windowWidthExp.has_value()) return windowWidthExp.error();
        auto windowHeightExp = mrWin.windowPixelHeight(id);
        if (!windowHeightExp.has_value()) return windowHeightExp.error();
        VkClearValue clears[] = {
            {.color = {.float32 = {0.1f, 0.11f, 0.13f, 1.0f}}},
            {.depthStencil = {1.0f}}
        };
        VkRenderPassBeginInfo passInfo{};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passInfo.renderPass = window.renderPass.renderPass;
        passInfo.renderArea.extent = {windowWidthExp.value(), windowHeightExp.value()};
        passInfo.framebuffer = window.swapchain.links[window.swapchain.currentLinkIndex].framebuffer;
        passInfo.clearValueCount = std::size(clears);
        passInfo.pClearValues = clears;
        mLoader.vkCmdBeginRenderPass()(frame.renderCommandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);

        if (frame.entityChunksBuffer.byteSize != 0) {
            mLoader.vkCmdBindDescriptorSets()(
                frame.renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mBasicPipelineLayout,
                0, 1, &frame.entityDataSet, 0, nullptr
            );

            mLoader.vkCmdBindPipeline()(frame.renderCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, window.renderPass.basicPipeline);

            for (const auto& targetWeak : window.renderTargets) {
                auto target = targetWeak.lock();
                if (target) {

                    VkViewport viewport{};
                    viewport.x = target->viewport.x;
                    viewport.y = target->viewport.y;
                    viewport.width = target->viewport.width;
                    viewport.height = target->viewport.height;
                    viewport.minDepth = target->viewport.minDepth;
                    viewport.maxDepth = target->viewport.maxDepth;
                    mLoader.vkCmdSetViewport()(frame.renderCommandBuffer, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset.x = target->scissor.x;
                    scissor.offset.y = target->scissor.y;
                    scissor.extent.width = target->scissor.width;
                    scissor.extent.height = target->scissor.height;
                    mLoader.vkCmdSetScissor()(frame.renderCommandBuffer, 0, 1, &scissor);

                    for (const auto& draw : target->draws) {
                        VkDeviceSize vertexOffset = 0;
                        mLoader.vkCmdBindVertexBuffers()(frame.renderCommandBuffer, 0, 1, &draw.mesh->vertexBuffer->buffer, &vertexOffset);

                        mLoader.vkCmdBindIndexBuffer()(frame.renderCommandBuffer, draw.mesh->indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

                        // mLogger() << draw.offset << ", " << draw.count;

                        mLoader.vkCmdDrawIndexedIndirect()(
                            frame.renderCommandBuffer, target->indirectDrawBuffer.buffer,
                            draw.offset * sizeof(VkDrawIndexedIndirectCommand), draw.count, sizeof(VkDrawIndexedIndirectCommand)
                        );
                    }
                }
            }
        }

        mLoader.vkCmdEndRenderPass()(frame.renderCommandBuffer);
    }
    if (auto r = mLoader.vkEndCommandBuffer()(frame.renderCommandBuffer); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkEndCommandBuffer failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    if (auto r = mLoader.vkResetFences()(mDevice, 1, &frame.inFlightFence); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkResetFences failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    std::vector<VkPipelineStageFlags> waitMasks(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailableSemaphores.size());
    VkSubmitInfo renderSubmit{};
    renderSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    renderSubmit.commandBufferCount = 1;
    renderSubmit.pCommandBuffers = &frame.renderCommandBuffer;
    renderSubmit.signalSemaphoreCount = renderFinishedSemaphores.size();
    renderSubmit.pSignalSemaphores = renderFinishedSemaphores.data();
    renderSubmit.waitSemaphoreCount = imageAvailableSemaphores.size();
    renderSubmit.pWaitSemaphores = imageAvailableSemaphores.data();
    renderSubmit.pWaitDstStageMask = waitMasks.data();
    if (auto r = mLoader.vkQueueSubmit()(mRenderQueue, 1, &renderSubmit, frame.inFlightFence); r != VK_SUCCESS) {
        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkQueueSubmit failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
    for (auto& [id, window] : mWindowContexts) {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &window.swapchain.swapchain;
        presentInfo.pImageIndices = &window.swapchain.currentLinkIndex;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &window.swapchain.links[window.swapchain.currentLinkIndex].renderFinishedSemaphore;
        if (auto r = mLoader.vkQueuePresentKHR()(mRenderQueue, &presentInfo); r != VK_SUCCESS) {
            switch (r) {
                case VK_SUBOPTIMAL_KHR:
                case VK_ERROR_OUT_OF_DATE_KHR:
                    if (auto r = mLoader.vkDeviceWaitIdle()(mDevice); r != VK_SUCCESS) {
                        mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkDeviceWaitIdle failed [" << r << "]";
                        mrRunResult.store(TPR_PANIC);
                        return TPR_PANIC;
                    }
                    ensureSwapchain(window);
                    break;
                default:
                    mLogger.panic() << __FILE__ ": " << __LINE__ << ": vkQueuePresentKHR failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
            }
        }
    }

    return successExitLambda();
}

