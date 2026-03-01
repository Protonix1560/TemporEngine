
#include "resource_registry.hpp"
#include "core.hpp"
#include "mio/mmap.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <variant>



#if defined(_WIN32)
    #include <windows.h>
#endif

#if defined(__linux__)
    #include <elf.h>
#endif




ResourceRegistry::ResourceRegistry(Logger& rLogger)
    : mrLogger(rLogger) {

    #if defined(WINDOWS)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        mAllocationGranularity = si.dwAllocationGranularity;
    #elif defined(POSIX)
        mAllocationGranularity = getpagesize();
    #endif
    mrLogger.trace() << logPrxRReg() << "Memory mapped file allocation is aligned by " << mAllocationGranularity << " bytes\n";

}


ResourceRegistry::~ResourceRegistry() noexcept {

}


void ResourceRegistry::update() {

}




TprResult ResourceRegistry::validateHandle(TprResource handle) {
    if (get_basic_handle_type(handle) != handle_type::resource) {
        return TPR_INVALID_VALUE;
    }
    if (mResources.find(get_basic_handle_index(handle)) == mResources.end()) {
        return TPR_INVALID_VALUE;
    }
    return TPR_SUCCESS;
}


TprResource ResourceRegistry::getRootResource(TprResource resource) {
    while (std::holds_alternative<ResourceCapability>(mResources[get_basic_handle_index(resource)])) {
        resource = std::get<ResourceCapability>(mResources[get_basic_handle_index(resource)]).resource;
    }
    return resource;
}


TprProtectResourceFlags ResourceRegistry::accumulateProtectFlags(TprResource resource) {

    if (!std::holds_alternative<ResourceCapability>(mResources[get_basic_handle_index(resource)])) return std::numeric_limits<TprProtectResourceFlags>::max();

    TprProtectResourceFlags protectFlags = std::get<ResourceCapability>(mResources[get_basic_handle_index(resource)]).protectFlags;
    while (std::holds_alternative<ResourceCapability>(mResources[get_basic_handle_index(resource)])) {
        ResourceCapability& cap = std::get<ResourceCapability>(mResources[get_basic_handle_index(resource)]);
        protectFlags = protectFlags & cap.protectFlags;
        resource = cap.resource;
    }
    return protectFlags;
}


void ResourceRegistry::closeResource(TprResource handle) noexcept {

    try {

        TprResult validateResult = validateHandle(handle);
        if (validateResult < 0) return;

        TprProtectResourceFlags flags = accumulateProtectFlags(handle);

        std::vector<TprResource> stack;

        if (flags & TPR_PROTECT_RESOURCE_DESTROY_FLAG_BIT) {
            ResourceBase& resource = std::visit(
                [](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); },
                mResources.at(get_basic_handle_index(handle))
            );
            stack.push_back(getRootResource(handle));

        } else {
            std::visit(overload{
                [this, handle](ResourceCapability& res) {
                    auto& caps = std::visit(
                        [](auto& r) -> ResourceBase& { return static_cast<ResourceBase&>(r); },
                        mResources.at(get_basic_handle_index(res.resource))
                    ).capabilityResources;
                    caps.erase(std::find_if(caps.begin(), caps.end(), [handle](const TprResource r) {
                        return r._d == handle._d;
                    }));
                },
                [](auto& res) {}
            }, mResources.at(get_basic_handle_index(handle)));
            stack.push_back(handle);
        }

        while (!stack.empty()) {
            TprResource h = stack.back();
            stack.pop_back();
            ResourceBase& res = std::visit(
                [](auto& r) -> ResourceBase& { return static_cast<ResourceBase&>(r); },
                mResources.at(get_basic_handle_index(h))
            );
            stack.insert(stack.end(), res.capabilityResources.begin(), res.capabilityResources.end());
            mResources.erase(get_basic_handle_index(h));
        }

    } catch (...) {}
}




