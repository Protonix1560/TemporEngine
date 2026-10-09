
#include "filesystem.hpp"
#include "core.hpp"
#include "tempor.h"
#include "log_entry.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <system_error>
#include <variant>


Filesystem::Filesystem(Logger logger, std::atomic<TprResult>& rRunResult) : mLogger(logger), mrRunResult(rRunResult) {}

Filesystem::~Filesystem() {}


expected<TprFile, TprResult> Filesystem::openFile(std::filesystem::path path, TprOpenFileFlags flags) noexcept {
    if (path.empty()) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        std::filesystem::path normPath = std::filesystem::path(path);
        if (
            (flags & TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT) ||
            ((flags & TPR_OPEN_FILE_NEW_IF_NONE_FLAG_BIT) && !std::filesystem::exists(normPath))
        ) {
            errno = 0;
            FILE* file = std::fopen(normPath.c_str(), "wb");
            if (!file) throw std::system_error(errno, std::generic_category(), "fopen failed");
            std::fclose(file);
        }
        const char* modes = (flags & TPR_OPEN_FILE_SYNC_FLAG_BIT) ? "rb+" : "rb";
        FILE* file;
        errno = 0;
        file = std::fopen(normPath.c_str(), modes);
        if (!file) mLogger.error() << "fopen failed: " << std::strerror(errno);
        if (!file) {
            switch (errno) {
                case ENOMEM:
                case EMFILE:
                case EOVERFLOW:
                    return unexpected(TPR_ERROR_OUT_OF_MEMORY);
                case EACCES:
                case EPERM:
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                case EISDIR:
                case ENOENT:
                case ENOTDIR:
                    return unexpected(TPR_ERROR_DOESNT_EXIST);
                default:
                    mLogger.panic() << "Unexpected errno: " << errno;
                    mrRunResult.store(TPR_PANIC);
                    return unexpected(TPR_PANIC);
            }
        }
        FileHandle handle{};
        if (!(flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) handle.capability &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
        handle.entry = std::make_shared<FileEntry>(FileSource{file, flags});
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Created File " << mFileCounter << " for file " << path;
        auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
        mFileCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprFile, TprResult> Filesystem::createMemoryFile(TprCreateMemoryFileFlags flags) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        FileHandle handle{};
        handle.entry = std::make_shared<FileEntry>(MemorySource{});
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Created memory File " << mFileCounter;
        auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
        mFileCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprFile, TprResult> Filesystem::createFileCapability(TprFile file, TprFileCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        FileHandle handle{};
        handle.capability = mask & handleIt->second.capability;
        handle.entry = handleIt->second.entry;
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Created File capability " << mFileCounter << " for File " << get_basic_handle_index(file);
        auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
        mFileCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void Filesystem::closeFile(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return;
        auto source = handleIt->second.entry;
        mFiles.erase(handleIt);
        if (source.use_count() == 1 && std::holds_alternative<OverlaySource>(source->source)) {
            auto& overlay = std::get<OverlaySource>(source->source);
            auto entry = overlay.overlayed.lock();
            if (entry) entry->overlays.erase(source);
        }
        mLogger.debug() << "Destroyed File " << get_basic_handle_index(file);

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}

expected<TprFile, TprResult> Filesystem::forkFile(TprFile file, TprForkFileFlags flags) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& srcEntry = handleIt->second.entry;

        std::shared_ptr<FileEntry> entry;
        std::visit(overload{
            [&entry](FileSource& src) {
                errno = 0;
                if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src.file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                MemorySource source{};
                source.data.resize(len);
                rewind(src.file);
                errno = 0;
                auto n = fread(source.data.data(), 1, len, src.file);
                if (n != len) throw std::system_error(errno, std::generic_category(), "fread failed");
                entry = std::make_shared<FileEntry>(source);
            },
            [&entry](MemorySource& src) {
                MemorySource source{};
                source.data.resize(src.data.size());
                std::ranges::copy(src.data, source.data.begin());
                entry = std::make_shared<FileEntry>(source);
            },
            [&entry, &srcEntry, this](OverlaySource& src) {
                OverlaySource source{};
                source.overlayed = srcEntry;
                std::visit(overload{
                    [&size = source.size](FileSource& src) {
                        errno = 0;
                        if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                        errno = 0;
                        long len = ftell(src.file);
                        if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                        size = len;
                    },
                    [&size = source.size](MemorySource& src) {
                        size = src.data.size();
                    },
                    [&size = source.size](OverlaySource& src) {
                        size = src.size;
                    }
                }, srcEntry->source);
                entry = std::make_shared<FileEntry>(source);
                srcEntry->overlays.insert(entry);
            }
        }, srcEntry->source);

        FileHandle handle{entry};
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Forked to File " << mFileCounter << " from File " << get_basic_handle_index(file);
        auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
        mFileCounter++;
        return h;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprBool8, TprResult> Filesystem::fileJoinable(TprFile file) noexcept {

}

TprResult Filesystem::joinFile(TprFile file) noexcept {

}


TprResult Filesystem::reset(TprFile file, uint64_t pos, uint64_t n) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        if (!std::holds_alternative<OverlaySource>(handle.entry->source)) return TPR_ERROR_INVALID_OPERATION;
        auto& overlay = std::get<OverlaySource>(handle.entry->source);
        if (pos + n >= overlay.size) return TPR_ERROR_OUT_OF_RANGE;
        overlay.data -= interval<size_t>(pos, pos + n);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::resize(TprFile file, uint64_t newSize) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        std::visit(overload{
            [newSize](FileSource& src) {
                #ifdef POSIX
                    int fd = fileno(src.file);
                    errno = 0;
                    if (ftruncate(fd, newSize) == -1) throw std::system_error(errno, std::generic_category(), "ftruncate failed");
                #endif
                #ifdef WINDOWS
                    int fd = _fileno(src.file);
                    errno = 0;
                    if (_chsize(fd, newSize) == -1) throw std::system_error(errno, std::generic_category(), "_chsize failed");
                #endif
            },
            [newSize](MemorySource& src) {
                src.data.resize(newSize);
            },
            [newSize](OverlaySource& src) {
                src.data &= interval<size_t>(0, newSize);
                src.size = newSize;
            }
        }, handle.entry->source);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::read(TprFile file, uint64_t pos, uint64_t n, void* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        uint64_t size;
        std::visit(overload{
            [&size](FileSource& src) {
                errno = 0;
                if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src.file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](MemorySource& src) {
                size = src.data.size();
            },
            [&size](OverlaySource& src) {
                size = src.size;
            }
        }, handle.entry->source);
        if (pos + n > size) return TPR_ERROR_OUT_OF_RANGE;
        std::visit(overload{
            [pData, n, pos](FileSource& src) {
                errno = 0;
                if (fseek(src.file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fread(pData, 1, n, src.file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [pData, n, pos](MemorySource& src) {
                std::memcpy(pData, src.data.data() + pos, n);
            },
            [pData, n, pos](OverlaySource& src) {
                src.data.copy_subset(interval<size_t>(pos, pos + n), reinterpret_cast<std::byte*>(pData));
            }
        }, handle.entry->source);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::write(TprFile file, uint64_t pos, uint64_t n, const void* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        uint64_t size;
        std::visit(overload{
            [&size](FileSource& src) {
                errno = 0;
                if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src.file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](MemorySource& src) {
                size = src.data.size();
            },
            [&size](OverlaySource& src) {
                size = src.size;
            }
        }, handle.entry->source);
        if (pos + n > size) return TPR_ERROR_OUT_OF_RANGE;
        std::visit(overload{
            [pData, n, pos](FileSource& src) {
                errno = 0;
                if (fseek(src.file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src.file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [pData, n, pos](MemorySource& src) {
                std::memcpy(src.data.data() + pos, pData, n);
            },
            [pData, n, pos](OverlaySource& src) {
                #error ITODO
            }
        }, handle.entry->source);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}


expected<TprPathType, TprResult> Filesystem::pathType(std::filesystem::path path) noexcept {
    try {
        std::error_code ec;
        auto canonical = std::filesystem::canonical(path, ec);
        if (ec) return unexpected(TPR_ERROR_DOESNT_EXIST);
        if (std::filesystem::is_directory(canonical)) return TPR_PATH_TYPE_DIRECTORY;
        if (std::filesystem::is_regular_file(canonical)) return TPR_PATH_TYPE_FILE;
        return unexpected(TPR_ERROR_DOESNT_EXIST);
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

TprResult Filesystem::createDirectory(std::filesystem::path path, TprCreateDirectoryFlags flags) noexcept {
    try {
        std::filesystem::create_directory(path);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::touchFile(std::filesystem::path path, TprTouchFileFlags flags) noexcept {
    try {
        errno = 0;
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) throw std::system_error(errno, std::generic_category(), "fopen failed");
        std::fclose(f);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::remove(std::filesystem::path path) noexcept {
    try {
        std::filesystem::remove_all(path);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult Filesystem::move(std::filesystem::path path, std::filesystem::path newPath) noexcept {
    try {
        if (!std::filesystem::exists(path)) return TPR_ERROR_DOESNT_EXIST;
        if (std::filesystem::exists(newPath)) std::filesystem::remove_all(newPath);
        std::filesystem::rename(path, newPath);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}
