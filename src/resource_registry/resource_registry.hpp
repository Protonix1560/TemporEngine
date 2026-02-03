
#ifndef RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
#define RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_


#include "plugin_core.h"

#include <mio/mmap.hpp>
#include <elfio/elfio.hpp>

#include <filesystem>
#include <streambuf>
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
            char* begin = const_cast<char*>(mmap.data());
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
using MMapSinkStreambuf = BasicMMapSinkStreambuf<char>;
using MMapSourceStreambuf = BasicMMapSourceStreambuf<char>;



class ResourceRegistry {

    public:
        void init();
        void update();
        void shutdown() noexcept;

        TprResource openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags = TPR_OPEN_PATH_RESOURCE_READ_FLAG_BIT, size_t alignment = 1, const TprLifetime* lifetime = nullptr);
        TprResource openResource(size_t size, TprOpenEmptyResourceFlags flags = 0, size_t alignment = 1, const TprLifetime* lifetime = nullptr);
        TprResource openResource(std::byte* begin, std::byte* end, TprOpenRefResourceFlags flags = 0, const TprLifetime* lifetime = nullptr);
        TprResource openResource(TprResource protectedResource, TprProtectResourceFlags protectFlags, TprOpenCapabilityResourceFlags flags = 0, const TprLifetime* lifetime = nullptr);

        std::vector<std::filesystem::path> enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t depth);

        void resetResourceLifetime(TprResource resource, const TprLifetime* lifetime);
        void resizeResource(TprResource resource, size_t newSize);
        size_t sizeofResource(TprResource resource);
        std::byte* getResourceRawRWDataPointer(TprResource resource);
        const std::byte* getResourceRawRODataPointer(TprResource resource);
        std::filesystem::path getResourceFilepath(TprResource resource);

        void closeResource(TprResource resource) noexcept;

    private:

        struct ResourceBase {
            TprLifetime lifetime;
            uint32_t generation = 0;
            bool actual = true;
        };

        struct ResourceROFile : public ResourceBase {
            mio::mmap_source mmapSource;
            std::filesystem::path path;
        };

        struct ResourceRWFile : public ResourceBase {
            mio::mmap_sink mmapSink;
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

        std::vector<std::variant<ResourceROFile, ResourceRWFile, ResourceData, ResourceReference, ResourceCapability>> mResources;
        std::vector<size_t> mFreeResources;
        size_t mAllocationGranularity;

        void validateHandle(TprResource handle);
        TprResource getRootResource(TprResource resource);
        TprProtectResourceFlags accumulateProtectFlags(TprResource resource);

};


#endif  // RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
