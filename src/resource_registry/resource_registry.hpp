
#ifndef RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
#define RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_


#include "plugin_core.h"
#include "core.hpp"

#include <mio/mmap.hpp>
#include <elfio/elfio.hpp>

#include <filesystem>
#include <streambuf>
#include <unordered_map>
#include <vector>
#include <variant>



#if defined(_WIN32)
    #include <malloc.h>

#elif defined(_POSIX_VERSION)
    #include <cstdlib>
#endif


template <typename T>
class AlignedAllocator {
    public:
        using value_type = T;

        explicit AlignedAllocator(size_t align = alignof(T)) : align(align) {
            if (align < alignof(T)) align = alignof(T);
            if (align && ((align & (align - 1)) != 0)) {
                throw std::bad_alloc();
            }
        }

        template <typename U> AlignedAllocator(const AlignedAllocator<U>& other) : align(other.alignment()) {}

        T* allocate(size_t n) {
            if (n == 0) return nullptr;
            void* ptr = nullptr;

            #if defined(_WIN32)
                if (align < 2) {
                    ptr = malloc(n * sizeof(T));
                } else {
                    ptr = _aligned_malloc(n * sizeof(T), align);
                }

            #elif defined(_POSIX_VERSION)
                if (align < alignof(max_align_t)) {
                    ptr = malloc(n * sizeof(T));
                } else {
                    if (posix_memalign(&ptr, align, n * sizeof(T)) != 0) {
                        ptr = nullptr;
                    }
                }
            #endif

            if (!ptr) throw std::bad_alloc();
            return static_cast<T*>(ptr);
        }

        void deallocate(T* p, size_t n) noexcept {
            if (!p) return;
            #if defined(_WIN32)
                if (align < 2) {
                    free(p);
                } else {
                    _aligned_free(p);
                }
            #elif defined(_POSIX_VERSION)
                free(p);
            #endif
        }

        size_t alignment() const noexcept { return align; }
        bool operator==(const AlignedAllocator& other) const noexcept { return align == other.alignment(); }

    private:
        size_t align;

};




template<mio::access_mode AccessMode, typename ByteT>
class BasicMMapStreambuf : public std::streambuf {
    public:
        explicit BasicMMapStreambuf(const mio::basic_mmap<AccessMode, ByteT>& mmap) {
            char* begin = const_cast<char*>(reinterpret_cast<const char*>(mmap.data()));
            setg(begin, begin, begin + mmap.size());
        }
    protected:
        int_type overflow(int_type) override {
            return traits_type::eof();
        }
        pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which) override {

            if (!(which & std::ios_base::in)) {
                return pos_type(off_type(-1));
            }

            char* newpos = nullptr;

            if (dir == std::ios_base::beg) {
                newpos = eback() + off;
            } else if (dir == std::ios_base::cur) {
                newpos = gptr() + off;
            } else if (dir == std::ios_base::end) {
                newpos = egptr() + off;
            }

            if (newpos < eback() || newpos > egptr()) {
                return pos_type(off_type(-1));
            }

            setg(eback(), newpos, egptr());
            return pos_type(newpos - eback());
        }

        pos_type seekpos(pos_type pos, std::ios_base::openmode which) override {
            return seekoff(off_type(pos), std::ios_base::beg, which);
        }
};
template <typename ByteT> using BasicMMapSinkStreambuf = BasicMMapStreambuf<mio::access_mode::write, ByteT>;
template <typename ByteT> using BasicMMapSourceStreambuf = BasicMMapStreambuf<mio::access_mode::read, ByteT>;
using MMapSinkStreambuf = BasicMMapSinkStreambuf<std::byte>;
using MMapSourceStreambuf = BasicMMapSourceStreambuf<std::byte>;

using MMapSink = mio::basic_mmap_sink<std::byte>;
using MMapSource = mio::basic_mmap_source<std::byte>;



// from "logger.hpp"
class Logger;



class ResourceRegistry {

    public:

        ResourceRegistry(Logger& rLogger);
        ~ResourceRegistry() noexcept;

        void update();

        expected<TprResource, TprResult> openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags = 0, size_t alignment = 1);
        expected<TprResource, TprResult> openResource(size_t size, TprOpenEmptyResourceFlags flags = 0, size_t alignment = 1);
        expected<TprResource, TprResult> openResource(std::byte* begin, std::byte* end, TprOpenReferenceResourceFlags flags = 0, size_t alignment = 1);
        expected<TprResource, TprResult> openResource(TprResource protectedResource, TprProtectResourceFlags protectFlags, TprOpenCapabilityResourceFlags flags = 0);

        TprResult resetResourceLifetime(TprResource resource, const TprLifetime* lifetime);
        TprResult resizeResource(TprResource resource, size_t newSize);
        expected<uint64_t, TprResult> sizeofResource(TprResource resource);
        expected<std::byte*, TprResult> getResourceRawDataPointer(TprResource resource);
        expected<const std::byte*, TprResult> getResourceConstPointer(TprResource resource);
        void closeResource(TprResource resource) noexcept;

        [[deprecated("breaks the architecture, no replacement yet")]] std::filesystem::path getResourceFilepath(TprResource resource);

        std::vector<std::filesystem::path> enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t depth);

    private:

        Logger& mrLogger;

        struct ResourceBase {
            std::vector<TprResource> capabilityResources;
        };

        struct ResourceROFile : public ResourceBase {
            MMapSource mmapSource;
            std::filesystem::path path;
        };

        struct ResourceRWFile : public ResourceBase {
            MMapSink mmapSink;
            std::filesystem::path path;
        };

        struct ResourceData : public ResourceBase {
            std::vector<std::byte, AlignedAllocator<std::byte>> data;
            ResourceData(AlignedAllocator<std::byte> alloc)
                : data(alloc) {}
        };

        struct ResourceReference : public ResourceBase {
            std::byte* begin;
            std::byte* end;
        };

        struct ResourceCapability : public ResourceBase {
            TprResource resource;
            TprProtectResourceFlags protectFlags;
        };

        std::unordered_map<uint32_t, std::variant<ResourceROFile, ResourceRWFile, ResourceData, ResourceReference, ResourceCapability>> mResources;
        uint32_t mResourceCounter = 0;
        size_t mAllocationGranularity;

        TprResult validateHandle(TprResource handle);
        TprResource getRootResource(TprResource resource);
        TprProtectResourceFlags accumulateProtectFlags(TprResource resource);

};

REGISTER_TYPE_NAME(ResourceRegistry);



#endif  // RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
