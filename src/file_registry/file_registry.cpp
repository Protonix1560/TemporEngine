
#include "file_registry.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "log_entry.hpp"

#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <variant>


FileRegistry::FileRegistry(Logger logger, std::atomic<TprResult>& rRunResult) : mLogger(logger), mrRunResult(rRunResult) {}

FileRegistry::~FileRegistry() {}


expected<TprFile, TprResult> FileRegistry::openFile(std::filesystem::path path, TprOpenFileFlags flags) noexcept {
    if (path.empty()) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        std::filesystem::path normPath = std::filesystem::path(path).make_preferred();
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
        #ifdef POSIX
            file = std::fopen(normPath.c_str(), modes);
            if (!file) mLogger.error() << "fopen failed: " << std::strerror(errno);
        #endif
        #ifdef WINDOWS
            file = _wfopen(normPath.c_str(), modes);
            if (!file) mLogger.error() << "_wfopen failed: " << std::strerror(errno);
        #endif
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
        file_identity id(file);
        auto idIt = mFileSources.find(id);
        if (idIt != mFileSources.end()) {
            auto source = idIt->second;
            if (!(source->flags & TPR_OPEN_FILE_SYNC_FLAG_BIT) && (flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) {
                std::fclose(source->file);
                source->file = file;
            } else {
                std::fclose(file);
            }
            FileHandle handle{};
            if (!(flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) handle.mask &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
            handle.source = source;
            mFiles.insert_or_assign(mFileCounter, handle);

        } else {
            FileHandle handle{};
            if (!(flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) handle.mask &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
            handle.source = std::make_shared<FileSource>(file, flags);
            mFiles.insert_or_assign(mFileCounter, handle);
        }
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

expected<TprFile, TprResult> FileRegistry::createMemoryFile() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        FileHandle handle{};
        handle.source = std::make_shared<MemorySource>();
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

expected<TprFile, TprResult> FileRegistry::forkFile(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& srcSource = handleIt->second.source;

        auto mem = std::make_shared<MemorySource>();
        std::visit(overload{
            [&mem](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                mem->data.resize(len);
                rewind(src->file);
                errno = 0;
                auto n = fread(mem->data.data(), 1, len, src->file);
                if (n != len) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [&mem](std::shared_ptr<MemorySource> src) {
                // TODO: add COW
                mem->data.resize(src->data.size());
                std::memcpy(mem->data.data(), src->data.data(), mem->data.size());
            }
        }, srcSource);

        FileHandle handle{};
        handle.source = mem;
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

expected<TprFile, TprResult> FileRegistry::createFileCapability(TprFile file, TprFileCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        FileHandle handle{};
        handle.mask = mask & handleIt->second.mask;
        handle.source = handleIt->second.source;
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

void FileRegistry::closeFile(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return;
        auto source = handleIt->second.source;
        mFiles.erase(handleIt);
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


TprResult FileRegistry::seek(TprFile file, int64_t offset, TprSeekWhence whence) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        uint64_t size;
        std::visit(overload{
            [&size](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](std::shared_ptr<MemorySource> src) {
                size = src->data.size();
            }
        }, handle.source);

        switch (whence) {
            case TPR_SEEK_WHENCE_BEGIN:
                if (offset > size || offset < 0) return TPR_ERROR_OUT_OF_RANGE;
                handle.pos = offset;
                break;
            case TPR_SEEK_WHENCE_CURRENT:
                if (offset + handle.pos > size || offset + handle.pos < 0) return TPR_ERROR_OUT_OF_RANGE;
                handle.pos += offset;
                break;
            case TPR_SEEK_WHENCE_END:
                if (-offset > size || offset > 0) return TPR_ERROR_OUT_OF_RANGE;
                handle.pos = size + offset;
                break;
            default:
                return TPR_ERROR_INVALID_VALUE;
        }

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

expected<uint64_t, TprResult> FileRegistry::tell(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        return handle.pos;

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

TprResult FileRegistry::read(TprFile file, uint64_t n, std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        uint64_t size;
        std::visit(overload{
            [&size](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](std::shared_ptr<MemorySource> src) {
                size = src->data.size();
            }
        }, handle.source);
        if (handle.pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos = handle.pos](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fread(pData, 1, n, src->file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [pData, n, pos = handle.pos](std::shared_ptr<MemorySource> src) {
                std::memcpy(pData, src->data.data() + pos, n);
            }
        }, handle.source);

        handle.pos += n;
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

TprResult FileRegistry::readAt(TprFile file, uint64_t pos, uint64_t n, std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        uint64_t size;
        std::visit(overload{
            [&size](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](std::shared_ptr<MemorySource> src) {
                size = src->data.size();
            }
        }, handle.source);
        if (pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fread(pData, 1, n, src->file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [pData, n, pos](std::shared_ptr<MemorySource> src) {
                std::memcpy(pData, src->data.data() + pos, n);
            }
        }, handle.source);

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

TprResult FileRegistry::resize(TprFile file, uint64_t newSize) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        std::visit(overload{
            [newSize](std::shared_ptr<FileSource> src) {
                #ifdef POSIX
                    int fd = fileno(src->file);
                    errno = 0;
                    if (ftruncate(fd, newSize) == -1) throw std::system_error(errno, std::generic_category(), "ftruncate failed");
                #endif
                #ifdef WINDOWS
                    int fd = _fileno(src->file);
                    errno = 0;
                    if (_chsize(fd, newSize) == -1) throw std::system_error(errno, std::generic_category(), "_chsize failed");
                #endif
            },
            [newSize](std::shared_ptr<MemorySource> src) {
                src->data.resize(newSize);
            }
        }, handle.source);

        handle.pos = 0;
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

TprResult FileRegistry::write(TprFile file, uint64_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        uint64_t size;
        std::visit(overload{
            [&size](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](std::shared_ptr<MemorySource> src) {
                size = src->data.size();
            }
        }, handle.source);
        if (handle.pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos = handle.pos](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src->file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [pData, n, pos = handle.pos](std::shared_ptr<MemorySource> src) {
                std::memcpy(src->data.data() + pos, pData, n);
            }
        }, handle.source);

        handle.pos += n;
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

TprResult FileRegistry::writeAt(TprFile file, uint64_t pos, uint64_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        uint64_t size;
        std::visit(overload{
            [&size](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src->file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                size = len;
            },
            [&size](std::shared_ptr<MemorySource> src) {
                size = src->data.size();
            }
        }, handle.source);
        if (pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src->file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [pData, n, pos](std::shared_ptr<MemorySource> src) {
                std::memcpy(src->data.data() + pos, pData, n);
            }
        }, handle.source);

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

TprResult FileRegistry::append(TprFile file, uint64_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;

        std::visit(overload{
            [n, pData](std::shared_ptr<FileSource> src) {
                errno = 0;
                if (fseek(src->file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long size = ftell(src->file);
                if (size < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                #ifdef POSIX
                    int fd = fileno(src->file);
                    errno = 0;
                    if (ftruncate(fd, size + n) == -1) throw std::system_error(errno, std::generic_category(), "ftruncate failed");
                #endif
                #ifdef WINDOWS
                    int fd = _fileno(src->file);
                    if (_chsize(fd, size + n) == -1) throw std::system_error(errno, std::generic_category(), "_chsize failed");
                #endif
                errno = 0;
                if (fseek(src->file, size, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src->file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [n, pData](std::shared_ptr<MemorySource> src) {
                src->data.insert(src->data.end(), pData, pData + n);
            }
        }, handle.source);

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


expected<TprPathType, TprResult> FileRegistry::pathType(std::filesystem::path path) noexcept {
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

TprResult FileRegistry::createDirectory(std::filesystem::path path, TprCreateDirectoryFlags flags) noexcept {
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

TprResult FileRegistry::touchFile(std::filesystem::path path, TprTouchFileFlags flags) noexcept {
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

TprResult FileRegistry::remove(std::filesystem::path path) noexcept {
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

TprResult FileRegistry::move(std::filesystem::path path, std::filesystem::path newPath) noexcept {
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
