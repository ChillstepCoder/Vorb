#pragma once

#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

// Handles managing dirtying objects in a thread safe manner
template <typename T>
class ThreadSafeDirtySet
{
public:

    void dirtyObject(T obj) {
        std::lock_guard lock(mMutex);
        mDirtyObjects.insert(obj);
    }

    // Returns false if already dirty
    bool tryDirtyObject(T obj) {
        bool didAdd = false;
        { // Critical section
            std::lock_guard lock(mMutex);
            auto&& it = mDirtyObjects.find(obj);
            if (it == mDirtyObjects.end()) {
                didAdd = true;
                mDirtyObjects.insert(obj);
            }
        }
        return didAdd;
    }

    // Returns false if not dirty
    bool tryRemoveDirtyObject(T obj) {
        bool didRemove = false;
        {
            std::lock_guard lock(mMutex);
            auto&& it = mDirtyObjects.find(obj);
            if (it == mDirtyObjects.end()) {
                didRemove = true;
                mDirtyObjects.erase(it);
            }
        }
        return didRemove;
    }
    
    // Will fill outObjects with all currently dirty objects and clear
    void aquireAllDirtyObjects(std::unordered_set<T>& outObjects) {
        std::lock_guard lock(mMutex);
        std::swap(outObjects, mDirtyObjects);
    }
private:
    std::mutex mMutex;
    std::unordered_set<T> mDirtyObjects;
};

// Intended to flush dirty objects to another thread once per frame
template <typename T>
class GameThreadBatchedDirtySet
{
public:
    // Returns false if already dirty
    bool gameThreadTryDirtyObject(T obj) {
        ASSERT_GAME_THREAD();
        auto&& it = mDirtyObjectsGameThread.find(obj);
        if (it == mDirtyObjectsGameThread.end()) {
            mDirtyObjectsGameThread.insert(obj);
            return true;
        }
        return false;
    }

    //// Returns false if already dirty
    //bool workerThreadTryDirtyObject(T obj) {
    //    assert(!IS_GAME_THREAD());
    //    bool didAdd = false;
    //    { // Critical section
    //        std::lock_guard lock(mMutex);
    //        auto&& it = mDirtyObjectsWorkerThread.find(obj);
    //        if (it == mDirtyObjectsWorkerThread.end()) {
    //            didAdd = true;
    //            mDirtyObjectsWorkerThread.insert(obj);
    //        }
    //    }
    //    return didAdd;
    //}

    // Will fill outObjects with all currently dirty objects and clear, but only if the worker thread already cleared it before
    // As otherwise we can end up with duplicates in the workerThread set which can cause dangling refs (as they combine)
    void gameThreadCopyToWorkerThread() {
        ASSERT_GAME_THREAD();
        {
            std::lock_guard lock(mMutex);
            if (mDirtyObjectsWorkerThread.empty()) {
                std::swap(mDirtyObjectsWorkerThread, mDirtyObjectsGameThread);
            }
        }
    }

    // Will fill outObjects with all currently dirty objects and clear
    void workerThreadAquireAllDirtyObjects(boost::container::flat_set<T>& outObjects) {
        assert(!IS_GAME_THREAD());
        std::lock_guard lock(mMutex);
        std::swap(outObjects, mDirtyObjectsWorkerThread);
        mDirtyObjectsWorkerThread.clear();
    }
private:
    std::mutex mMutex;
    boost::container::flat_set<T> mDirtyObjectsGameThread;
    boost::container::flat_set<T> mDirtyObjectsWorkerThread;
};

// Intended to flush dirty objects to another thread once per frame but with nested sets
// Useful for example, to assosicate a set of Tile IDs with a specific ChunkID
template <typename K, typename V>
class GameThreadBatchedDirtyMapSet
{
    typedef boost::container::flat_map<K, boost::container::flat_set<V>> MapType;
public:
    // Returns false if already dirty
    bool gameThreadTryDirtyObject(K key, const V& val) {
        ASSERT_GAME_THREAD();
        bool didAdd = false;
        auto&& it = mDirtyObjectsGameThread.find(key);
        if (it == mDirtyObjectsGameThread.end()) {
            didAdd = true;
            auto&& it2 = mDirtyObjectsGameThread.insert(std::make_pair(key, boost::container::flat_set<V>())).first;
            it2.second.insert(val);
        }
        else {
            auto&& it2 = it.second.find(val);
            if (it2 == it.second.end()) {
                it.second.insert(val);
                didAdd = true;
            }
        }
        return didAdd;
    }

    // Will fill outObjects with all currently dirty objects and clear
    void gameThreadCopyToWorkerThread() {
        ASSERT_GAME_THREAD();
        {
            std::lock_guard lock(mMutex);
            for (auto&& gameIt : mDirtyObjectsGameThread) {
                auto&& workerIt = mDirtyObjectsWorkerThread.find(gameIt.first);
                if (workerIt != mDirtyObjectsWorkerThread) {
                    // Need to merge subset
                    workerIt.second.merge(gameIt.second);
                }
                else {
                    // Just copy the whole set
                    mDirtyObjectsWorkerThread.insert(std::make_pair(gameIt.first, std::move(gameIt.second)));
                }
            }
        }
        mDirtyObjectsGameThread.clear();
    }

    // Will fill outObjects with all currently dirty objects and clear
    void workerThreadAquireAllDirtyObjects(MapType& outObjects) {
        assert(!IS_GAME_THREAD());
        std::lock_guard lock(mMutex);
        mDirtyObjectsWorkerThread.swap(outObjects);
    }
private:
    std::mutex mMutex;
    MapType mDirtyObjectsGameThread;
    MapType mDirtyObjectsWorkerThread;
};

// Intended to flush dirty objects to another thread once per frame
template <typename T>
class GameThreadBatchedDirtyVector
{
public:
    // Returns false if already dirty
    void gameThreadDirtyObject(const T& obj) {
        ASSERT_GAME_THREAD();
        mDirtyObjectsGameThread.emplace_back(obj);
    }
    void gameThreadDirtyObject(T&& obj) {
        ASSERT_GAME_THREAD();
        mDirtyObjectsGameThread.emplace_back(std::move(obj));
    }

    // Will fill outObjects with all currently dirty objects and clear
    void gameThreadCopyToWorkerThread() {
        ASSERT_GAME_THREAD();
        {
            std::lock_guard lock(mMutex);
            mDirtyObjectsWorkerThread.reserve(mDirtyObjectsWorkerThread.size() + mDirtyObjectsGameThread.size());
            mDirtyObjectsWorkerThread.insert(mDirtyObjectsWorkerThread.end(), mDirtyObjectsGameThread.begin(), mDirtyObjectsGameThread.end());
        }
        mDirtyObjectsGameThread.clear();
    }

    // Will fill outObjects with all currently dirty objects and clear
    void workerThreadAquireAllDirtyObjects(std::vector<T>& outObjects) {
        assert(!IS_GAME_THREAD());
        std::lock_guard lock(mMutex);
        std::swap(outObjects, mDirtyObjectsWorkerThread);
    }
private:
    std::mutex mMutex;
    std::vector<T> mDirtyObjectsGameThread;
    std::vector<T> mDirtyObjectsWorkerThread;
};