
#include "resource_registry.hpp"
#include "core.hpp"
#include "mio/mmap.hpp"
#include "plugin_core.h"
#include "logger.hpp"

#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <variant>



#if defined(_WIN32)
    #include <windows.h>
#endif

#if defined(__linux__)
    #include <elf.h>
#endif



void ResourceRegistry::init() {

    #if defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        mAllocationGranularity = si.dwAllocationGranularity;
    #elif defined(_POSIX_VERSION)
        mAllocationGranularity = getpagesize();
    #endif
    gGetServiceLocator()->get<Logger>().trace() << logPrxRReg() + "Memory mapped file allocation is aligned by " << mAllocationGranularity << " bytes\n";

}

void ResourceRegistry::update() {

}

void ResourceRegistry::shutdown() noexcept {

}


TprResource ResourceRegistry::openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags, size_t alignment, const TprLifetime* lifetime) {

    if (alignment == 0) throw std::bad_alloc();
    if (alignment && ((alignment & (alignment - 1)) != 0)) throw std::bad_alloc();
    if (mAllocationGranularity % alignment != 0) throw std::bad_alloc();

    size_t index;
    if (!mFreeResources.empty()) {
        index = mFreeResources.back();
        mFreeResources.pop_back();
    } else {
        index = mResources.size();
        mResources.emplace_back();
    }

    if (flags & TPR_OPEN_PATH_RESOURCE_READ_FLAG_BIT && flags & TPR_OPEN_PATH_RESOURCE_WRITE_FLAG_BIT) {
        mResources[index] = ResourceRWFile{};
    } else if (flags & TPR_OPEN_PATH_RESOURCE_READ_FLAG_BIT) {
        mResources[index] = ResourceROFile{};
    } else if (flags & TPR_OPEN_PATH_RESOURCE_WRITE_FLAG_BIT) {
        throw Exception(ErrCode::NoSupportError, logPrxRReg() + "Opening path resource with write-only access is not supported");
    } else {
        throw Exception(ErrCode::NoSupportError, logPrxRReg() + "Opening path resource without access is not supported");
    }

    ResourceBase& resource = std::visit([](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); }, mResources[index]);
    resource.actual = true;
    resource.generation++;
    if (lifetime) {
        resource.lifetime = *lifetime;
    } else {
        resource.lifetime = {};
    }

    std::visit(overload{

        [filepath](ResourceROFile& resource) -> void {
            resource.mmapSource = mio::mmap_source(filepath.string());
            resource.path = filepath;
        },

        [filepath](ResourceRWFile& resource) -> void {
            resource.mmapSink = mio::mmap_sink(filepath.string());
            resource.path = filepath;
        },

        [](auto& resource) -> void {}

    }, mResources[index]);

    TprResource handle = constructBasicHandle<TprResource>(index, resource.generation, HandleType::Resource);
    return handle;
}


TprResource ResourceRegistry::openResource(size_t size, TprOpenEmptyResourceFlags flags, size_t alignment, const TprLifetime* lifetime) {
    
    if (alignment == 0) throw std::bad_alloc();
    if (alignment && ((alignment & (alignment - 1)) != 0)) throw std::bad_alloc();

    size_t index;
    if (!mFreeResources.empty()) {
        index = mFreeResources.back();
        mFreeResources.pop_back();
    } else {
        index = mResources.size();
        mResources.emplace_back();
    }

    mResources[index] = ResourceData(AlignedAllocator<std::byte>(alignment));
    ResourceData& resource = std::get<ResourceData>(mResources[index]);
    resource.actual = true;
    resource.generation++;
    if (lifetime) {
        resource.lifetime = *lifetime;
    } else {
        resource.lifetime = {};
    }
    resource.data.reserve(size);
    resource.data.resize(size);

    TprResource handle = constructBasicHandle<TprResource>(index, resource.generation, HandleType::Resource);
    return handle;
}


TprResource ResourceRegistry::openResource(std::byte* begin, std::byte* end, TprOpenRefResourceFlags flags, const TprLifetime* lifetime) {

    size_t index;
    if (!mFreeResources.empty()) {
        index = mFreeResources.back();
        mFreeResources.pop_back();
    } else {
        index = mResources.size();
        mResources.emplace_back();
    }

    if (flags & TPR_OPEN_REF_RESOURCE_DONT_COPY_FLAG_BIT) {
        mResources[index] = ResourceReference{};
    } else {
        mResources[index] = ResourceData(AlignedAllocator<std::byte>(1));
    }

    ResourceBase& resource = std::visit([](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); }, mResources[index]);
    resource.actual = true;
    resource.generation++;
    if (lifetime) {
        resource.lifetime = *lifetime;
    } else {
        resource.lifetime = {};
    }

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

    }, mResources[index]);

    TprResource handle = constructBasicHandle<TprResource>(index, resource.generation, HandleType::Resource);
    return handle;
}


