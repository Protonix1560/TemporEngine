#ifndef FIO_HPP_
#define FIO_HPP_

#include "core.hpp"
#include "tempor.h"

#include <mat/interval_union.hpp>

#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <cerrno>
#include <vector>
#include <array>

#include <mio/mmap.hpp>

#ifdef LINUX
    #include <sys/types.h>
    #include <stdio.h>
    #include <unistd.h>
#endif


namespace fio {

    inline TprResult seek(FILE* file, int64_t pos, int whence) noexcept {
        switch (whence) {
            case SEEK_CUR: case SEEK_END: case SEEK_SET: break;
            default: return TPR_ERROR_INVALID_VALUE;
        }
        #ifdef POSIX
            if (fseeko(file, static_cast<off_t>(pos), whence)) {
                switch (errno) {
                    case EBADF:
                        return TPR_ERROR_INVALID_VALUE;
                    case EOVERFLOW:
                    case EINVAL:
                        return TPR_ERROR_OUT_OF_RANGE;
                    case ENOMEM:
                        return TPR_ERROR_BAD_ALLOC;
                    default:
                        return TPR_UNKNOWN_ERROR;
                }
            }
        #endif
        #ifdef WINDOWS
            if (_fseeki64(file, static_cast<__int64>(pos), whence)) {
                return TPR_UNKNOWN_ERROR;
            }
        #endif
        return TPR_SUCCESS;
    }

    inline expected<uint64_t, TprResult> tell(FILE* file) noexcept {
        #ifdef POSIX
            auto pos = ftello(file);
            if (pos < 0) {
                switch (errno) {
                    case EBADF:
                    case EINVAL:
                        return unexpected(TPR_ERROR_INVALID_VALUE);
                    case EOVERFLOW:
                        return unexpected(TPR_ERROR_OUT_OF_RANGE);
                    case ENOMEM:
                        return unexpected(TPR_ERROR_BAD_ALLOC);
                    default:
                        return unexpected(TPR_UNKNOWN_ERROR);
                }
            }
            return pos;
        #endif
        #ifdef WINDOWS
            errno = 0;
            auto pos = _fseeki64(file, static_cast<__int64>(pos), whence);
            if (pos < 0) {
                switch (errno) {
                    case EBADF:
                    case EINVAL:
                        return unexpected(TPR_ERROR_INVALID_VALUE);
                    default:
                        return unexpected(TPR_UNKNOWN_ERROR);
                }
            }
            return pos;
        #endif
        return TPR_SUCCESS;
    }

    inline uint64_t size(FILE* file) noexcept {
        auto oldPos = tell(file);
        seek(file, 0, SEEK_END);
        auto newPos = tell(file);
        seek(file, oldPos, SEEK_SET);
        return newPos;
    }

    inline uint64_t size_destructive(FILE* file) noexcept {
        seek(file, 0, SEEK_END);
        return tell(file);
    }

    inline expected<FILE*, TprResult> open(const std::filesystem::path& path, bool persistent = false, bool text = false) noexcept {
        constexpr const char* const modesMap[4] = {
            "rb", "rb+", "r", "r+"
        };
        auto modes = modesMap[(persistent ? 1 : 0) | (text ? 2 : 0)];
        FILE* file;
        file = std::fopen(path.c_str(), modes);
        if (!file) {
            switch (errno) {
                case ENOMEM:
                    return unexpected(TPR_ERROR_BAD_ALLOC);
                case EACCES:
                case EPERM:
                case EROFS:
                case ETXTBSY:
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                case ENOENT:
                case EISDIR:
                case ENOTDIR:
                    // means "no such file"
                    return unexpected(TPR_ERROR_DOESNT_EXIST);
                case EINVAL:
                case ENAMETOOLONG:
                    // means "not an accepted file name"
                    return unexpected(TPR_ERROR_INVALID_VALUE);
                case EMFILE:
                case ENFILE:
                case ENOSPC:
                    // means "not enough room for a new file"
                    return unexpected(TPR_ERROR_COUNT_OVERFLOW);
                default:
                    return unexpected(TPR_UNKNOWN_ERROR);
            }
        }
        return file;
    }

    inline expected<FILE*, TprResult> zero(const std::filesystem::path& path) noexcept {
        FILE* file;
        file = std::fopen(path.c_str(), "wb");
        if (!file) {
            switch (errno) {
                case ENOMEM:
                    return unexpected(TPR_ERROR_BAD_ALLOC);
                case EACCES:
                case EPERM:
                case EROFS:
                case ETXTBSY:
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                case ENOENT:
                case EISDIR:
                case ENOTDIR:
                    // means "no such file"
                    return unexpected(TPR_ERROR_DOESNT_EXIST);
                case EINVAL:
                case ENAMETOOLONG:
                    // means "not an accepted file name"
                    return unexpected(TPR_ERROR_INVALID_VALUE);
                case EMFILE:
                case ENFILE:
                case ENOSPC:
                    // means "not enough room for a new file"
                    return unexpected(TPR_ERROR_COUNT_OVERFLOW);
                default:
                    return unexpected(TPR_UNKNOWN_ERROR);
            }
        }
        return file;
    }

