
#ifndef DATA_BRIDGE_DATA_BRIDGE_HPP_
#define DATA_BRIDGE_DATA_BRIDGE_HPP_


#include "plugin_core.h"

#include <functional>




struct OArray_T {
    private:

        std::function<void*()> dataHandle;
        std::function<void(size_t size)> resizeHandle;
        size_t size;

        size_t prevSize = 0;
        bool actual = true;
        bool finalized = true;
        friend class DataBridge;
};

struct TprOArrayEntityDrawDesc_T : public OArray_T {};



class DataBridge {

    public:

        template <typename T, typename = std::enable_if<std::is_same_v<std::remove_pointer_t<T>, OArray_T>>>
        struct OArrayHandle {
            public:
                ~OArrayHandle() = default;
                OArrayHandle(const OArrayHandle& other) = default;
                OArrayHandle(OArrayHandle&& other) = default;

                T opaqueHandle() { return &oarray; }
                bool finalized() { return bridge->isOArrayFinalized(*this); }
                void clear() { bridge->clearOArray(*this); }

            private:
                OArrayHandle() = default;
                std::remove_pointer_t<T> oarray;
                DataBridge* bridge;
                friend class DataBridge;
        };

        void init();
        void shutdown() noexcept;
        void update();

        template <typename T>
        OArrayHandle<T> registerOArray(std::function<void*()> dataHandle, std::function<void(size_t size)> resizeHandle, size_t originalSize) {
            
            OArrayHandle<T> handle;
            handle.bridge = this;
            handle.oarray.dataHandle = dataHandle;
            handle.oarray.resizeHandle = resizeHandle;
            handle.oarray.size = originalSize;

            return handle;
        }

        TprResult growOArray(OArray_T* oarray, uint32_t count, void** start) noexcept;
        TprResult endOArray(OArray_T* oarray, uint32_t actualSize) noexcept;
        TprResult sizeofOArray(OArray_T* oarray, uint32_t* size) noexcept;

    private:

        template <typename T>
        bool isOArrayFinalized(OArrayHandle<T>& handle) {
            return handle.oarray.finalized;
        }

        template <typename T>
        void clearOArray(OArrayHandle<T>& handle) {
            handle.oarray.resizeHandle(handle.oarray.prevSize);
            handle.oarray.size = handle.oarray.prevSize;
            handle.oarray.finalized = true;
        }

        /*
            UB DETECTED:
            Giving a pointer to a TprROFile_T from that vector.
            After a reallocation this pointer will be a dangling pointer.
            Should be switched to a stable identificator
        */

        template <typename, typename> friend struct OArrayHandle;

};



#endif  // DATA_BRIDGE_DATA_BRIDGE_HPP_