expected<TprResource, TprResult> ResourceRegistry::openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags, size_t alignment) {

    if (alignment == 0) return unexpected(TPR_CONTRACT_VIOLATION);
    if (alignment && ((alignment & (alignment - 1)) != 0)) return unexpected(TPR_CONTRACT_VIOLATION);
    if (mAllocationGranularity % alignment != 0) return unexpected(TPR_BAD_ALLOC);

    size_t index = mResourceCounter;
    mResources.try_emplace(index);
    mResourceCounter++;

    TprResource handle;

    try {

        // populating resource
        if (flags & TPR_OPEN_PATH_RESOURCE_SYNC_FLAG_BIT) {
            mResources.at(index) = ResourceRWFile();
        } else {
            mResources.at(index) = ResourceROFile();
        }
        ResourceBase& resource = std::visit(
            [](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); },
            mResources.at(index)
        );
        std::visit(overload{
            [filepath](ResourceROFile& resource) -> void {
                resource.mmapSource = MMapSource(filepath.string());
                resource.path = filepath;
            },
            [filepath](ResourceRWFile& resource) -> void {
                resource.mmapSink = MMapSink(filepath.string());
                resource.path = filepath;
            },
            [](auto& resource) -> void {}
        }, mResources.at(index));

        handle = construct_basic_handle<TprResource>(index, 0, handle_type::resource);

    } catch (const Exception& e) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << "[" << e.code() << "]: " << e.what() << "\n";
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);

    } catch (const std::runtime_error& e) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << e.what() << "\n";
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);

    } catch (...) {
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


expected<TprResource, TprResult> ResourceRegistry::openResource(size_t size, TprOpenEmptyResourceFlags flags, size_t alignment) {
    
    if (alignment == 0) return unexpected(TPR_CONTRACT_VIOLATION);
    if (alignment && ((alignment & (alignment - 1)) != 0)) return unexpected(TPR_CONTRACT_VIOLATION);

    size_t index = mResourceCounter;
    mResources.try_emplace(index);
    mResourceCounter++;

    TprResource handle;

    try {

        mResources.at(index) = ResourceData(AlignedAllocator<std::byte>(alignment));
        ResourceData& resource = std::get<ResourceData>(mResources.at(index));
        resource.data.reserve(size);
        resource.data.resize(size);
        if (flags & TPR_OPEN_EMPTY_RESOURCE_ZEROED_FLAG_BIT) {
            std::memset(resource.data.data(), 0, resource.data.size() * sizeof(decltype(resource.data)::value_type));
        }

        handle = construct_basic_handle<TprResource>(index, 0, handle_type::resource);

    } catch (...) {
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


expected<TprResource, TprResult> ResourceRegistry::openResource(std::byte* begin, std::byte* end, TprOpenReferenceResourceFlags flags, size_t alignment) {

    if (alignment == 0) return unexpected(TPR_CONTRACT_VIOLATION);
    if (alignment && ((alignment & (alignment - 1)) != 0)) return unexpected(TPR_CONTRACT_VIOLATION);
    if (!begin) return unexpected(TPR_CONTRACT_VIOLATION);
    if (!end) return unexpected(TPR_CONTRACT_VIOLATION);

    size_t index = mResourceCounter;
    mResources.try_emplace(index);
    mResourceCounter++;

    TprResource handle;

    try {
        // populating resource
        if (flags & TPR_OPEN_REFERENCE_RESOURCE_DONT_COPY_FLAG_BIT) {
            if (reinterpret_cast<uintptr_t>(begin) % alignment != 0) return unexpected(TPR_BAD_ALLOC);
            mResources.at(index) = ResourceReference{};
        } else {
            mResources.at(index) = ResourceData(AlignedAllocator<std::byte>(alignment));
        }
        ResourceBase& resource = std::visit(
            [](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); },
            mResources.at(index)
        );
        std::visit(overload{
            [begin, end](ResourceReference& resource) -> void {
                resource.begin = begin;
                resource.end = end;
            },
            [begin, end](ResourceData& resource) -> void {
                size_t size = end - begin;
                resource.data.reserve(size);
                resource.data.resize(size);
                std::memcpy(resource.data.data(), begin, size);
            },
            [](auto& resource) -> void {}

        }, mResources.at(index));

        handle = construct_basic_handle<TprResource>(index, 0, handle_type::resource);

    } catch (...) {
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    return handle;
}


expected<TprResource, TprResult> ResourceRegistry::openResource(TprResource protectedResourceHandle, TprProtectResourceFlags protectFlags, TprOpenCapabilityResourceFlags flags) {

    TprResult validateResult = validateHandle(protectedResourceHandle);
    if (validateResult < 0) return unexpected(validateResult);

    size_t index = mResourceCounter;
    mResources.try_emplace(index);
    mResourceCounter++;

    TprResource handle;

    try {

        mResources.at(index) = ResourceCapability{};
        ResourceCapability& resource = std::get<ResourceCapability>(mResources.at(index));
        resource.resource = protectedResourceHandle;
        resource.protectFlags = protectFlags;

        handle = construct_basic_handle<TprResource>(index, 0, handle_type::resource);

    } catch (...) {
        mResources.erase(index);
        return unexpected(TPR_UNKNOWN_ERROR);
    }

    ResourceBase& protectedResource = std::visit(
        [](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); },
        mResources.at(get_basic_handle_index(protectedResourceHandle))
    );
    protectedResource.capabilityResources.push_back(handle);
    
    return handle;
}




expected<uint64_t, TprResult> ResourceRegistry::sizeofResource(TprResource handle) {

    TprResult validateResult = validateHandle(handle);
    if (validateResult < 0) return unexpected(validateResult);

    TprResource resource = getRootResource(handle);

    uint64_t size;

    TprResult visitResult = std::visit(overload{

        [&size](ResourceData& resource) -> TprResult {
            size = resource.data.size();
            return TPR_SUCCESS;
        },

        [&size](ResourceReference& resource) -> TprResult {
            size = resource.end - resource.begin;
            return TPR_SUCCESS;
        },

        [&size](ResourceROFile& resource) -> TprResult {
            size = resource.mmapSource.size();
            return TPR_SUCCESS;
        },

        [&size](ResourceRWFile& resource) -> TprResult {
            size = resource.mmapSink.size();
            return TPR_SUCCESS;
        },

        [handle](ResourceCapability& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        }

    }, mResources[get_basic_handle_index(resource)]);

    if (visitResult < 0) return unexpected(visitResult);

    return size;
}


TprResult ResourceRegistry::resizeResource(TprResource handle, size_t newSize) {

    TprResult validateResult = validateHandle(handle);
    if (validateResult < 0) return validateResult;

    TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
    if (!(protectFlags & TPR_PROTECT_RESOURCE_RESIZE_FLAG_BIT)) {
        return TPR_NOT_PERMITTED;
    }

    TprResult visitResult = std::visit(overload{

        [newSize](ResourceData& resource) -> TprResult {
            resource.data.resize(newSize);
            return TPR_SUCCESS;
        },

        [handle, newSize](ResourceReference& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        },

        [newSize](ResourceROFile& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        },

        [newSize](ResourceRWFile& resource) -> TprResult {
            resource.mmapSink.unmap();
            std::filesystem::resize_file(resource.path, newSize);
            resource.mmapSink = MMapSink(resource.path.string());
            return TPR_SUCCESS;
        },

        [handle](ResourceCapability& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        }

    }, mResources.at(get_basic_handle_index(handle)));

    if (visitResult < 0) return visitResult;

    return TPR_SUCCESS;
}


expected<const std::byte*, TprResult> ResourceRegistry::getResourceConstPointer(TprResource handle) {

    TprResult validateResult = validateHandle(handle);
    if (validateResult < 0) return unexpected(validateResult);

    TprResource resource = getRootResource(handle);

    TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
    if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT)) {
        return unexpected(TPR_NOT_PERMITTED);
    }

    // if (std::holds_alternative<ResourceCapability>(mResources.at(getBasicHandleIndex(handle)))) {
    //     TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
    //     if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT)) {
    //         return Unexpected(TPR_NOT_PERMITTED);
    //     }
    // }

    const std::byte* data;

    TprResult visitResult = std::visit(overload{

        [&data](ResourceData& resource) -> TprResult {
            data = resource.data.data();
            return TPR_SUCCESS;
        },

        [&data](ResourceReference& resource) -> TprResult {
            data = resource.begin;
            return TPR_SUCCESS;
        },

        [&data](ResourceROFile& resource) -> TprResult {
            data = resource.mmapSource.data();
            return TPR_SUCCESS;
        },

        [&data](ResourceRWFile& resource) -> TprResult {
            data = resource.mmapSink.data();
            return TPR_SUCCESS;
        },

        [handle](ResourceCapability& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        }

    }, mResources[get_basic_handle_index(resource)]);

    if (visitResult < 0) return unexpected(visitResult);

    return data;
}


