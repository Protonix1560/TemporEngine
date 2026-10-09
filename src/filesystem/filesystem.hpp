
#ifndef FILE_REGISTRY_FILE_REGISTRY_HPP_
#define FILE_REGISTRY_FILE_REGISTRY_HPP_


#include "core.hpp"
#include "interval_union.hpp"
#include "logger.hpp"
#include "tempor.h"
#include "hash.hpp"

#include <atomic>
#include <limits>
#include <filesystem>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <variant>
#include <mutex>
#include <cstdio>
#include <unordered_set>


#ifdef POSIX
    #include <sys/stat.h>
#endif

#ifdef WINDOWS
    #include <windows.h>
#endif

struct file_identity {
    private:
        #if defined(POSIX)
            ino_t m_ino;
            dev_t m_dev;
        #endif
        #if defined(WINDOWS)
            DWORD m_id_low;
            DWORD m_id_high;
            DWORD m_volume;
        #endif

        friend class std::hash<file_identity>;

    public:
        file_identity(FILE* file) {
            #ifdef POSIX
                int fd = fileno(file);
                errno = 0;
                struct stat st;
                if (fstat(fd, &st) == -1) throw std::system_error(errno, std::generic_category(), "fileno failed");
                m_ino = st.st_ino;
                m_dev = st.st_dev;
            #endif
            #ifdef WINDOWS
                HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
                if (handle == INVALID_HANDLE_VALUE) {
                    DWORD err = GetLastError();
                    throw std::system_error(static_cast<int>(err), std::system_category(), "Cast to HANDLE failed");
                }
                BY_HANDLE_FILE_INFORMATION info;
                if (!GetFileInformationByHandle(handle, &info)) {
                    DWORD err = GetLastError();
                    throw std::system_error(static_cast<int>(err), std::system_category(), "GetFileInformationByHandle failed");
                }
                m_id_low = info.nFileIndexLow;
                m_id_high = info.nFileIndexHigh;
                m_volume = info.dwVolumeSerialNumber;
            #endif
        }

        bool operator==(const file_identity& other) const noexcept {
            #ifdef POSIX
                return m_ino == other.m_ino && m_dev == other.m_dev;
            #endif
            #ifdef WINDOWS
                return m_id_low == other.m_id_low && m_id_high == other.m_id_high && m_volume == other.m_volume;
            #endif
        }
};

template <>
class std::hash<file_identity> {
    public:
        size_t operator()(const file_identity& id) const noexcept {
            if constexpr (sizeof(size_t) == sizeof(XXH64_hash_t)) {
                thread_local static xxhash64_holder state;
                XXH64_reset(state.state(), 0);
                #ifdef POSIX
                    XXH64_update(state.state(), &id.m_ino, sizeof(id.m_ino));
                    XXH64_update(state.state(), &id.m_dev, sizeof(id.m_dev));
                #endif
                #ifdef WINDOWS
                    XXH64_update(state.state(), &id.m_id_high, sizeof(id.m_id_high));
                    XXH64_update(state.state(), &id.m_id_low, sizeof(id.m_id_low));
                    XXH64_update(state.state(), &id.m_volume, sizeof(id.m_volume));
                #endif
                return XXH64_digest(state.state());

            } else if constexpr (sizeof(size_t) == sizeof(XXH32_hash_t)) {
                thread_local static xxhash32_holder state;
                XXH32_reset(state.state(), 0);
                #ifdef POSIX
                    XXH32_update(state.state(), &id.m_ino, sizeof(id.m_ino));
                    XXH32_update(state.state(), &id.m_dev, sizeof(id.m_dev));
                #endif
                #ifdef WINDOWS
                    XXH32_update(state.state(), &id.m_id_high, sizeof(id.m_id_high));
                    XXH32_update(state.state(), &id.m_id_low, sizeof(id.m_id_low));
                    XXH32_update(state.state(), &id.m_volume, sizeof(id.m_volume));
                #endif
                return XXH32_digest(state.state());

            } else {
                throw "Unsupported architecture";
            }
        }
};


class Filesystem {
    public:
        Filesystem(Logger logger, std::atomic<TprResult>& rRunResult);
        ~Filesystem();

        expected<TprFile, TprResult> openFile(std::filesystem::path path, TprOpenFileFlags flags = 0) noexcept;
        expected<TprFile, TprResult> createMemoryFile(TprCreateMemoryFileFlags flags = 0) noexcept;
        expected<TprFile, TprResult> createFileCapability(TprFile file, TprFileCapabilityFlags mask) noexcept;
        void closeFile(TprFile file) noexcept;
        expected<TprFile, TprResult> forkFile(TprFile file, TprForkFileFlags flags = 0) noexcept;
        expected<TprBool8, TprResult> fileJoinable(TprFile file) noexcept;
        TprResult joinFile(TprFile file) noexcept;

        TprResult reset(TprFile file, uint64_t pos, uint64_t n) noexcept;
        TprResult resize(TprFile file, uint64_t newSize) noexcept;
        TprResult read(TprFile file, uint64_t pos, uint64_t n, void* pData) noexcept;
        TprResult write(TprFile file, uint64_t pos, uint64_t n, const void* pData) noexcept;
        expected<uint64_t, TprResult> fileSize(TprFile file) noexcept;

        expected<TprPathType, TprResult> pathType(std::filesystem::path path) noexcept;
        TprResult createDirectory(std::filesystem::path path, TprCreateDirectoryFlags flags = 0) noexcept;
        TprResult touchFile(std::filesystem::path path, TprTouchFileFlags flags = 0) noexcept;
        TprResult remove(std::filesystem::path path) noexcept;
        TprResult move(std::filesystem::path path, std::filesystem::path newPath) noexcept;

    private:
        struct FileEntry;
        struct Source {};

        struct FileSource : Source {
            FILE* file = nullptr;
            TprOpenFileFlags flags;
            FileSource(FILE* file, TprOpenFileFlags flags) : file(file), flags(flags) {}
            ~FileSource() {
                if (file) std::fclose(file);
            }
        };

        struct MemorySource : Source {
            std::vector<std::byte> data;
        };

        struct OverlaySource : Source {
            data_interval_union<std::byte> data;
            size_t size;
            std::weak_ptr<FileEntry> overlayed;
        };

        struct FileEntry {
            std::variant<FileSource, MemorySource, OverlaySource> source;
            std::unordered_set<std::weak_ptr<FileEntry>, std::owner_less<std::weak_ptr<FileEntry>>> overlays;
        };

        struct FileHandle {
            std::shared_ptr<FileEntry> entry;
            TprFileCapabilityFlags capability = std::numeric_limits<TprFileCapabilityFlags>::max();
        };


        Logger mLogger;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;

        std::unordered_map<uint32_t, FileHandle> mFiles;

        uint32_t mFileCounter = 0;
};

REGISTER_TYPE_NAME_S(Filesystem, "FReg");


#endif  // FILE_REGISTRY_FILE_REGISTRY_HPP_