TprResource ResourceRegistry::openResource(TprResource protectedResource, TprProtectResourceFlags protectFlags, TprOpenCapabilityResourceFlags flags, const TprLifetime* lifetime) {

    size_t index;
    if (!mFreeResources.empty()) {
        index = mFreeResources.back();
        mFreeResources.pop_back();
    } else {
        index = mResources.size();
        mResources.emplace_back();
    }

    mResources[index] = ResourceCapability{};
    ResourceCapability& resource = std::get<ResourceCapability>(mResources[index]);
    resource.actual = true;
    resource.generation++;
    if (lifetime) {
        resource.lifetime = *lifetime;
    } else {
        resource.lifetime = {};
    }
    resource.resource = protectedResource;
    resource.protectFlags = protectFlags;

    TprResource handle = constructBasicHandle<TprResource>(index, resource.generation, HandleType::Resource);
    return handle;
}


void ResourceRegistry::validateHandle(TprResource handle) {
    if (getBasicHandleType(handle) != HandleType::Resource) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + "Handle type is not Resource");
    }
    if (getBasicHandleIndex(handle) >= mResources.size()) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + "Invalid handle");
    }
    ResourceBase& resource = std::visit([](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); }, mResources[getBasicHandleIndex(handle)]);
    if (!resource.actual) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + "Closed resource");
    }
    if (resource.generation != getBasicHandleGeneration(handle)) {
        throw Exception(ErrCode::WrongValueError, logPrxRReg() + "Closed resource");
    }
}


TprResource ResourceRegistry::getRootResource(TprResource resource) {
    while (std::holds_alternative<ResourceCapability>(mResources[getBasicHandleIndex(resource)])) {
        resource = std::get<ResourceCapability>(mResources[getBasicHandleIndex(resource)]).resource;
    }
    return resource;
}


TprProtectResourceFlags ResourceRegistry::accumulateProtectFlags(TprResource resource) {
    TprProtectResourceFlags protectFlags = std::get<ResourceCapability>(mResources[getBasicHandleIndex(resource)]).protectFlags;
    while (std::holds_alternative<ResourceCapability>(mResources[getBasicHandleIndex(resource)])) {
        ResourceCapability& cap = std::get<ResourceCapability>(mResources[getBasicHandleIndex(resource)]);
        protectFlags = protectFlags & cap.protectFlags;
        resource = cap.resource;
    }
    return protectFlags;
}


void ResourceRegistry::resetResourceLifetime(TprResource handle, const TprLifetime* lifetime) {
    validateHandle(handle);
    ResourceBase& resource = std::visit(
        [](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); }, 
        mResources[getBasicHandleIndex(getRootResource(handle))]
    );
    resource.lifetime = *lifetime;
}


size_t ResourceRegistry::sizeofResource(TprResource handle) {
    validateHandle(handle);
    TprResource resource = getRootResource(handle);

    size_t size;

    std::visit(overload{

        [&size](ResourceData& resource) -> void {
            size = resource.data.size();
        },

        [&size](ResourceReference& resource) -> void {
            size = resource.end - resource.begin;
        },

        [&size](ResourceROFile& resource) -> void {
            size = resource.mmapSource.size();
        },

        [&size](ResourceRWFile& resource) -> void {
            size = resource.mmapSink.size();
        },

        [handle](ResourceCapability& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Sizeof capability resource: "s + std::to_string(handle._d) + " is not allowed");
        }

    }, mResources[getBasicHandleIndex(resource)]);

    return size;
}


void ResourceRegistry::resizeResource(TprResource handle, size_t newSize) {
    validateHandle(handle);
    TprResource resource = getRootResource(handle);

    if (std::holds_alternative<ResourceCapability>(mResources[getBasicHandleIndex(handle)])) {
        TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
        if (sizeofResource(resource) < newSize && !(protectFlags & TPR_PROTECT_RESOURCE_GROW_FLAG_BIT)) {
            throw Exception(ErrCode::AccessError, logPrxRReg() + "Growing resource:"s + std::to_string(handle._d) + " is not permitted"s);
        }
        if (sizeofResource(resource) > newSize && !(protectFlags & TPR_PROTECT_RESOURCE_SHRINK_FLAG_BIT)) {
            throw Exception(ErrCode::AccessError, "Shrinking resource:"s + std::to_string(handle._d) + " is not permitted");
        }
    }

    std::visit(overload{

        [newSize](ResourceData& resource) -> void {
            resource.data.resize(newSize);
        },

        [handle, newSize](ResourceReference& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Resizing reference resource: "s + std::to_string(handle._d) + " is not allowed");
        },

        [newSize](ResourceROFile& resource) -> void {
            resource.mmapSource.unmap();
            std::filesystem::resize_file(resource.path, newSize);
            resource.mmapSource = mio::mmap_source(resource.path.string());
        },

        [newSize](ResourceRWFile& resource) -> void {
            resource.mmapSink.unmap();
            std::filesystem::resize_file(resource.path, newSize);
            resource.mmapSink = mio::mmap_sink(resource.path.string());
        },

        [handle](ResourceCapability& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Resizing capability resource: "s + std::to_string(handle._d) + " is not allowed");
        }

    }, mResources[getBasicHandleIndex(resource)]);

}


