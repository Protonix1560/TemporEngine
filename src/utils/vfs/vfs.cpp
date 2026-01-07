
#include "vfs.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <utility>


#if defined(__linux__)
    #include <linux/memfd.h>
    #include <sys/mman.h>
    #include <elf.h>
    #include <sys/inotify.h>
#endif




ROFile VFSManager::openRO(std::filesystem::path path) {

    ROFile file(std::filesystem::absolute(path));
    
    return file;
}


std::vector<std::filesystem::path> VFSManager::enumDir(std::filesystem::path dirPath, EnumDirInclude incl) {
    
    if (!std::filesystem::exists(dirPath)) {
        throw Exception(ErrCode::WrongValueError, "VFSManager::enumDir: "s + dirPath.string() + " doesn't exist");
    }

    if (!std::filesystem::is_directory(dirPath)) {
        throw Exception(ErrCode::WrongValueError, "VFSManager::enumDir: "s + dirPath.string() + " is not a directory");
    }

    std::vector<std::filesystem::path> entries;

    auto entryCheck = [incl, this](const std::filesystem::directory_entry& entry) -> bool {
        if (incl == ENUM_DIR_INCLUDE_ALL) return true;
        if (!(incl & ENUM_DIR_INCLUDE_DIRS) && entry.is_directory()) return false;
        if (incl & ENUM_DIR_INCLUDE_DIRS && entry.is_directory()) return true;
        auto packet = openRO(entry.path());
        auto span = packet.read();
        const unsigned char* data = reinterpret_cast<const unsigned char*>(span.data());
        #if defined(__linux__)
            if (span.size() >= EI_NIDENT && data[EI_MAG0] == ELFMAG0 && data[EI_MAG1] == ELFMAG1 && data[EI_MAG2] == ELFMAG2 && data[EI_MAG3] == ELFMAG3) {
                unsigned char elfClass = data[4];
                if (elfClass == ELFCLASS32 && span.size() >= sizeof(Elf32_Ehdr)) {
                    auto elf = reinterpret_cast<const Elf32_Ehdr*>(data);
                    if (elf->e_type == ET_EXEC && (incl & ENUM_DIR_INCLUDE_EXECS)) return true;
                    if (elf->e_type == ET_DYN && span.size() >= (elf->e_phoff + elf->e_phnum * elf->e_phentsize) && elf->e_phentsize >= sizeof(Elf32_Word) && elf->e_phnum != 0) {
                        for (int i = elf->e_phoff; i < elf->e_phoff + elf->e_phnum * elf->e_phentsize; i += elf->e_phentsize) {
                            auto seg = reinterpret_cast<const Elf32_Phdr*>(data + i);
                            if (seg->p_type == PT_INTERP) {
                                if (incl & ENUM_DIR_INCLUDE_EXECS) return true;
                                else return false;
                            }
                        }
                        if (incl & ENUM_DIR_INCLUDE_LIBS) return true;
                        else return false;
                    }
                } else if (elfClass == ELFCLASS64 && span.size() >= sizeof(Elf64_Ehdr)) {
                    auto elf = reinterpret_cast<const Elf64_Ehdr*>(data);
                    if (elf->e_type == ET_EXEC && (incl & ENUM_DIR_INCLUDE_EXECS)) return true;
                    if (elf->e_type == ET_DYN && span.size() >= (elf->e_phoff + elf->e_phnum * elf->e_phentsize) && elf->e_phentsize >= sizeof(Elf64_Word) && elf->e_phnum != 0) {
                        for (int i = elf->e_phoff; i < elf->e_phoff + elf->e_phnum * elf->e_phentsize; i += elf->e_phentsize) {
                            auto seg = reinterpret_cast<const Elf64_Phdr*>(data + i);
                            if (seg->p_type == PT_INTERP) {
                                if (incl & ENUM_DIR_INCLUDE_EXECS) return true;
                                else return false;
                            }
                        }
                        if (incl & ENUM_DIR_INCLUDE_LIBS) return true;
                        else return false;
                    }
                }
            }
        #endif
        if (incl & ENUM_DIR_INCLUDE_OTHER) return true;
        return false;
    };

    if (incl & ENUM_DIR_INCLUDE_RECURSIVE) {
        auto it = std::filesystem::recursive_directory_iterator(dirPath);
        for (const auto& entry : it) {
            if (entryCheck(entry)) entries.push_back(entry.path());
        }

    } else {
        auto it = std::filesystem::directory_iterator(dirPath);
        for (const auto& entry : it) {
            if (entryCheck(entry)) entries.push_back(entry.path());
        }

    }

    return entries;
}


