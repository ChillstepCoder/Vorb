#pragma once

// Handles managing dirtying objects in a thread safe manner
template <typename T>
class ThreadSafeDirtyList
{
public:
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
    void aquireAllDirtyObjects(std::set<T>& outObjects) {
        std::lock_guard lock(mMutex);
        std::swap(outObjects, mDirtyObjects);
    }
private:
    std::mutex mMutex;
    std::set<T> mDirtyObjects;
};