    inline expected<FILE*, TprResult> assure(const std::filesystem::path& path) noexcept {
        FILE* file;
        file = std::fopen(path.c_str(), "ab");
        if (!file) {
            switch (errno) {
                case ENOMEM:
                    return unexpected(TPR_ERROR_BAD_ALLOC);
                case EACCES:
                case EPERM:
                case EROFS:
                case ETXTBSY:
                    return unexpected(TPR_ERROR_NOT_PERMITTED);
                case ENOENT:
                case EISDIR:
                case ENOTDIR:
                    // means "no such file"
                    return unexpected(TPR_ERROR_DOESNT_EXIST);
                case EINVAL:
                case ENAMETOOLONG:
                    // means "not an accepted file name"
                    return unexpected(TPR_ERROR_INVALID_VALUE);
                case EMFILE:
                case ENFILE:
                case ENOSPC:
                    // means "not enough room for a new file"
                    return unexpected(TPR_ERROR_COUNT_OVERFLOW);
                default:
                    return unexpected(TPR_UNKNOWN_ERROR);
            }
        }
        return file;
    }

    inline TprResult resize(FILE* file, uint64_t size) noexcept {
        #ifdef POSIX
            assert(size <= std::numeric_limits<off_t>::max());
            int fd = fileno(file);
            if (ftruncate(fd, static_cast<off_t>(size))) {
                switch (errno) {
                    case EFBIG:
                    case EINVAL:
                        return TPR_ERROR_BAD_ALLOC;
                    default:
                        return TPR_UNKNOWN_ERROR;
                }
            }
        #endif
        #ifdef WINDOWS
            assert(size <= INT64_MAX);
            int fd = _fileno(src.file);
            if (_chsize_s(fd, static_cast<__int64>(size))) {
                switch (errno) {
                    case EACCES:
                        return TPR_ERROR_NOT_PERMITTED;
                    case ENOSPC:
                        return TPR_ERROR_BAD_ALLOC;
                    default:
                        return TPR_UNKNOWN_ERROR;
                }
            }
        #endif
        return TPR_SUCCESS;
    }

    template <typename ByteT, size_t Chunk = TEMPOR_FIO_RW_CHUNK_SIZE>
    inline expected<std::vector<ByteT>, TprResult> read(FILE* file, const mat::interval<size_t>& inter) {
        std::vector<ByteT> result;
        std::array<ByteT, Chunk> buffer;
        if (auto r = seek(file, static_cast<int64_t>(inter.set_begin()), SEEK_SET); !r) return unexpected(r);
        while (true) {
            size_t n = std::fread(buffer.data(), 1, Chunk, file);
            if (std::ferror(file) || n == 0) return unexpected(TPR_UNKNOWN_ERROR);
            auto remain = inter.length() - result.size();
            if (remain > Chunk) remain = Chunk;
            result.insert(result.end(), buffer.begin(), buffer.begin() + remain);
            if (remain < Chunk || std::feof(file)) return result;
        }
        return result;
    }

    template <typename ByteT, size_t Chunk = TEMPOR_FIO_RW_CHUNK_SIZE>
    inline TprResult read(FILE* file, const mat::interval<size_t>& inter, ByteT* begin) {
        auto it = begin;
        if (auto r = seek(file, static_cast<int64_t>(inter.set_begin()), SEEK_SET); !r) return r;
        while (true) {
            size_t n = std::fread(it, 1, Chunk, file);
            if (std::ferror(file) || n == 0) return TPR_UNKNOWN_ERROR;
            auto remain = inter.length() - (it - begin);
            if (remain > Chunk) remain = Chunk;
            if (remain < Chunk || std::feof(file)) return TPR_SUCCESS;
        }
        return TPR_SUCCESS;
    }

    inline mio::file_handle_type mio_handle(FILE* file) noexcept {
        #ifdef POSIX
            return fileno(file);
        #endif
        #ifdef WINDOWS
            return reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
        #endif
    }

    template <typename ByteT, size_t Chunk = TEMPOR_FIO_RW_CHUNK_SIZE>
    inline TprResult write(FILE* file, const mat::view_interval<const ByteT>& inter) {
        auto it = inter.begin();
        if (auto r = seek(file, static_cast<int64_t>(inter.set_begin()), SEEK_SET); !r) return r;
        while (true) {
            size_t n = std::fwrite(it, 1, Chunk, file);
            if (std::ferror(file) || n == 0) return TPR_UNKNOWN_ERROR;
            auto remain = inter.length() - (it - inter.begin());
            if (remain > Chunk) remain = Chunk;
            if (remain < Chunk || std::feof(file)) return TPR_SUCCESS;
        }
        return TPR_SUCCESS;
    }
}

#endif  // FIO_HPP_