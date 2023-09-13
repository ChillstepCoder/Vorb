#pragma once

// std::hardware_destructive_interference_size
#include <new>

 // Prevent false sharing
template <typename T>
class alignas(std::hardware_destructive_interference_size) ExclusiveCacheLine : public T {
public:
    using T::T;
};

template <IsAssetType T>
using ExclusiveCacheLinePtr = std::unique_ptr<ExclusiveCacheLine<T>>;