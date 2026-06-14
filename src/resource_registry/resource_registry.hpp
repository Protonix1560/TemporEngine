
#ifndef RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
#define RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_


#include "plugin_core.h"
#include "core.hpp"
#include "hash.hpp"

#include <mio/mmap.hpp>
#include <elfio/elfio.hpp>

#include <filesystem>
#include <mutex>
#include <optional>
#include <streambuf>
#include <unordered_map>
#include <vector>
#include <variant>



#ifdef WINDOWS
    #include <malloc.h>
#endif

#ifdef POSIX
    #include <cstdlib>
#endif


template <typename T>
class aligned_allocator {
    public:
        using value_type = T;

        explicit aligned_allocator(size_t align = alignof(T)) : align(align) {
            if (align < alignof(T)) align = alignof(T);
            if (align && ((align & (align - 1)) != 0)) {
                throw std::bad_alloc();
            }
        }

        template <typename U> aligned_allocator(const aligned_allocator<U>& other) : align(other.alignment()) {}

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
        bool operator==(const aligned_allocator& other) const noexcept { return align == other.alignment(); }

    private:
        size_t align;

};



#ifdef POSIX
    #include <sys/stat.h>
#endif

#ifdef WINDOWS
    #include <windows.h>
    #include <winioctl.h>
#endif


class file_cache_key {
    private:
        #ifdef POSIX
            ino_t m_ino;
            dev_t m_dev;
        #endif

        #ifdef WINDOWS
            FILE_ID_128 m_ino;
            DWORDLONG m_dev;
        #endif

        bool m_strong = false;

        std::filesystem::path m_path;

        friend class std::hash<file_cache_key>;

    public:
        file_cache_key() noexcept = default;
        file_cache_key(std::filesystem::path path) noexcept {
            try {
                m_path = std::filesystem::canonical(path);
            } catch (...) {
                try {
                    m_path = std::filesystem::absolute(path);
                } catch (...) {
                    m_path = path;
                }
            }

            #ifdef POSIX
                struct stat st;
                if (stat(m_path.c_str(), &st) == 0) {
                    m_ino = st.st_ino;
                    m_dev = st.st_dev;
                    m_strong = true;
                }
            #endif

            #ifdef WINDOWS
                HANDLE h = CreateFileW(
                    m_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr
                );
                if (h != INVALID_HANDLE_VALUE) {
                    FILE_ID_INFO info;
                    if (GetFileInformationByHandleEx(h, FileIdInfo, &info, sizeof(info))) {
                        m_dev = info.VolumeSerialNumber;
                        m_ino = info.FileId;
                        m_strong = true;
                    }
                    CloseHandle(h);
                }
            #endif
        }

        bool operator==(const file_cache_key& other) const noexcept {
            if (m_strong && other.m_strong) {
                return m_ino == other.m_ino && m_dev == other.m_dev;
            } else if (!m_strong && !other.m_strong) {
                return m_path == other.m_path;
            }
            return false;
        }

        bool operator!=(const file_cache_key& other) const noexcept { return !(*this == other); }
};


template <>
struct std::hash<file_cache_key> {
    size_t operator()(const file_cache_key& f) const {
        XXH64_state_t* state = XXH64_createState();
        XXH64_reset(state, 0);

        XXH64_update(state, &f.m_strong, sizeof(f.m_strong));
        if (f.m_strong) {
            XXH64_update(state, &f.m_ino, sizeof(f.m_ino));
            XXH64_update(state, &f.m_dev, sizeof(f.m_dev));
        } else {
            auto size = f.m_path.native().size();
            XXH64_update(state, &size, sizeof(size));
            XXH64_update(state, f.m_path.c_str(), f.m_path.native().size() * sizeof(std::filesystem::path::value_type));
        }
        
        uint64_t hash = XXH64_digest(state);
        XXH64_freeState(state);
        return hash;
    }
};



