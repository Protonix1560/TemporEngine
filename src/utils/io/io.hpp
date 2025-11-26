

#ifndef UTILS_IO_IO_HPP_
#define UTILS_IO_IO_HPP_


#include "core.hpp"

#include <filesystem>
#include <mio/mmap.hpp>
#include <map>
#include <vector>



// aligned allocator

#if defined(_WIN32)
#include <malloc.h>
#endif

template <typename T>
class AlignedAllocator {

    public:

        // for compatibility with STL
        template <typename U>
        struct rebind {
            using other = AlignedAllocator<U>;
        };
        using value_type = T;


        explicit AlignedAllocator(size_t align = alignof(T)) noexcept
            : align(align) {
            if (align < alignof(T)) {
                align = alignof(T);
            }
        }

        template <typename U>
        AlignedAllocator(const AlignedAllocator<U>& other) noexcept
            : align(other.alignment()) {}

            
        T* allocate(size_t n) {
            if (n == 0) return nullptr;
            void* ptr;
            #ifdef _WIN32
                if (align <= alignof(max_align_t)) {
                    ptr = malloc(n * sizeof(T));
                } else {
                    ptr = _aligned_malloc(n * sizeof(T), align);
                }
            #else
                if (align <= alignof(max_align_t)) {
                    ptr = malloc(n * sizeof(T));
                } else {
                    if (posix_memalign(&ptr, align, n * sizeof(T)) != 0) {
                        ptr = nullptr;
                    }
                }
            #endif
            if (!ptr) throw std::bad_alloc();
            return static_cast<T*>(ptr);
        }

        void deallocate(T* p, size_t n) noexcept {
            if (!p) return;
            #ifdef _WIN32
                if (align <= alignof(max_align_t)) {
                    free(p);
                } else {
                    _aligned_free(p);
                }
            #else
                free(p);
            #endif
        }

        size_t alignment() const noexcept {
            return align;
        }

    private:
        size_t align;

};

template <typename T, typename U>
bool operator==(const AlignedAllocator<T>& a, const AlignedAllocator<U>& b) noexcept {
    return a.alignment() == b.alignment();
}

template <typename T, typename U>
bool operator!=(const AlignedAllocator<T>& a, const AlignedAllocator<U>& b) noexcept {
    return a.alignment() != b.alignment();
}



struct Iterator {

    public:
        Iterator& operator++() { ++index; return *this; }
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
        Iterator& operator--() { --index; return *this; }
        Iterator operator--(int) { Iterator tmp = *this; --(*this); return tmp; }
        Iterator operator+(size_t n) const { return Iterator(index + n); }
        Iterator operator-(size_t n) const { return Iterator(index - n); }
        size_t operator-(const Iterator& other) const { return index - other.index; }
        bool operator>(const Iterator& other) const { return index > other.index; }
        bool operator<(const Iterator& other) const { return index < other.index; }
        bool operator>=(const Iterator& other) const { return index >= other.index; }
        bool operator<=(const Iterator& other) const { return index <= other.index; }
        bool operator==(const Iterator& other) const { return index == other.index; }
        bool operator!=(const Iterator& other) const { return index != other.index; }

        Iterator(size_t index)
            : index(index) {}

    private:
        friend const std::byte* operator+(const std::byte* data, const Iterator& it) { return data + it.index; }
        friend const std::byte* operator+(const Iterator& it, const std::byte* data) { return data + it.index; }

        size_t index;

        friend struct ROPacket;
};



struct ROSpan {

    public:
        size_t size() { return mSize; }
        const std::byte* data() { return mpData; }
        const std::byte* begin() { return mpData; }
        const std::byte* end() { return mpData + mSize; }

    private:
        ROSpan(size_t size, const std::byte* data)
            : mSize(size), mpData(data) {}

        size_t mSize;
        const std::byte* mpData;

        friend struct ROPacket;
};

struct ROPacket {

    public:
        ROSpan read(const Iterator& start, const Iterator& end, size_t align = 0) const;
        ROSpan read(const Iterator& end, size_t align = 0) const { return read(0, end, align); }
        ROSpan read(size_t align = 0) const { return read(0, mSize, align); };
        std::byte operator[](const Iterator& it) const;
        size_t size() const;
        std::filesystem::path getVirtualPath() const;

        ~ROPacket() noexcept;

    private:
        ROPacket(std::filesystem::path path, const GlobalServiceLocator* pServiceLocator)
            : mMmap(path), mPath(path), mpServiceLocator(pServiceLocator) {

            mpData = reinterpret_cast<const std::byte*>(mMmap.data());
            mSize = static_cast<size_t>(mMmap.size());
        }

        ROPacket(ROPacket&& other)
            : mMmap(other.mPath), mPath(other.mPath), mpServiceLocator(other.mpServiceLocator) {

            mpData = reinterpret_cast<const std::byte*>(mMmap.data());
            mSize = static_cast<size_t>(mMmap.size());
        }

        struct View {
            Iterator start;
            Iterator end;
            size_t align;
            bool operator<(const View& other) const {
                if (start != other.start) return start < other.start;
                if (end != other.end) return end < other.end;
                return align < other.align;
            }
        };

        mio::mmap_source mMmap;
        const std::byte* mpData;
        size_t mSize;
        std::filesystem::path mPath;
        const GlobalServiceLocator* mpServiceLocator;

        #if defined(__linux__)
            mutable int mVfd = -1;
        #endif

        mutable std::map<View, std::vector<std::byte, AlignedAllocator<std::byte>>> mAlignedData;
        mutable std::filesystem::path mVirtualPath;

        friend class IOManager;

};



class IOManager {

    public:
        void init(const GlobalServiceLocator* pServiceLocator);
        ROPacket openRO(std::filesystem::path path);
        void shutdown() noexcept;
    
        private:
            const GlobalServiceLocator* mpServiceLocator;

};




#endif  // UTILS_IO_IO_HPP_
