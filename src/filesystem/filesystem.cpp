
#include "filesystem.hpp"
#include "core.hpp"
#include "tempor.h"
#include "log_entry.hpp"
#include "logger.hpp"

#include <mat/interval_union.hpp>
#include <fio/fio.hpp>

#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <system_error>
#include <utility>
#include <variant>
#include <cstdio>
#include <vector>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <new>

#include <mio/mmap.hpp>
#include <mio/page.hpp>


Filesystem::Filesystem(Logger logger, std::atomic<TprResult>& rRunResult) : mLogger(std::move(logger)), mrRunResult(rRunResult) {}
Filesystem::~Filesystem() {}


expected<TprFile, TprResult> Filesystem::openPathFile(const std::filesystem::path& path, TprOpenFileFlags flags) noexcept {
    if (path.empty()) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        if (
            (flags & TPR_OPEN_FILE_ALWAYS_NEW_FLAG_BIT) ||
            ((flags & TPR_OPEN_FILE_NEW_IF_NONE_FLAG_BIT) && !std::filesystem::exists(path))
        ) {
            auto file = fio::zero(path);
            if (!file) return unexpected(file.error());
            std::fclose(file.value());
        }
        auto fileExp = fio::open(path, flags & TPR_OPEN_FILE_PERSISTENT_FLAG_BIT, flags & TPR_OPEN_FILE_TEXT_BIT);
        if (!fileExp.has_value()) return unexpected(fileExp.error());
        auto file = fileExp.value();
        FileHandle handle{};
        if (!(flags & TPR_OPEN_FILE_PERSISTENT_FLAG_BIT)) handle.capability &= ~TPR_FILE_CAPABILITY_WRITE_FLAG_BIT;
        handle.entry = std::make_shared<FileSource>(file, flags);
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Created File " << mFileCounter << " for \"" << path.string() << "\"";
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
        handle.entry = std::make_shared<MemorySource>();
        mFiles.insert_or_assign(mFileCounter, handle);
        mLogger.debug() << "Created Memory File " << mFileCounter;
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

expected<TprFile, TprResult> Filesystem::cloneFileCapability(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        FileHandle handle{handleIt->second.entry, handleIt->second.capability};
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

TprResult Filesystem::attenuateFileCapability(TprFile file, TprFileCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        handleIt->second.capability &= mask;
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

void Filesystem::destroyFile(TprFile file) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return;
        auto entry = handleIt->second.entry;
        mFiles.erase(handleIt);
        if (std::holds_alternative<std::shared_ptr<SnapshotSource>>(entry)) {
            auto snapshot = std::get<std::shared_ptr<SnapshotSource>>(entry);
            if (snapshot.use_count() == 1) {
                auto origin = variant_cast_to<std::weak_ptr<FileEntry>>(snapshot->origin).lock();
                if (origin) origin->snapshots.erase(snapshot);
            }
        }
        auto& snapshots = variant_cast_to<std::shared_ptr<FileEntry>>(entry)->snapshots;
        for (auto it = snapshots.begin(); it != snapshots.end();) {
            if (auto snapshot = it->lock()) {
                auto save = mat::interval_union<size_t>{mat::interval<size_t>{0, snapshot->size}} - snapshot->data.set();
                if (save.empty()) continue;
                auto saveResult = std::visit(overload{
                    [&snapshot, save](const std::shared_ptr<FileSource>& src) {
                        for (const auto& el : save) {
                            auto exp = fio::read<std::byte>(src->file, el);
                            if (!exp) return exp.error();
                            auto inter = mat::data_interval<std::byte>{std::move(exp.value()), el.set_begin()};
                            snapshot->data.merge_with(std::move(inter));
                        }
                        return TPR_SUCCESS;
                    },
                    [&snapshot, save](const std::shared_ptr<MemorySource>& src) {
                        for (const auto& el : save) {
                            mat::view_interval<std::byte> d{
                                src->data.begin() + static_cast<std::vector<std::byte>::difference_type>(el.set_begin()),
                                src->data.begin() + static_cast<std::vector<std::byte>::difference_type>(el.set_end()),
                                el.set_begin()
                            };
                            snapshot->data.merge_with(d);
                        }
                        return TPR_SUCCESS;
                    },
                    [&snapshot, save](const std::shared_ptr<SnapshotSource>& src) {
                        snapshot->data.merge_with(std::move(src->data & save));
                        return TPR_SUCCESS;
                    }
                }, entry);
                if (!saveResult) {
                    mLogger.warn() << "Failed to write to a snapshot [" << to_underlying(saveResult) << "]";
                }
                it++;
            } else {
                mLogger.warn() << "File " << get_basic_handle_index(file) << " had an invalid pointer to snapshot. Removing it";
                it = snapshots.erase(it);
            }
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

expected<TprFile, TprResult> Filesystem::cloneFile(TprFile file, TprForkFileFlags flags) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto& origin = handleIt->second.entry;

        if (flags & TPR_FORK_FILE_SNAPSHOT_FLAG_BIT) {
            auto snapshot = std::make_shared<SnapshotSource>();
            snapshot->origin = variant_cast<weakFileVariant>(origin);
            snapshot->size = std::visit(overload{
                [](const std::shared_ptr<FileSource>& src) { return fio::size(src->file); },
                [](const std::shared_ptr<MemorySource>& src) { return src->data.size(); },
                [](const std::shared_ptr<SnapshotSource>& src) { return src->size; }
            }, origin);
            FileHandle handle{snapshot};
            mFiles.insert_or_assign(mFileCounter, handle);
            mLogger.debug() << "Forked to Snapshot File " << mFileCounter << " from File " << get_basic_handle_index(file);
            auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
            mFileCounter++;
            return h;

        } else {
            auto dataExp = std::visit(overload{
                [this](const std::shared_ptr<FileSource>& src) -> expected<sharedFileVariant, TprResult> {
                    auto dataExp = fio::read<std::byte>(src->file, mat::interval<size_t>(0, SIZE_MAX));
                    if (!dataExp.has_value()) return unexpected(dataExp.error());
                    return sharedFileVariant(std::make_shared<MemorySource>(std::move(dataExp.value())));
                },
                [](const std::shared_ptr<MemorySource>& src) -> expected<sharedFileVariant, TprResult> {
                    return sharedFileVariant(std::make_shared<MemorySource>(*src));
                },
                [&origin, this](const std::shared_ptr<SnapshotSource>& src) -> expected<sharedFileVariant, TprResult> {
                    auto snapshot = std::make_shared<SnapshotSource>(*src);
                    auto origin = variant_cast_to<std::weak_ptr<FileEntry>>(src->origin).lock();
                    if (origin) origin->snapshots.insert(snapshot);
                    return sharedFileVariant(snapshot);
                }
            }, origin);
            if (!dataExp) return unexpected(dataExp.error());
            FileHandle handle{dataExp.value()};
            mFiles.insert_or_assign(mFileCounter, handle);
            mLogger.debug() << "Forked to File " << mFileCounter << " from File " << get_basic_handle_index(file);
            auto h = construct_basic_handle<TprFile>(mFileCounter, 0, handle_type::file);
            mFileCounter++;
            return h;
        }
    } catch (const std::bad_alloc& e) {
        return unexpected(TPR_ERROR_BAD_ALLOC);
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

bool Filesystem::isSnapshotCommitable(SnapshotSource& snapshot) {
    return (snapshot.conflict & (snapshot.data.set() | snapshot.read)).empty();
}


expected<bool, TprResult> Filesystem::snapshotCommitable(TprFile handle) noexcept {
    if (get_basic_handle_type(handle) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(handle));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        if (!std::holds_alternative<std::shared_ptr<SnapshotSource>>(handle.entry)) return TPR_ERROR_INVALID_OPERATION;
        return isSnapshotCommitable(*std::get<std::shared_ptr<SnapshotSource>>(handle.entry).get());
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

TprResult Filesystem::commitSnapshot(TprFile handle) noexcept {
    if (get_basic_handle_type(handle) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(handle));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        if (!std::holds_alternative<std::shared_ptr<SnapshotSource>>(handle.entry)) return TPR_ERROR_INVALID_OPERATION;
        auto snapshot = std::get<std::shared_ptr<SnapshotSource>>(handle.entry);
        if (!isSnapshotCommitable(*snapshot.get())) return TPR_ERROR_INVALID_OPERATION;
        auto commitResult = std::visit(overload{
            [&snapshot](const std::weak_ptr<FileSource>& dstWeak) -> TprResult {
                auto dst = dstWeak.lock();
                if (!dst) return TPR_ERROR_DOESNT_EXIST;
                std::fflush(dst->file);
                fio::resize(dst->file, snapshot->data.set_end());
                mio::file_handle_type handle = fio::mio_handle(dst->file);
                mio::basic_mmap_sink<std::byte> mmap(handle);
                snapshot->data.copy_subset(mat::interval<size_t>{0, mmap.size()}, mmap.begin());
                snapshot->data.clear();
                return TPR_SUCCESS;
            },
            [&snapshot](const std::weak_ptr<MemorySource>& dstWeak) -> TprResult {
                auto dst = dstWeak.lock();
                if (!dst) return TPR_ERROR_DOESNT_EXIST;
                snapshot->data.copy_subset(mat::interval<size_t>(0, snapshot->data.set_end()), dst->data.begin());
                return TPR_SUCCESS;
            },
            [&snapshot](const std::weak_ptr<SnapshotSource>& dstWeak) -> TprResult {
                auto dst = dstWeak.lock();
                if (!dst) return TPR_ERROR_DOESNT_EXIST;
                dst->data.merge_with(snapshot->data);
                return TPR_SUCCESS;
            }
        }, snapshot->origin);
        if (!commitResult) return commitResult;
        snapshot->data.clear();
        snapshot->read.clear();
        snapshot->conflict.clear();
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

TprResult Filesystem::resetSnapshot(TprFile handle, uint64_t pos, uint64_t n) noexcept {
    if (get_basic_handle_type(handle) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    if (pos > INT64_MAX || n > INT64_MAX) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(handle));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        if (!std::holds_alternative<std::shared_ptr<SnapshotSource>>(handle.entry)) return TPR_ERROR_INVALID_OPERATION;
        auto snapshot = std::get<std::shared_ptr<SnapshotSource>>(handle.entry);
        if (pos + n >= snapshot->size) return TPR_ERROR_OUT_OF_RANGE;
        snapshot->data -= mat::interval<size_t>(pos, pos + n);
        snapshot->read -= mat::interval<size_t>(pos, pos + n);
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


TprResult Filesystem::resizeFile(TprFile file, uint64_t newSize) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    if (newSize > INT64_MAX) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        uint64_t oldSize = std::visit(overload{
            [](std::shared_ptr<FileSource>& src) { return fio::size_destructive(src->file); },
            [](std::shared_ptr<MemorySource>& src) { return src->data.size(); },
            [](std::shared_ptr<SnapshotSource>& src) { return src->size; }
        }, handle.entry);
        if (oldSize == newSize) return TPR_SUCCESS;
        std::visit(overload{
            [newSize](const std::shared_ptr<FileSource>& src) { fio::resize(src->file, newSize); },
            [newSize](const std::shared_ptr<MemorySource>& src) { src->data.resize(newSize); },
            [newSize](const std::shared_ptr<SnapshotSource>& src) { src->data &= mat::interval<size_t>(0, newSize); src->size = newSize; }
        }, handle.entry);
        auto& snapshots = variant_cast_to<std::shared_ptr<FileEntry>>(handle.entry)->snapshots;
        for (auto it = snapshots.begin(); it != snapshots.end();) {
            if (auto snapshot = it->lock()) {
                if (oldSize > newSize) snapshot->conflict |= mat::interval<size_t>{newSize, oldSize};
                else snapshot->conflict |= mat::interval<size_t>{oldSize, newSize};
                it++;
            } else {
                mLogger.warn() << "File " << get_basic_handle_index(file) << " had an invalid pointer to snapshot. Removing it";
                it = snapshots.erase(it);
            }
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

TprResult Filesystem::readFile(TprFile file, uint64_t pos, uint64_t n, void* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    if (pos > INT64_MAX || n > INT64_MAX) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        uint64_t size = std::visit(overload{
            [](std::shared_ptr<FileSource>& src) { return fio::size_destructive(src->file); },
            [](std::shared_ptr<MemorySource>& src) { return src->data.size(); },
            [](std::shared_ptr<SnapshotSource>& src) { return src->size; }
        }, handle.entry);
        if (pos + n > size) return TPR_ERROR_OUT_OF_RANGE;
        auto readResult = std::visit(overload{
            [pData, n, pos](const std::shared_ptr<FileSource>& src) {
                return fio::read(src->file, mat::interval<size_t>{pos, pos + n}, pData);
            },
            [pData, n, pos](const std::shared_ptr<MemorySource>& src) {
                std::memcpy(pData, src->data.data() + pos, n);
                return TPR_SUCCESS;
            },
            [pData, n, pos](const std::shared_ptr<SnapshotSource>& src) {
                src->read |= mat::interval<size_t>(pos, pos + n);
                std::shared_ptr<SnapshotSource> curr = src;
                mat::interval_union<size_t> copy{{pos, pos + n}};
                while (!copy.empty() && curr) {
                    auto origin = curr->origin;
                    auto nextExp = std::visit(overload{
                        [&copy, pData](const std::weak_ptr<FileSource>& srcWeak) -> expected<std::shared_ptr<SnapshotSource>, TprResult> {
                            auto src = srcWeak.lock();
                            if (!src) return std::shared_ptr<SnapshotSource>(nullptr);
                            for (const auto& el : copy) {
                                fio::read(src->file, el, reinterpret_cast<std::byte*>(pData) + el.set_begin());
                            }
                            copy.clear();
                            return std::shared_ptr<SnapshotSource>(nullptr);
                        },
                        [&copy, pData](const std::weak_ptr<MemorySource>& srcWeak) -> expected<std::shared_ptr<SnapshotSource>, TprResult> {
                            auto src = srcWeak.lock();
                            if (!src) return std::shared_ptr<SnapshotSource>(nullptr);
                            for (const auto& el : copy) {
                                std::memcpy(
                                    reinterpret_cast<std::byte*>(pData) + el.set_begin(),
                                    src->data.data() + el.set_begin(), el.length()
                                );
                            }
                            copy.clear();
                            return std::shared_ptr<SnapshotSource>(nullptr);
                        },
                        [&copy, pData](const std::weak_ptr<SnapshotSource>& srcWeak) -> expected<std::shared_ptr<SnapshotSource>, TprResult> {
                            auto src = srcWeak.lock();
                            if (!src) return std::shared_ptr<SnapshotSource>(nullptr);
                            src->data.copy_subset(copy, reinterpret_cast<std::byte*>(pData) + copy.set_begin());
                            copy -= src->data.set();
                            return src;
                        }
                    }, origin);
                    if (!nextExp) return nextExp.error();
                    curr = nextExp.value();
                }
                return TPR_SUCCESS;
            }
        }, handle.entry);
        if (!readResult) return readResult;
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

TprResult Filesystem::writeFile(TprFile file, uint64_t pos, uint64_t n, const void* pData) noexcept {
    if (get_basic_handle_type(file) != handle_type::file) return TPR_ERROR_INVALID_VALUE;
    if (pos > INT64_MAX || n > INT64_MAX) return TPR_ERROR_INVALID_VALUE;
    std::lock_guard<std::mutex> lock(mMutex);
    try {
        auto handleIt = mFiles.find(get_basic_handle_index(file));
        if (handleIt == mFiles.end()) return TPR_ERROR_INVALID_VALUE;
        auto& handle = handleIt->second;
        auto view = mat::view_interval<const std::byte>(
            reinterpret_cast<const std::byte*>(pData),
            reinterpret_cast<const std::byte*>(pData) + n,
            pos
        );
        auto size = std::visit(overload{
            [](const std::shared_ptr<FileSource>& src) -> size_t { return fio::size_destructive(src->file); },
            [](const std::shared_ptr<MemorySource>& src) -> size_t { return src->data.size(); },
            [](const std::shared_ptr<SnapshotSource>& src) -> size_t { return src->size; }
        }, handle.entry);
        if (view.set_end() > size) return TPR_ERROR_OUT_OF_RANGE;
        auto& snapshots = variant_cast_to<std::shared_ptr<FileEntry>>(handle.entry)->snapshots;
        for (auto it = snapshots.begin(); it != snapshots.end();) {
            if (auto snapshot = it->lock()) {
                auto save =
                    mat::interval_union<size_t>(view.set() & mat::interval<size_t>{0, snapshot->size}) - snapshot->data.set();
                if (save.empty()) continue;
                auto saveResult = std::visit(overload{
                    [&snapshot, save](const std::shared_ptr<FileSource>& src) {
                        for (const auto& el : save) {
                            auto exp = fio::read<std::byte>(src->file, el);
                            if (!exp) return exp.error();
                            auto inter = mat::data_interval<std::byte>{std::move(exp.value()), el.set_begin()};
                            snapshot->data.merge_with(std::move(inter));
                        }
                        return TPR_SUCCESS;
                    },
                    [&snapshot, save](const std::shared_ptr<MemorySource>& src) {
                        for (const auto& el : save) {
                            mat::view_interval<std::byte> d{
                                src->data.begin() + static_cast<std::vector<std::byte>::difference_type>(el.set_begin()),
                                src->data.begin() + static_cast<std::vector<std::byte>::difference_type>(el.set_end()),
                                el.set_begin()
                            };
                            snapshot->data.merge_with(d);
                        }
                        return TPR_SUCCESS;
                    },
                    [&snapshot, save](const std::shared_ptr<SnapshotSource>& src) {
                        snapshot->data.merge_with(std::move(src->data & save));
                        return TPR_SUCCESS;
                    }
                }, handle.entry);
                if (!saveResult) return saveResult;
                snapshot->conflict |= save;
                it++;
            } else {
                mLogger.warn() << "File " << get_basic_handle_index(file) << " had an invalid pointer to snapshot. Removing it";
                it = snapshots.erase(it);
            }
        }
        auto writeResult = std::visit(overload{
            [view](const std::shared_ptr<FileSource>& src) {
                return fio::write(src->file, view);
            },
            [view](const std::shared_ptr<MemorySource>& src) {
                std::memcpy(src->data.data() + view.set_begin(), view.data(), view.length()); return TPR_SUCCESS;
            },
            [view](const std::shared_ptr<SnapshotSource>& src) {
                src->data.merge_with(view); return TPR_SUCCESS;
            }
        }, handle.entry);
        if (!writeResult) return writeResult;
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



expected<TprPathType, TprResult> Filesystem::pathType(const std::filesystem::path& path) noexcept {
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

TprResult Filesystem::createDirectory(const std::filesystem::path& path, TprCreateDirectoryFlags flags) noexcept {
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

TprResult Filesystem::touchFile(const std::filesystem::path& path, TprTouchFileFlags flags) noexcept {
    try {
        if (flags & TPR_TOUCH_FILE_CLEAR_FLAG_BIT) {
            auto file = fio::zero(path.c_str());
            if (!file) return file.error();
            std::fclose(file.value());
        } else {
            auto file = fio::assure(path.c_str());
            if (!file) return file.error();
            std::fclose(file.value());
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

TprResult Filesystem::remove(const std::filesystem::path& path) noexcept {
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

TprResult Filesystem::move(const std::filesystem::path& path, const std::filesystem::path& newPath) noexcept {
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
