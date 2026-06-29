
#include "file_registry.hpp"
#include "core.hpp"
#include "plugin_core.h"

#include <cerrno>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <system_error>
#include <variant>


FileRegistry::FileRegistry(Logger logger) : mLogger(logger) {}
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
            if (!file) mLogger.error(TPR_LOG_STYLE_ERROR1) << "fopen failed: " << std::strerror(errno);
        #endif
        #ifdef WINDOWS
            file = _wfopen(normPath.c_str(), modes);
            if (!file) mLogger.error(TPR_LOG_STYLE_ERROR1) << "_wfopen failed: " << std::strerror(errno);
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
                    return unexpected(TPR_PANIC);
            }
        }
        file_identity id(file);
        auto idIt = mFileMap.find(id);
        if (idIt != mFileMap.end()) {
            auto entryIt = mEntries.find(idIt->second);
            if (entryIt == mEntries.end()) {
                mLogger.panic() << "Corrupted internal structures: for file " << path << " " << idIt->second
                    << " doesn't appear in mEntries";
                return unexpected(TPR_PANIC);
            }
            auto& entry = entryIt->second;
            if (std::holds_alternative<MemorySource>(entry.source)) {
                mLogger.panic() << "Corrupted internal structures: entry " << entryIt->first
                    << " is a memory file but it appears in mFileMap for file " << path;
                return unexpected(TPR_PANIC);
            }
            auto& source = std::get<FileSource>(entry.source);
            if (!(source.flags & TPR_OPEN_FILE_SYNC_FLAG_BIT) && (flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) {
                std::fclose(source.file);
                source.file = file;
            } else {
                std::fclose(file);
            }
            FileHandle handle{};
            if (!(flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) handle.mask &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
            handle.entry = entryIt->first;
            mHandles[mHandleCounter] = handle;
            entry.refcount++;
            auto h = construct_basic_handle<TprFile>(mHandleCounter, 0, handle_type::file);
            mHandleCounter++;
            return h;

        } else {
            FileEntry entry{};
            entry.source = FileSource{file, flags};
            FileHandle handle{};
            if (!(flags & TPR_OPEN_FILE_SYNC_FLAG_BIT)) handle.mask &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
            handle.entry = mEntryCounter;
            mEntries[mEntryCounter] = entry;
            mHandles[mHandleCounter] = handle;
            auto h = construct_basic_handle<TprFile>(mHandleCounter, 0, handle_type::file);
            mEntryCounter++;
            mHandleCounter++;
            return h;
        }
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

expected<TprFile, TprResult> FileRegistry::createMemoryFile() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        FileEntry entry{};
        entry.source = MemorySource{};
        FileHandle handle{};
        handle.entry = mEntryCounter;
        mEntries[mEntryCounter] = entry;
        mHandles[mHandleCounter] = handle;
        auto h = construct_basic_handle<TprFile>(mHandleCounter, 0, handle_type::file);
        mEntryCounter++;
        mHandleCounter++;
        return h;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

expected<TprFile, TprResult> FileRegistry::forkFile(TprFile file) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto entryIt = mEntries.find(handleIt->second.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handleIt->second.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return unexpected(TPR_PANIC);
        }
        auto& srcEntry = entryIt->second;

        MemorySource mem{};
        std::visit(overload{
            [&mem](FileSource& src) {
                errno = 0;
                if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long len = ftell(src.file);
                if (len < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                mem.data.resize(len);
                rewind(src.file);
                errno = 0;
                auto n = fread(mem.data.data(), 1, len, src.file);
                if (n != len) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [&mem](MemorySource& src) {
                // TODO: add COW
                mem.data.resize(src.data.size());
                std::memcpy(mem.data.data(), src.data.data(), mem.data.size());
            }
        }, srcEntry.source);

        FileEntry dstEntry{};
        dstEntry.source = mem;
        FileHandle handle{};
        handle.entry = mEntryCounter;
        mEntries[mEntryCounter] = dstEntry;
        mHandles[mHandleCounter] = handle;
        auto h = construct_basic_handle<TprFile>(mHandleCounter, 0, handle_type::file);
        mEntryCounter++;
        mHandleCounter++;
        return h;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

expected<TprFile, TprResult> FileRegistry::createCapability(TprFile file, TprFileCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto entryIt = mEntries.find(handleIt->second.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handleIt->second.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return unexpected(TPR_PANIC);
        }
        auto& entry = entryIt->second;

        FileHandle handle{};
        handle.mask = mask & handleIt->second.mask;
        handle.entry = entryIt->first;
        mHandles[mHandleCounter] = handle;
        auto h = construct_basic_handle<TprFile>(mHandleCounter, 0, handle_type::file);
        mEntryCounter++;
        mHandleCounter++;
        return h;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

void FileRegistry::closeFile(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return;
        auto entryIt = mEntries.find(handleIt->second.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handleIt->second.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return;
        }
        auto& entry = entryIt->second;
        mHandles.erase(handleIt);
        entry.refcount--;
        if (entry.refcount == 0) {
            std::visit(overload{
                [](FileSource& src) {
                    std::fclose(src.file);
                },
                [](auto& src) {}
            }, entry.source);
            mEntries.erase(entryIt);
        }
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return;
    }
}


TprResult FileRegistry::seek(TprFile file, int32_t offset, TprSeekWhence whence) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        uint32_t size;
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
            }
        }, entry.source);

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
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

expected<uint32_t, TprResult> FileRegistry::tell(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        return handle.pos;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::read(TprFile file, uint32_t n, std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        uint32_t size;
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
            }
        }, entry.source);
        if (handle.pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos = handle.pos](FileSource& src) {
                errno = 0;
                if (fseek(src.file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fread(pData, 1, n, src.file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fread failed");
            },
            [pData, n, pos = handle.pos](MemorySource& src) {
                std::memcpy(pData, src.data.data() + pos, n);
            }
        }, entry.source);

        handle.pos += n;
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::readAt(TprFile file, uint32_t pos, uint32_t n, std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        uint32_t size;
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
            }
        }, entry.source);
        if (pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

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
            }
        }, entry.source);

        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::resize(TprFile file, uint32_t newSize) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

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
            }
        }, entry.source);

        handle.pos = 0;
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::write(TprFile file, uint32_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        uint32_t size;
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
            }
        }, entry.source);
        if (handle.pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

        std::visit(overload{
            [pData, n, pos = handle.pos](FileSource& src) {
                errno = 0;
                if (fseek(src.file, pos, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src.file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [pData, n, pos = handle.pos](MemorySource& src) {
                std::memcpy(src.data.data() + pos, pData, n);
            }
        }, entry.source);

        handle.pos += n;
        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::writeAt(TprFile file, uint32_t pos, uint32_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        uint32_t size;
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
            }
        }, entry.source);
        if (pos + n > size) {
            return TPR_ERROR_OUT_OF_RANGE;
        }

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
            }
        }, entry.source);

        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::append(TprFile file, uint32_t n, const std::byte* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mHandles.find(get_basic_handle_index(file));
        if (handleIt == mHandles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto entryIt = mEntries.find(handle.entry);
        if (entryIt == mEntries.end()) {
            mLogger.panic() << "Corrupted internal structures: entry " << handle.entry
                << " from handle " << handleIt->first << " doesn't appear in mEntries";
            return TPR_PANIC;
        }
        auto& entry = entryIt->second;

        std::visit(overload{
            [n, pData](FileSource& src) {
                errno = 0;
                if (fseek(src.file, 0, SEEK_END)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                long size = ftell(src.file);
                if (size < 0) throw std::system_error(errno, std::generic_category(), "ftell failed");
                #ifdef POSIX
                    int fd = fileno(src.file);
                    errno = 0;
                    if (ftruncate(fd, size + n) == -1) throw std::system_error(errno, std::generic_category(), "ftruncate failed");
                #endif
                #ifdef WINDOWS
                    int fd = _fileno(src.file);
                    if (_chsize(fd, size + n) == -1) throw std::system_error(errno, std::generic_category(), "_chsize failed");
                #endif
                errno = 0;
                if (fseek(src.file, size, SEEK_SET)) throw std::system_error(errno, std::generic_category(), "fseek failed");
                errno = 0;
                auto s = fwrite(pData, 1, n, src.file);
                if (s != n) throw std::system_error(errno, std::generic_category(), "fwrite failed");
            },
            [n, pData](MemorySource& src) {
                src.data.insert(src.data.end(), pData, pData + n);
            }
        }, entry.source);

        return TPR_SUCCESS;

    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
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
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return unexpected(TPR_PANIC);
    }
}

TprResult FileRegistry::createDirectory(std::filesystem::path path, TprCreateDirectoryFlags flags) noexcept {
    try {
        std::filesystem::create_directory(path);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
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
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}

TprResult FileRegistry::remove(std::filesystem::path path) noexcept {
    try {
        std::filesystem::remove_all(path);
        return TPR_SUCCESS;
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
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
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        return TPR_PANIC;
    }
}
