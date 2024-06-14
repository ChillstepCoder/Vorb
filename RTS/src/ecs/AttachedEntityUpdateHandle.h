#pragma once

class IAttachedEntityUpdateHandle;

typedef std::function<void(entt::entity, entt::registry&, IAttachedEntityUpdateHandle*)> EntityAttachedUpdateFunc;

using AttachedEntityUpdateTypeID = ui32;
constexpr AttachedEntityUpdateTypeID INVALID_ATTACHED_ENTITY_UPDATE_TYPE = std::numeric_limits<AttachedEntityUpdateTypeID>::max();

class IAttachedEntityUpdateHandle {
    friend class AttachedEntityUpdater;
public:
    IAttachedEntityUpdateHandle(AttachedEntityUpdateTypeID updateIDType, EntityAttachedUpdateFunc func) : mFunc(func), mUpdateTypeID(updateIDType) {}
    virtual ~IAttachedEntityUpdateHandle() = default;

    // Returns false when the entity is no longer valid
    bool isValid() const { return mIsValid; }
    AttachedEntityUpdateTypeID getUpdateTypeID() const { return mUpdateTypeID; }

protected:
    std::mutex mMutex;
private:
    EntityAttachedUpdateFunc mFunc;
    bool mIsValid = true;
    AttachedEntityUpdateTypeID mUpdateTypeID = 0;
};
typedef std::shared_ptr<IAttachedEntityUpdateHandle> AttachedEntityUpdateHandlePtr;

// Owned by whoever applies the update function. If destroyed, removes update.
// Func will be called with IAttachedEntityUpdateHandle* which can be dynamic_cast to 
// the correct type.
template <typename T>
class AttachedEntityUpdateHandle : public IAttachedEntityUpdateHandle {
    friend class AttachedEntityUpdater;
public:
    using IAttachedEntityUpdateHandle::IAttachedEntityUpdateHandle;

    T getDataCopy() {
        std::lock_guard<std::mutex> lock(mMutex);
        return data;
    }

    void doThreadSafe(std::function<void(T&)> func) {
        std::lock_guard<std::mutex> lock(mMutex);
        func(data);
    }

private:
    T data;
};