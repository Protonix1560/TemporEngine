
#include "io.hpp"
#include "logger.hpp"
#include "plugin.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>


#if defined(__linux__)
    #include <linux/memfd.h>
    #include <sys/mman.h>
#endif




ROPacket IOManager::openRO(std::filesystem::path path) {

    ROPacket packet(path, mpServiceLocator);
    
    return packet;
}


void IOManager::init(const GlobalServiceLocator* pServiceLocator) {
    mpServiceLocator = pServiceLocator;
}


void IOManager::shutdown() noexcept {

}




ROSpan ROPacket::read(const Iterator& start, const Iterator& end, size_t align) const {

    if (start == end) throw Exception(ErrCode::InternalError, "ROPacket::read: start iterator mustn't be equal to end iterator");

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


std::filesystem::path ROPacket::getVirtualPath() const {

    auto& log = mpServiceLocator->get<Logger>();

    // trying to use memfd to create an anonymous memory file
    // normally works only on linux >= 3.17
    #if defined(__linux__)
        log.trace() << "Trying to create anonymous memory file using memfd...\n";
        mVfd = memfd_create(mPath.c_str(), MFD_CLOEXEC);
        if (mVfd < 0) {
            log.error(TPR_LOG_STYLE_ERROR1) << "Failed to create anonymous memory file. Fallbacking to tmp file\n";
            goto memfd_attempt_failure;
        }
        if (write(mVfd, mpData, mSize) != mSize) {
            log.error(TPR_LOG_STYLE_ERROR1) << "Failed to write to anonymous memory file. Fallbacking to tmp file\n";
            close(mVfd);
            mVfd = -1;
            goto memfd_attempt_failure;
        }
        mVirtualPath = "/proc/self/fd/"s + std::to_string(mVfd);
        log.trace() << "Created an anonymous memory file " << mVirtualPath << "\n";
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
        if (!ofs) throw Exception(ErrCode::InternalError, "Failed to create tmp file "s + tempFile.string());
        mpServiceLocator->get<Logger>().trace() << "Created tmp file " << tempFile << "\n";
        ofs.write(reinterpret_cast<const char*>(mpData), mSize);
    } catch (...) {
        ofs.close();
        throw;
    }

    ofs.close();

    mVirtualPath = tempFile;

    return mVirtualPath;
}


ROPacket::~ROPacket() noexcept {

    #if defined(__linux__)
        if (mVfd != -1) {
            mpServiceLocator->get<Logger>().trace() << "Closed anonymous memory file " << mVirtualPath << "\n";
            close(mVfd);
            mVirtualPath = "";
        }
    #endif

    if (!mVirtualPath.empty()) {
        try {
            if (!std::filesystem::remove(mVirtualPath)) throw std::runtime_error("");
            mpServiceLocator->get<Logger>().trace() << "Removed tmp file " << mVirtualPath << "\n";

        } catch(const std::exception&) {
            mpServiceLocator->get<Logger>().warn(TPR_LOG_STYLE_WARN1)
                << "Failed to remove temporary file " << mVirtualPath << ". If it is still there you can delete it manually\n";
        }
    }

}