void VFSManager::init() {

    auto& logger = gGetServiceLocator()->get<Logger>();

    logger.info(TPR_LOG_STYLE_TIMESTAMP1) << LOG_VFS_NAME ": Initializing virtual filesystem...\n";

    if (getenv(ENV_TEMPOR_CONF_PATH)) {
        mCWDPath = std::filesystem::path(getenv(ENV_TEMPOR_CONF_PATH)).parent_path();
    } else {
        mCWDPath = std::filesystem::current_path();
    }

    logger.debug() << LOG_VFS_NAME ": Getting file tree...\n";

    std::vector<std::pair<std::filesystem::directory_iterator, size_t>> stack = {{std::filesystem::directory_iterator(mCWDPath), 0}};
    while (!stack.empty()) {
        auto& dir = stack.back().first;
        size_t depth = stack.back().second;
        for (const auto& entry : dir) {
            // logger.trace() << entry.path().string() << "\n";
            if (entry.is_directory()) {
                stack.emplace_back(std::filesystem::directory_iterator(entry), depth + 1);
            }
        }
        stack.pop_back();
    }

    logger.debug() << LOG_VFS_NAME ": Current working directory: \"" << mCWDPath.string() << "\"\n";

    logger.debug() << LOG_VFS_NAME ": Initializing filesystem event notification...\n";

    #if defined(__linux__)
    
        logger.trace() << LOG_VFS_NAME ": Trying to initialize inotify...\n";

        mInotifyFd = inotify_init1(O_NONBLOCK);
        if (mInotifyFd == -1) {
            switch (errno) {
                case ENOSYS:
                    logger.trace() << LOG_VFS_NAME ": Failed to initialize inotify: not supported by current kernel (ENOSYS)\n";
                default:
                    logger.trace() << LOG_VFS_NAME ": Failed to initialize inotify: errno " << errno << "\n";
            }
            inotify_failed:
            logger.trace() << LOG_VFS_NAME ": Fallbacking to regular subdir polling\n";
            mPeriodicScan = true;
        }

    #endif

}


void VFSManager::update() {

}


void VFSManager::shutdown() noexcept {

    auto& logger = gGetServiceLocator()->get<Logger>();

    #if defined(__linux__)
        if (mInotifyFd > 0) {
            close(mInotifyFd);
            logger.trace() << LOG_VFS_NAME ": Closed inotify fd\n";
        }
    #endif

}




ROSpan ROFile::read(const Iterator& start, const Iterator& end, size_t align) const {

    if (start > end) throw Exception(ErrCode::InternalError, "ROFile::read: end iterator must be equal or greater to start iterator");

    ROSpan span(end - start, nullptr);

    if (align == 0 || align == 1) {
        span.mpData = mpData + start;

    } else {
        for (const auto& [view, data] : mAlignedData) {
            if (view.start <= start && view.end >= end && reinterpret_cast<uintptr_t>(data.data() + (start - view.start)) % align == 0) {
                span.mpData = data.data() + (start - view.start);
                return span;
            }
        }
        auto& data = mAlignedData.emplace(View{start, end, align}, std::vector<std::byte, AlignedAllocator<std::byte>>(AlignedAllocator<std::byte>(align))).first->second;
        data.resize(end - start);
        std::memcpy(data.data(), mpData + start, end - start);
        span.mpData = data.data();
    }

    return span;
}


std::filesystem::path ROFile::getVirtualPath() const {

    auto& log = gGetServiceLocator()->get<Logger>();

    // trying to use memfd to create an anonymous memory file
    // normally works only on systems with linux kernel version >= 3.17
    #if defined(__linux__)
        log.trace() << LOG_VFS_NAME ": Trying to create anonymous memory file using memfd...\n";
        mVfd = memfd_create(mPath.c_str(), MFD_CLOEXEC);
        if (mVfd < 0) {
            log.error(TPR_LOG_STYLE_ERROR1) << LOG_VFS_NAME ": Failed to create anonymous memory file. Fallbacking to tmp file\n";
            goto memfd_attempt_failure;
        }
        if (write(mVfd, mpData, mSize) != mSize) {
            log.error(TPR_LOG_STYLE_ERROR1) << LOG_VFS_NAME ": Failed to write to anonymous memory file. Fallbacking to tmp file\n";
            close(mVfd);
            mVfd = -1;
            goto memfd_attempt_failure;
        }
        mVirtualPath = "/proc/self/fd/"s + std::to_string(mVfd);
        log.trace() << LOG_VFS_NAME ": Created anonymous memory file " << mVirtualPath << "\n";
        return mVirtualPath;
        memfd_attempt_failure:
    #endif

    std::filesystem::path tempDir = std::filesystem::temp_directory_path();
    std::filesystem::path tempFile = tempDir / std::filesystem::path("tempor_ioXXXXXX");

    std::ofstream ofs;

    do {
        tempFile.replace_filename("tempor_io" + std::to_string(rand()) + ".tmp");
    } while (std::filesystem::exists(tempFile));

    try {
        ofs.open(tempFile, std::ios::binary);
        if (!ofs) throw Exception(ErrCode::InternalError, LOG_VFS_NAME ": Failed to create tmp file "s + tempFile.string());
        gGetServiceLocator()->get<Logger>().trace() << LOG_VFS_NAME ": Created tmp file " << tempFile << "\n";
        ofs.write(reinterpret_cast<const char*>(mpData), mSize);
    } catch (...) {
        ofs.close();
        throw;
    }

    ofs.close();

    mVirtualPath = tempFile;

    return mVirtualPath;
}


std::filesystem::path ROFile::getRealPath() const {
    return mPath;
}


ROFile::~ROFile() noexcept {

    #if defined(__linux__)
        if (mVfd != -1) {
            gGetServiceLocator()->get<Logger>().trace() << LOG_VFS_NAME ": Closed anonymous memory file " << mVirtualPath << "\n";
            close(mVfd);
            mVirtualPath = "";
        }
    #endif

    if (!mVirtualPath.empty()) {
        try {
            if (!std::filesystem::remove(mVirtualPath)) throw std::runtime_error("");
            gGetServiceLocator()->get<Logger>().trace() << LOG_VFS_NAME ": Removed tmp file " << mVirtualPath << "\n";

        } catch(const std::exception&) {
            gGetServiceLocator()->get<Logger>().warn(TPR_LOG_STYLE_WARN1)
                << LOG_VFS_NAME ": Failed to remove temporary file " << mVirtualPath << ". If it is still there you can delete it manually\n";
        }
    }

}



