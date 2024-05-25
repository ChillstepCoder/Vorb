#pragma once

// Fixed size vector, mainly for allocating on the stack
template<typename T, i16 N>
class FixedSizeVector {
public:
    FixedSizeVector() : mSize(0) {}

    ~FixedSizeVector() {
        clear();
    }

    bool try_push_back(const T& value) {
        if (mSize < N) {
            new(&mData[mSize]) T(value);
            ++mSize;
            return true;
        }
        return false;
    }
    bool try_push_back(T&& value) {
        if (mSize < N) {
            new(&mData[mSize]) T(std::move(value));
            ++mSize;
            return true;
        }
        return false;
    }

    template<typename... Args>
    bool try_emplace_back(Args&&... args) {
        if (mSize < N) {
            new(&mData[mSize]) T(std::forward<Args>(args)...);
            ++mSize;
            return true;
        }
        return false;
    }

    void pop_back() {
        assert(mSize > 0);
        if (mSize > 0) {
            mData[--mSize].~T();
        }
    }

    void resize(i16 newSize) {
        assert(newSize <= N);
        
        if (newSize < mSize) {
            for (i16 i = newSize; i < mSize; ++i) {
                mData[i].~T();
            }
        }
        else if (newSize > mSize) {
            if (newSize > N) {
                newSize = N;
            }
            for (i16 i = mSize; i < newSize; ++i) {
                new(&mData[i]) T();
            }
        }
        mSize = newSize;
    }

    void clear() {
        resize(0);
    }

    T& operator[](i16 index) {
        return mData[index];
    }

    const T& operator[](i16 index) const {
        return mData[index];
    }

    i16 size() const {
        return mSize;
    }

    T* begin() {
        return reinterpret_cast<T*>(mData);
    }

    T* end() {
        return reinterpret_cast<T*>(mData + mSize);
    }

    const T* begin() const {
        return reinterpret_cast<const T*>(mData);
    }

    const T* end() const {
        return reinterpret_cast<const T*>(mData + mSize);
    }

    static consteval i16 getMaxSize() {
        return N;
    }

private:
    typename std::aligned_storage<sizeof(T), alignof(T)>::type mData[N];
    i16 mSize;
};