const std::byte* ResourceRegistry::getResourceRawRODataPointer(TprResource handle) {
    validateHandle(handle);
    TprResource resource = getRootResource(handle);

    if (std::holds_alternative<ResourceCapability>(mResources[getBasicHandleIndex(handle)])) {
        TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
        if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT)) {
            throw Exception(
                ErrCode::AccessError, logPrxRReg() +
                "Getting raw read-only data pointer of resource:"s + std::to_string(handle._d) + " is not permitted"s
            );
        }
    }

    const std::byte* data;

    std::visit(overload{

        [&data](ResourceData& resource) -> void {
            data = resource.data.data();
        },

        [&data](ResourceReference& resource) -> void {
            data = resource.begin;
        },

        [&data](ResourceROFile& resource) -> void {
            data = reinterpret_cast<const std::byte*>(resource.mmapSource.data());
        },

        [&data](ResourceRWFile& resource) -> void {
            data = reinterpret_cast<const std::byte*>(resource.mmapSink.data());
        },

        [handle](ResourceCapability& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Getting raw read-only data pointer of capability resource: "s + std::to_string(handle._d) + " is not allowed");
        }

    }, mResources[getBasicHandleIndex(resource)]);

    return data;
}


std::byte* ResourceRegistry::getResourceRawRWDataPointer(TprResource handle) {
    validateHandle(handle);
    TprResource resource = getRootResource(handle);

    if (std::holds_alternative<ResourceCapability>(mResources[getBasicHandleIndex(handle)])) {
        TprProtectResourceFlags protectFlags = accumulateProtectFlags(handle);
        if (!(protectFlags & TPR_PROTECT_RESOURCE_READ_FLAG_BIT) || !(protectFlags & TPR_PROTECT_RESOURCE_WRITE_FLAG_BIT)) {
            throw Exception(
                ErrCode::AccessError, logPrxRReg() +
                "Getting raw data pointer of resource:"s + std::to_string(handle._d) + " is not permitted"s
            );
        }
    }

    std::byte* data;

    std::visit(overload{

        [&data](ResourceData& resource) -> void {
            data = resource.data.data();
        },

        [&data](ResourceReference& resource) -> void {
            data = resource.begin;
        },

        [handle](ResourceROFile& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Getting raw read-write data pointer of rofile resource: "s + std::to_string(handle._d) + " is not allowed");
        },

        [&data](ResourceRWFile& resource) -> void {
            data = reinterpret_cast<std::byte*>(resource.mmapSink.data());
        },

        [handle](ResourceCapability& resource) -> void {
            throw Exception(ErrCode::NoSupportError, "Getting raw read-write data pointer of capability resource: "s + std::to_string(handle._d) + " is not allowed");
        }

    }, mResources[getBasicHandleIndex(resource)]);

    return data;
}


std::filesystem::path ResourceRegistry::getResourceFilepath(TprResource handle) {
    validateHandle(handle);
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

    }, mResources[getBasicHandleIndex(resource)]);

    return path;
}


void ResourceRegistry::closeResource(TprResource handle) noexcept {
    try {
        validateHandle(handle);
        ResourceBase& resource = std::visit([](auto& resource) -> ResourceBase& { return static_cast<ResourceBase&>(resource); }, mResources[getBasicHandleIndex(handle)]);
        resource.actual = false;
        mFreeResources.push_back(getBasicHandleIndex(handle));

    } catch (...) {}
}


std::vector<std::filesystem::path> ResourceRegistry::enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t maxDepth) {

    auto& logger = gGetServiceLocator()->get<Logger>();

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
                    stack.emplace_back(depth + 1, std::filesystem::directory_iterator{entry});
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

                    mio::mmap_source mmap(entry.path().string());
                    MMapSourceStreambuf streambuf(mmap);
                    std::istream stream(&streambuf);
                    ELFIO::elfio reader;
                    if (!reader.load(stream)) {
                        logger.warn(TPR_LOG_STYLE_WARN1)
                            << logPrxRReg() + "ELFIO: Failed to process file " << entry.path() << ". Skipping\n";
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


