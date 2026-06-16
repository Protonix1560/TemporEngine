
#include "resource_registry.hpp"
#include "core.hpp"
#include "mio/mmap.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <variant>
#include <cassert>



ResourceRegistry::ResourceRegistry(Logger logger) : mLogger(logger) {}

ResourceRegistry::~ResourceRegistry() noexcept {}



expected<TprResource, TprResult> ResourceRegistry::openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        TprResource h;

        file_cache_key fkey{filepath};

        auto it = mFileResourceCache.find(fkey);
        if (it == mFileResourceCache.end()) {
            if (
                !(flags & TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT) &&
                !(flags & TPR_OPEN_PATH_RESOURCE_CREATE_IF_NONE_FLAG_BIT) &&
                !std::filesystem::exists(filepath) ||
                std::filesystem::exists(filepath) && std::filesystem::is_directory(filepath)
            ) {
                return unexpected(TPR_ERROR_DOESNT_EXIST);
            }

            if ((std::filesystem::status(filepath).permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
                return unexpected(TPR_ERROR_NOT_PERMITTED);
            }

            ResourceVariant resource;

            if ((std::filesystem::status(filepath).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none) {
                resource = ResourceRWFile{};
            } else {
                if (
                    (flags & TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT) ||
                    (flags & TPR_OPEN_PATH_RESOURCE_CREATE_IF_NONE_FLAG_BIT) && !std::filesystem::exists(filepath) ||
                    (flags & TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT)
                ) {
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                }
                resource = ResourceROFile{};
            }

            if (std::filesystem::exists(filepath)) {
                if (flags & TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT) {
                    // checked above, file mustn't be read-only at this point
                    std::filesystem::remove(filepath);
                    std::ofstream f(filepath);
                    f.close();
                }
            } else {
                auto parent = std::filesystem::absolute(filepath).parent_path();
                if ((std::filesystem::status(parent).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none) {
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                }
                if ((flags & TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT) || (flags & TPR_OPEN_PATH_RESOURCE_CREATE_IF_NONE_FLAG_BIT)) {
                    std::ofstream f(filepath);
                    f.close();
                }
            }

            std::visit(overload{
                [filepath](ResourceROFile& resource) -> void {
                    if (std::filesystem::file_size(filepath) > 0) resource.mmapSource = mmap_byte_source(filepath.string());
                    resource.path = filepath;
                },
                [filepath](ResourceRWFile& resource) -> void {
                    if (std::filesystem::file_size(filepath) > 0) resource.mmapSink = mmap_byte_sink(filepath.string());
                    resource.path = filepath;
                },
                [](auto& resource) -> void {
                    // must not happen
                }
            }, resource);

            ResourceHandle handle{};
            handle.resource = mResourceCounter;
            handle.flags = TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT;
            if (flags & TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT) handle.flags |= TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT;

            mResources.try_emplace(mResourceCounter, std::move(resource));
            mHandles.try_emplace(mHandleCounter, handle);
            mLogger.debug()  << "Adding " << filepath << " to the registry\n";
            mFileResourceCache.try_emplace(fkey, mResourceCounter);

            h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
            mResourceCounter++;
            mHandleCounter++;

        } else {
            auto resIt = mResources.find(it->second);
            if (resIt == mResources.end()) {
                mLogger.error(TPR_LOG_STYLE_PANIC1) 
                    << "openFileResource: Corrupted internal structures: id " << it->second << " for file \"" << filepath << "\""
                    << " from mFileResourceCache does not appear in mResources\n";
                return unexpected(TPR_PANIC);
            }
            auto& resource = resIt->second;
            if (flags & TPR_OPEN_PATH_RESOURCE_ALWAYS_NEW_FLAG_BIT) {
                // Someone might already use a pointer to that resource, so just making it empty isn't ideal
                // But it's the only way
                TprResult visitResult = std::visit(overload{
                    [](ResourceROFile& resource) -> TprResult {
                        return TPR_ERROR_NOT_PERMITTED;
                    },
                    [](ResourceRWFile& resource) -> TprResult {
                        if (resource.mmapSink.has_value()) resource.mmapSink->unmap();
                        std::filesystem::resize_file(resource.path, 0);
                        return TPR_SUCCESS;
                    },
                    [](auto& resource) -> TprResult {
                        return TPR_ERROR_NOT_PERMITTED;
                    },
                }, resource);
                if (visitResult != TPR_SUCCESS) return unexpected(visitResult);
            }
            if (std::holds_alternative<ResourceROFile>(resource) && (flags & TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT)) {
                return unexpected(TPR_ERROR_NOT_PERMITTED);
            }
            std::visit([](auto& r) {
                r.refCount++;
            }, resource);

            ResourceHandle handle{};
            handle.resource = it->second;
            handle.flags = TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT;
            if (flags & TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT) handle.flags |= TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT;

            mHandles.try_emplace(mHandleCounter, handle);

            h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
            mHandleCounter++;
        }
        return h;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openFileResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openFileResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<TprResource, TprResult> ResourceRegistry::openResource(size_t size, TprOpenEmptyResourceFlags flags, size_t alignment) noexcept {
    if (alignment == 0) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (alignment && ((alignment & (alignment - 1)) != 0)) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        TprResource h;

        ResourceData resource{aligned_allocator<std::byte>(alignment)};
        resource.data.resize(size);
        if (flags & TPR_OPEN_EMPTY_RESOURCE_ZEROED_FLAG_BIT) {
            std::memset(resource.data.data(), 0, resource.data.size() * sizeof(std::byte));
        }

        ResourceHandle handle{};
        handle.resource = mResourceCounter;
        handle.flags = TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT |
                       TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT | TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT;

        mResources.try_emplace(mResourceCounter, resource);
        mHandles.try_emplace(mHandleCounter, handle);

        h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
        mResourceCounter++;
        mHandleCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openEmptyResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openEmptyResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<TprResource, TprResult> ResourceRegistry::openResource(std::byte* begin, std::byte* end, TprOpenReferenceResourceFlags flags) noexcept {
    if (!begin) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (!end) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        TprResource h;

        ResourceReference resource{};
        resource.begin = begin;
        resource.end = end;

        ResourceHandle handle{};
        handle.resource = mResourceCounter;
        handle.flags = TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT |
                       TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT | TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT;

        mResources.try_emplace(mResourceCounter, resource);
        mHandles.try_emplace(mHandleCounter, handle);

        h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
        mResourceCounter++;
        mHandleCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openReferenceResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openReferenceResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<TprResource, TprResult> ResourceRegistry::openResource(const std::byte* begin, const std::byte* end, TprOpenViewResourceFlags flags) noexcept {
    if (!begin) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (!end) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        TprResource h;

        ResourceView resource{};
        resource.begin = begin;
        resource.end = end;

        ResourceHandle handle{};
        handle.resource = mResourceCounter;
        handle.flags = TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT | TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT |
                       TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT | TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT;

        mResources.try_emplace(mResourceCounter, resource);
        mHandles.try_emplace(mHandleCounter, handle);

        h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
        mResourceCounter++;
        mHandleCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openViewResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openViewResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<TprResource, TprResult> ResourceRegistry::openResource(
    TprResource protectedResource, TprResourceCapabilityFlags capability, TprOpenCapabilityResourceFlags flags
) noexcept {
    if (get_basic_handle_type(protectedResource) != handle_type::resource) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_index(protectedResource) > mResourceCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        TprResource h;

        auto protIt = mHandles.find(get_basic_handle_index(protectedResource));
        if (protIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

        ResourceHandle handle{};
        handle.resource = get_basic_handle_index(protectedResource);
        handle.flags = capability;

        ResourceHandle& protHandle = protIt->second;
        protHandle.childHandles.push_back(mHandleCounter);

        auto protRes = mResources.find(protHandle.resource);
        if (protRes == mResources.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) 
                << "openCapabilityResource: Corrupted internal structures: handle["
                << protIt->first << "].resource[" << protHandle.resource << "] does not appear in mResources\n";
            return unexpected(TPR_PANIC);
        }

        std::visit([](auto& r) { r.refCount++; }, protRes->second);

        h = construct_basic_handle<TprResource>(mHandleCounter, 0, handle_type::resource);
        mHandleCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openCapabilityResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "openCapabilityResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

void ResourceRegistry::closeResource(TprResource h) noexcept {
    if (get_basic_handle_type(h) != handle_type::resource) return;
    if (get_basic_handle_index(h) > mHandleCounter) return;
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = mHandles.find(get_basic_handle_index(h));
        if (it == mHandles.end()) return;

        auto resourceIt = mResources.find(it->second.resource);
        if (resourceIt == mResources.end()) return;  // must not happen

        ResourceBase& resource = std::visit(
            [](auto& r) -> ResourceBase& { return static_cast<ResourceBase&>(r); },
            resourceIt->second
        );

        std::vector<uint32_t> stack = {get_basic_handle_index(h)};

        while (!stack.empty()) {
            uint32_t id = stack.back();
            stack.pop_back();
            auto it = mHandles.find(id);
            if (it != mHandles.end()) {
                // must happen
                stack.insert(stack.end(), it->second.childHandles.begin(), it->second.childHandles.end());
                resource.refCount--;
                mHandles.erase(it);
            }
        }

        if (resource.refCount == 0) {
            std::visit(overload{
                [this](ResourceROFile& r) {
                    mLogger.debug()  << "Removing \"" << r.path.string() << "\" from the registry\n";
                    mFileResourceCache.erase(file_cache_key(r.path));
                },
                [this](ResourceRWFile& r) {
                    mLogger.debug()  << "Removing \"" << r.path.string() << "\" from the registry\n";
                    mFileResourceCache.erase(file_cache_key(r.path));
                },
                [](auto& r) {}
            }, resourceIt->second);
            mResources.erase(resourceIt);
        }

    } catch (...) {}
}

expected<uint64_t, TprResult> ResourceRegistry::sizeofResource(TprResource h) noexcept {
    if (get_basic_handle_type(h) != handle_type::resource) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_index(h) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        auto handleIt = mHandles.find(get_basic_handle_index(h));
        if (handleIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

        auto resIt = mResources.find(handleIt->second.resource);
        if (resIt == mResources.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) 
                << "sizeofResource: Corrupted internal structures: handle["
                << handleIt->first << "].resource[" << handleIt->second.resource << "] does not appear in mResources\n";
            return unexpected(TPR_PANIC);
        }

        uint64_t size;

        std::visit(overload{
            [&size](ResourceData& resource) -> void {
                size = resource.data.size();
            },
            [&size](ResourceReference& resource) -> void {
                size = resource.end - resource.begin;
            },
            [&size](ResourceView& resource) -> void {
                size = resource.end - resource.begin;
            },
            [&size](ResourceROFile& resource) -> void {
                if (resource.mmapSource.has_value()) size = resource.mmapSource->size();
                else size = 0;
            },
            [&size](ResourceRWFile& resource) -> void {
                if (resource.mmapSink.has_value()) size = resource.mmapSink->size();
                else size = 0;
            },
        }, resIt->second);
        return size;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "sizeofResource: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "sizeofResource: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

TprResult ResourceRegistry::resizeResource(TprResource h, size_t newSize) noexcept {
    if (get_basic_handle_type(h) != handle_type::resource) return TPR_ERROR_INVALID_VALUE;
    if (get_basic_handle_index(h) > mHandleCounter) return TPR_ERROR_INVALID_VALUE;
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        auto handleIt = mHandles.find(get_basic_handle_index(h));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;

        if (!(handleIt->second.flags & TPR_RESOURCE_CAPABILITY_RESIZE_FLAG_BIT)) {
            return TPR_ERROR_NOT_PERMITTED;
        }

        auto resIt = mResources.find(handleIt->second.resource);
        if (resIt == mResources.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) 
                << "sizeofResource: Corrupted internal structures: handle["
                << handleIt->first << "].resource[" << handleIt->second.resource << "] does not appear in mResources\n";
            return TPR_PANIC;
        }

        TprResult visitResult = std::visit(overload{
            [newSize](ResourceData& resource) -> TprResult {
                resource.data.resize(newSize);
                return TPR_SUCCESS;
            },
            [newSize](ResourceRWFile& resource) -> TprResult {
                if (resource.mmapSink.has_value()) resource.mmapSink->unmap();
                std::filesystem::resize_file(resource.path, newSize);
                if (newSize > 0) resource.mmapSink = mmap_byte_sink(resource.path.string());
                return TPR_SUCCESS;
            },
            [newSize](ResourceReference& resource) -> TprResult {
                resource.end = resource.begin + newSize;
                return TPR_SUCCESS;
            },
            [newSize](ResourceView& resource) -> TprResult {
                resource.end = resource.begin + newSize;
                return TPR_SUCCESS;
            },
            [](auto& resource) -> TprResult {
                return TPR_ERROR_NOT_PERMITTED;
            },
        }, resIt->second);
        return visitResult;
        
    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "resizeResource: Exception: " << e.what() << "\n";
        return TPR_PANIC;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "resizeResource: Unknown exception\n";
        return TPR_PANIC;
    }
}

expected<const std::byte*, TprResult> ResourceRegistry::getResourceConstPointer(TprResource h) noexcept {
    if (get_basic_handle_type(h) != handle_type::resource) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_index(h) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        auto handleIt = mHandles.find(get_basic_handle_index(h));
        if (handleIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

        if (!(handleIt->second.flags & TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT)) {
            return unexpected(TPR_ERROR_NOT_PERMITTED);
        }

        auto resIt = mResources.find(handleIt->second.resource);
        if (resIt == mResources.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) 
                << "sizeofResource: Corrupted internal structures: handle["
                << handleIt->first << "].resource[" << handleIt->second.resource << "] does not appear in mResources\n";
            return unexpected(TPR_PANIC);
        }

        const std::byte* data;

        std::visit(overload{
            [&data](ResourceData& resource) {
                data = resource.data.data();
            },
            [&data](ResourceReference& resource) {
                data = resource.begin;
            },
            [&data](ResourceView& resource) {
                data = resource.begin;
            },
            [&data](ResourceROFile& resource) {
                if (resource.mmapSource.has_value()) data = resource.mmapSource->data();
                else data = nullptr;
            },
            [&data](ResourceRWFile& resource) {
                if (resource.mmapSink.has_value()) data = resource.mmapSink->data();
                else data = nullptr;
            },
        }, resIt->second);
        return data;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "getResourceConstPointer: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "getResourceConstPointer: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}

expected<std::byte*, TprResult> ResourceRegistry::getResourceRawDataPointer(TprResource h) noexcept {
    if (get_basic_handle_type(h) != handle_type::resource) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (get_basic_handle_index(h) > mHandleCounter) return unexpected(TPR_ERROR_INVALID_VALUE);
    try {
        std::lock_guard<std::mutex> lock(mMutex);

        auto handleIt = mHandles.find(get_basic_handle_index(h));
        if (handleIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);

        if (!(handleIt->second.flags & TPR_RESOURCE_CAPABILITY_READ_FLAG_BIT) || !(handleIt->second.flags & TPR_RESOURCE_CAPABILITY_WRITE_FLAG_BIT)) {
            return unexpected(TPR_ERROR_NOT_PERMITTED);
        }

        auto resIt = mResources.find(handleIt->second.resource);
        if (resIt == mResources.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) 
                << "sizeofResource: Corrupted internal structures: handle["
                << handleIt->first << "].resource[" << handleIt->second.resource << "] does not appear in mResources\n";
            return unexpected(TPR_PANIC);
        }

        std::byte* data;

        TprResult visitResult = std::visit(overload{
            [&data](ResourceData& resource) -> TprResult {
                data = resource.data.data();
                return TPR_SUCCESS;
            },
            [&data](ResourceReference& resource) -> TprResult {
                data = resource.begin;
                return TPR_SUCCESS;
            },
            [](ResourceView& resource) -> TprResult {
                return TPR_ERROR_NOT_PERMITTED;
            },
            [](ResourceROFile& resource) -> TprResult {
                return TPR_ERROR_NOT_PERMITTED;
            },
            [&data](ResourceRWFile& resource) -> TprResult {
                if (resource.mmapSink.has_value()) data = resource.mmapSink->data();
                else data = nullptr;
                return TPR_SUCCESS;
            }
        }, resIt->second);
        if (visitResult != TPR_SUCCESS) return unexpected(visitResult);
        return data;

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "getResourceRawDataPointer: Exception: " << e.what() << "\n";
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_PANIC1)  << "getResourceRawDataPointer: Unknown exception\n";
        return unexpected(TPR_PANIC);
    }
}



expected<std::vector<std::filesystem::path>, TprResult> ResourceRegistry::enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t maxDepth) {

    // TODO: add it to the API

    if (!std::filesystem::exists(dirpath) || !std::filesystem::is_directory(dirpath)) {
        return unexpected(TPR_ERROR_DOESNT_EXIST);
    }

    std::vector<std::filesystem::path> entries;
    std::vector<std::pair<size_t, std::filesystem::directory_iterator>> stack = { {0, std::filesystem::directory_iterator(dirpath)} };

    while (!stack.empty()) {

        auto [depth, dir] = stack.back();
        stack.pop_back();

        for (auto& entry : dir) {

            if (entry.is_directory()) {
                if (flags & TPR_ENUM_DIR_DIRS_FLAG_BIT) {
                    entries.push_back(entry.path());
                }
                if (maxDepth > depth) {
                    stack.emplace_back(depth + 1, std::filesystem::directory_iterator(entry));
                }
                continue;

            }

            if (
                entry.is_regular_file() && (
                    flags & TPR_ENUM_DIR_NORMAL_FILES_FLAG_BIT ||
                    flags & TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT ||
                    flags & TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT
                )
            ) {

                if ((
                        flags & TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT ||
                        flags & TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT
                    ) && std::filesystem::file_size(entry) > 0
                ) {
                    
                    mio::basic_mmap_source<std::byte> mmap(entry.path().string());
                    mmap_byte_source_streambuf streambuf(mmap);
                    std::istream stream(&streambuf);
                    ELFIO::elfio reader;
                    if (!reader.load(stream)) {
                        mLogger.warn(TPR_LOG_STYLE_WARN1) << "ELFIO: Failed to process file " << entry.path() << ". Skipping\n";
                        continue;
                    }

                    if (reader.get_type() == ELFIO::ET_EXEC && !(flags & TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT)) continue;
                    if (reader.get_type() != ELFIO::ET_DYN) continue;

                    bool hasInterp = false;
                    for (const auto& seg : reader.segments) {
                        if (seg->get_type() == ELFIO::PT_INTERP) {
                            hasInterp = true;
                            break;
                        }
                    }

                    if (hasInterp && !(flags & TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT)) continue;
                    if (!hasInterp && !(flags & TPR_ENUM_DIR_RUNTIME_LIBS_FLAG_BIT)) continue;

                    entries.push_back(entry.path());
                    continue;

                }

                if (flags & TPR_ENUM_DIR_NORMAL_FILES_FLAG_BIT) {
                    entries.push_back(entry.path());
                    continue;
                }
            }
        }

    }

    return entries;

}

expected<std::filesystem::path, TprResult> ResourceRegistry::matchFile(std::filesystem::path path) {
    // TODO: search directories and scanning
    if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {
        return path;
    }
    return unexpected(TPR_ERROR_DOESNT_EXIST);
}

expected<std::filesystem::path, TprResult> ResourceRegistry::matchDir(std::filesystem::path path) {
    // TODO: search directories and scanning
    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        return path;
    }
    return unexpected(TPR_ERROR_DOESNT_EXIST);
}