expected<std::byte*, TprResult> ResourceRegistry::getResourceRawDataPointer(TprResource handle) {
    
    TprResult validateResult = validateHandle(handle);
    if (validateResult < 0) return unexpected(validateResult);

    TprResource resource = getRootResource(handle);

    TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
    if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT) || !(protectFlags & TPR_PROTECT_RESOURCE_WRITE_FLAG_BIT)) {
        return unexpected(TPR_NOT_PERMITTED);
    }

    // if (std::holds_alternative<ResourceCapability>(mResources.at(getBasicHandleIndex(handle)))) {
    //     TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
    //     if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT) || !(protectFlags & TPR_PROTECT_RESOURCE_WRITE_FLAG_BIT)) {
    //         return Unexpected(TPR_NOT_PERMITTED);
    //     }
    // }

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

        [handle](ResourceROFile& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        },

        [&data](ResourceRWFile& resource) -> TprResult {
            data = resource.mmapSink.data();
            return TPR_SUCCESS;
        },

        [handle](ResourceCapability& resource) -> TprResult {
            return TPR_NOT_PERMITTED;
        }

    }, mResources[get_basic_handle_index(resource)]);

    if (visitResult < 0) return unexpected(visitResult);

    return data;
}




std::vector<std::filesystem::path> ResourceRegistry::enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t maxDepth) {

    if (!std::filesystem::exists(dirpath)) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + dirpath.string() + " doesn't exist");
    }

    if (!std::filesystem::is_directory(dirpath)) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + dirpath.string() + " is not a directory");
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
                    MMapSourceStreambuf streambuf(mmap);
                    std::istream stream(&streambuf);
                    ELFIO::elfio reader;
                    if (!reader.load(stream)) {
                        mrLogger.warn(TPR_LOG_STYLE_WARN1) << logPrxRReg() + "ELFIO: Failed to process file " << entry.path() << ". Skipping\n";
                        continue;
                    }

                    if (reader.get_type() == ET_EXEC && !(flags & TPR_ENUM_DIR_EXECUTABLES_FLAG_BIT)) continue;
                    if (reader.get_type() != ET_DYN) continue;

                    bool hasInterp = false;
                    for (const auto& seg : reader.segments) {
                        if (seg->get_type() == PT_INTERP) {
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




std::filesystem::path ResourceRegistry::getResourceFilepath(TprResource handle) {

    TprResult validateResult = validateHandle(handle);
    if (validateResult < 0) throw Exception(ErrCode::WrongValueError, "Invalid handle");

    TprResource resource = getRootResource(handle);

    std::filesystem::path path;

    std::visit(overload{
        [&path](ResourceROFile& resource) -> void {
            path = resource.path;
        },

        [&path](ResourceRWFile& resource) -> void {
            path = resource.path;
        },

        [handle](auto& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Getting filepath of resource: "s + std::to_string(handle._d) + " is not allowed");
        }

    }, mResources[get_basic_handle_index(resource)]);

    return path;
}


