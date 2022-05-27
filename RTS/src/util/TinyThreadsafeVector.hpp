#pragma once

// Intended for small, mostly static data.
// Up to user to manage write lock

// Usage problem if we get this big...
constexpr ui16 MAX_TINY_THREADSAFE_VECTOR_DATA_SIZE = 500;

template <typename T>
class TinyThreadsafeVector {
public:

    void add(const T& val, bool isReadLocked);
    void remove(const T& val, bool isReadLocked);
    void copyThreadData(); // Must not be read locked

    std::pair<const T*, ui16> getMainThreadData() const { assert(IS_MAIN_THREAD()); return std::make_pair(mMainThreadData ? mMainThreadData.get() : nullptr, mMainThreadDataSize); }
    std::pair<const T*, ui16> getWorkerThreadData() const { assert(!IS_MAIN_THREAD()); return std::make_pair(mWorkerThreadData ? mWorkerThreadData.get() : nullptr, mWorkerThreadDataSize); }
    ui16 getMainThreadDataSize() { assert(IS_MAIN_THREAD()); return mMainThreadDataSize; }
    ui16 getWorkerThreadDataSize() { assert(!IS_MAIN_THREAD()); return mWorkerThreadDataSize; }

    void setQueuedWorkerThreadCopy() { mIsQueuedWorkerThreadCopy = true; }
    bool isQueuedWorkerThreadCopy() const { return mIsQueuedWorkerThreadCopy; }

private:
    void addInternal(std::unique_ptr<T[]>& dataArray, ui16& size, const T& val);

    std::unique_ptr<T[]> mMainThreadData;
    std::unique_ptr<T[]> mWorkerThreadData;
    ui16 mMainThreadDataSize = 0;
    ui16 mWorkerThreadDataSize = 0;
    bool mIsQueuedWorkerThreadCopy;
};

template <typename T>
void TinyThreadsafeVector<T>::copyThreadData() {
    assert(IS_MAIN_THREAD() && mIsQueuedWorkerThreadCopy);
    mWorkerThreadData = std::unique_ptr<T[]>(new T[mMainThreadDataSize]);
    memcpy(*mWorkerThreadData, *mMainThreadData, mMainThreadData * sizeof(T));
    mIsQueuedWorkerThreadCopy = false;
}

template <typename T>
void TinyThreadsafeVector<T>::remove(const T& val, bool isReadLocked) {
    assert(false); // TODO: Implement
}

template <typename T>
void TinyThreadsafeVector<T>::add(const T& val, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    addInternal(mMainThreadData, mMainThreadDataSize, val);
    // If not read locked, do the same to the worker thread data
    // if we arent queued to copy later
    if (!isReadLocked && !mIsQueuedWorkerThreadCopy) {
        addInternal(mWorkerThreadData, mWorkerThreadDataSize, val);
    }
}

template <typename T>
void TinyThreadsafeVector<T>::addInternal(std::unique_ptr<T[]>& dataArray, ui16& size, const T& val) {
    if (size) {
        assert(size < MAX_TINY_THREADSAFE_VECTOR_DATA_SIZE);
        // Resize the array and copy data
        std::unique_ptr<T[]> newArray(new T[size + 1]);
        for (ui16 i = 0; i < size; ++i) {
            newArray[i] = size[i];
        }
        (*newArray)[size] = val;
        ++size;
        dataArray = std::move(newArray);
    }
    else {
        dataArray = std::unique_ptr<T[]>(new T[1]);
        size = 1;
        (*dataArray)[0] = val;
    }
}