template<mio::access_mode AccessMode, typename ByteT>
class basic_mmap_streambuf : public std::streambuf {
    public:
        explicit basic_mmap_streambuf(const mio::basic_mmap<AccessMode, ByteT>& mmap) {
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
template <typename ByteT> using basic_mmap_sink_streambuf = basic_mmap_streambuf<mio::access_mode::write, ByteT>;
template <typename ByteT> using basic_mmap_source_streambuf = basic_mmap_streambuf<mio::access_mode::read, ByteT>;
using mmap_byte_sink_streambuf = basic_mmap_sink_streambuf<std::byte>;
using mmap_byte_source_streambuf = basic_mmap_source_streambuf<std::byte>;

using mmap_byte_sink = mio::basic_mmap_sink<std::byte>;
using mmap_byte_source = mio::basic_mmap_source<std::byte>;


// from "logger.hpp"
class Logger;


struct ResourceBase {
    uint32_t refCount = 1;
};

struct ResourceROFile : public ResourceBase {
    std::optional<mmap_byte_source> mmapSource;
    std::filesystem::path path;
};

struct ResourceRWFile : public ResourceBase {
    std::optional<mmap_byte_sink> mmapSink;
    std::filesystem::path path;
};

struct ResourceData : public ResourceBase {
    std::vector<std::byte, aligned_allocator<std::byte>> data;
    ResourceData(aligned_allocator<std::byte> alloc)
        : data(alloc) {}
};

struct ResourceReference : public ResourceBase {
    std::byte* begin;
    std::byte* end;
};

struct ResourceView : public ResourceBase {
    const std::byte* begin;
    const std::byte* end;
};


struct ResourceHandle {
    uint32_t resource;
    TprResourceCapabilityFlags flags;
    std::vector<uint32_t> childHandles;
};


class ResourceRegistry {

    public:

        ResourceRegistry(Logger& rLogger);
        ~ResourceRegistry() noexcept;

        expected<TprResource, TprResult> openResource(std::filesystem::path filepath, TprOpenPathResourceFlags flags = 0) noexcept;
        expected<TprResource, TprResult> openResource(size_t size, TprOpenEmptyResourceFlags flags = 0, size_t alignment = 1) noexcept;
        expected<TprResource, TprResult> openResource(std::byte* begin, std::byte* end, TprOpenReferenceResourceFlags flags = 0) noexcept;
        expected<TprResource, TprResult> openResource(const std::byte* begin, const std::byte* end, TprOpenViewResourceFlags flags = 0) noexcept;
        expected<TprResource, TprResult> openResource(TprResource protectedResource, TprResourceCapabilityFlags protectFlags, TprOpenCapabilityResourceFlags flags = 0) noexcept;

        void closeResource(TprResource resource) noexcept;

        TprResult resizeResource(TprResource resource, size_t newSize) noexcept;
        expected<uint64_t, TprResult> sizeofResource(TprResource resource) noexcept;
        expected<std::byte*, TprResult> getResourceRawDataPointer(TprResource resource) noexcept;
        expected<const std::byte*, TprResult> getResourceConstPointer(TprResource resource) noexcept;

        expected<std::filesystem::path, TprResult> matchFile(std::filesystem::path path);
        expected<std::filesystem::path, TprResult> matchDir(std::filesystem::path path);

        expected<std::vector<std::filesystem::path>, TprResult> enumDir(std::filesystem::path dirpath, TprEnumDirFlags flags, size_t depth);

    private:

        Logger& mrLogger;

        std::mutex mMutex;

        using ResourceVariant = std::variant<ResourceROFile, ResourceRWFile, ResourceData, ResourceReference, ResourceView>;

        std::unordered_map<uint32_t, ResourceVariant> mResources;
        uint32_t mResourceCounter = 0;
        std::unordered_map<uint32_t, ResourceHandle> mHandles;
        uint32_t mHandleCounter = 0;

        std::unordered_map<file_cache_key, uint32_t> mFileResourceCache;

        // TprResult validateHandle(TprResource handle);
        // TprResource getRootResource(TprResource resource);
        // TprProtectResourceFlags accumulateProtectFlags(TprResource resource);

};

REGISTER_TYPE_NAME_S(ResourceRegistry, "RReg");



#endif  // RESOURCE_REGISTRY_RESOURCE_REGISTRY_HPP_
