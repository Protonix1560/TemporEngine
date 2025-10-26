#pragma once

#include "core.hpp"

#include <fstream>
#include <vector>
#include <filesystem>




class IOManager;



class FileReader {
    public:
        virtual bool read(void* dst, size_t size) = 0;
        virtual bool seek(size_t offset) = 0;
        virtual size_t tell() = 0;
        virtual size_t size() = 0;
        virtual ~FileReader() = default;
};



struct File {
    private:
        std::unique_ptr<FileReader> reader;
        explicit File(std::unique_ptr<FileReader> r)
            : reader(std::move(r)) {}
        friend class IOManager;

    public:
        File() = default;
        File(File&&) noexcept = default;
        File& operator=(File&&) noexcept = default;
        File(const File&) = delete;
        File& operator=(const File&) = delete;

        bool read(void* dst, size_t size) { return reader->read(dst, size); };
        bool seek(size_t offset) { return reader->seek(offset); };
        size_t tell() { return reader->tell(); };
        size_t size() { return reader->size(); };
};



class FileReaderFS : public FileReader {
    public:
        bool read(void* dst, size_t size) override {
            mStream.read(reinterpret_cast<char*>(dst), size);
            return static_cast<size_t>(mStream.gcount()) == size;
        }
        bool seek(size_t offset) override {
            return !!mStream.seekg(offset);
        }
        size_t tell() override {
            return mStream.tellg();
        }
        size_t size() override {
            auto current = mStream.tellg();
            if (current == -1) return 0;
            mStream.seekg(0, std::ios::end);
            auto end = mStream.tellg();
            mStream.seekg(current);
            return (end == -1) ? 0 : static_cast<size_t>(end);
        }
    private:
        std::ifstream mStream;
        friend class IOManager;
};



class IOManager {

    public:
        File open(std::filesystem::path filepath) {
            std::unique_ptr<FileReaderFS> reader = std::make_unique<FileReaderFS>();
            reader->mStream.open(filepath, std::ios::binary);
            if (!reader->mStream.is_open()) throw Exception(ErrCode::IOError, "Failed to open "s + filepath.string());
            return File{ std::move(reader) };
        }

        std::vector<std::byte> readAll(std::filesystem::path filepath) {
            File file = open(filepath);
            std::vector<std::byte> buffer(file.size());
            if (!file.read(buffer.data(), buffer.size())) throw Exception(ErrCode::IOError, "Failed to read "s + filepath.string());
            return buffer;
        }

};

