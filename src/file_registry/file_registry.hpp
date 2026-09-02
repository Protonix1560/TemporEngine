
#ifndef FILE_REGISTRY_FILE_REGISTRY_HPP_
#define FILE_REGISTRY_FILE_REGISTRY_HPP_


#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
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


#ifdef POSIX
    #include <sys/stat.h>
#endif

#ifdef WINDOWS
    #include <windows.h>
#endif

struct file_identity {
    private:
        FILE* m_file;
        #if defined(POSIX)
            ino_t m_ino;
            dev_t m_dev;
        #elif defined(WINDOWS)
            DWORD m_id_low;
            DWORD m_id_high;
            DWORD m_volume;
        #endif

        friend class std::hash<file_identity>;

    public:
        file_identity(FILE* file) : m_file(file) {
            #ifdef POSIX
                int fd = fileno(m_file);
                errno = 0;
                struct stat st;
                if (fstat(fd, &st) == -1) throw std::system_error(errno, std::generic_category(), "fileno failed");
                m_ino = st.st_ino;
                m_dev = st.st_dev;
            #endif
            #ifdef WINDOWS
                HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(m_file)));
                if (handle == INVALID_HANDLE_VALUE) {
                    DWORD err = GetLastError();
                    throw std::system_error(static_cast<int>(err), std::system_category(), "Cast to HANDLE failed");
                }
                BY_HANDLE_FILE_INFORMATION info;
                if (!GetFileInformationByHandle(m_file, &info)) {
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


struct File {};

struct FileSource : File {
    FILE* file = nullptr;
    TprOpenFileFlags flags;
    FileSource(FILE* file, TprOpenFileFlags flags) : file(file), flags(flags) {}
    ~FileSource() {
        if (file) std::fclose(file);
    }
};

struct MemorySource : File {
    std::vector<std::byte> data;
};

using SharedSource = std::variant<std::shared_ptr<FileSource>, std::shared_ptr<MemorySource>>;

struct FileHandle {
    uint64_t pos = 0;
    TprFileCapabilityFlags mask = std::numeric_limits<TprFileCapabilityFlags>::max();
    SharedSource source;
};


class FileRegistry {
    public:
        FileRegistry(Logger logger, std::atomic<TprResult>& rRunResult);
        ~FileRegistry();

        expected<TprFile, TprResult> openFile(std::filesystem::path path, TprOpenFileFlags flags = 0) noexcept;
        expected<TprFile, TprResult> createMemoryFile() noexcept;
        expected<TprFile, TprResult> forkFile(TprFile file) noexcept;
        expected<TprFile, TprResult> createFileCapability(TprFile file, TprFileCapabilityFlags mask) noexcept;
        void closeFile(TprFile file) noexcept;

        TprResult seek(TprFile file, int64_t offset, TprSeekWhence whence) noexcept;
        expected<uint64_t, TprResult> tell(TprFile file) noexcept;
        TprResult read(TprFile file, uint64_t n, std::byte* pData) noexcept;
        TprResult readAt(TprFile file, uint64_t pos, uint64_t n, std::byte* pData) noexcept;
        TprResult resize(TprFile file, uint64_t newSize) noexcept;
        TprResult write(TprFile file, uint64_t n, const std::byte* pData) noexcept;
        TprResult writeAt(TprFile file, uint64_t pos, uint64_t n, const std::byte* pData) noexcept;
        TprResult append(TprFile file, uint64_t n, const std::byte* pData) noexcept;

        expected<TprPathType, TprResult> pathType(std::filesystem::path path) noexcept;
        TprResult createDirectory(std::filesystem::path path, TprCreateDirectoryFlags flags = 0) noexcept;
        TprResult touchFile(std::filesystem::path path, TprTouchFileFlags flags = 0) noexcept;
        TprResult remove(std::filesystem::path path) noexcept;
        TprResult move(std::filesystem::path path, std::filesystem::path newPath) noexcept;

    private:
        Logger mLogger;
        std::atomic<TprResult>& mrRunResult;

        std::mutex mMutex;

        std::unordered_map<uint32_t, FileHandle> mFiles;
        std::unordered_map<file_identity, std::shared_ptr<FileSource>> mFileSources;

        uint32_t mFileCounter = 0;
};

REGISTER_TYPE_NAME_S(FileRegistry, "FReg");


#endif  // FILE_REGISTRY_FILE_REGISTRY_HPP_
