#pragma once

#include <concepts>

// Allows contiguous allocation of arbitrary objects up to a max total size
// TODO: Unit test this
class ArbitraryObjectArray {
public:
    static constexpr size_t BLOCK_SIZE = 4096;
    static_assert(BLOCK_SIZE % sizeof(uint64_t) == 0);

    ArbitraryObjectArray() {
        addBlock(); // initial buffer
    }
    ~ArbitraryObjectArray() {
        // Call the destructors for the objects
        for (auto& destructor : mDestructors) {
            destructor();
        }
    }

    VORB_NON_COPYABLE_BUT_MOVABLE(ArbitraryObjectArray);

    void* operator[](size_t i) {
        return mDataPtrs[i];
    }

    template<typename T> requires std::copy_constructible<T>
    T* addObject(const T& object) {
        size_t alignment = alignof(T);
        std::size_t space = BLOCK_SIZE - mCurrentSizeBytes;
        void* p = (void*)(reinterpret_cast<char*>(mData.back().get() + mCurrentSizeBytes));
        void* alignedPtr = std::align(alignment, sizeof(T), p, space);

        // Check if alignment pushed us past the current buffer, or if there wasn't enough space to begin with
        if (alignedPtr == nullptr || space < sizeof(T)) {
            addBlock(); // add new buffer

            // Try again with the new buffer
            space = BLOCK_SIZE - mCurrentSizeBytes;
            p = (void*)(reinterpret_cast<char*>(mData.back().get() + mCurrentSizeBytes));
            alignedPtr = std::align(alignment, sizeof(T), p, space);
        }

        assert(alignedPtr);

        T* newObject = new (alignedPtr) T(object); // placement new

        mCurrentSizeBytes = reinterpret_cast<char*>(alignedPtr) - reinterpret_cast<char*>(mData.back().get()) + sizeof(T);

        // Save a lambda that will destroy the object
        mDestructors.push_back([=]() { newObject->~T(); });

        mDataPtrs.emplace_back(newObject);

        return newObject;
    }

protected:
    void addBlock() {
        mData.emplace_back(std::unique_ptr<uint64_t[]>(new uint64_t[BLOCK_SIZE / sizeof(uint64_t)]));
    }

    std::vector<std::unique_ptr<uint64_t[]>> mData;
    std::vector<void*> mDataPtrs;
    std::vector<std::function<void()>> mDestructors;
    size_t mCurrentSizeBytes = 0;
};

