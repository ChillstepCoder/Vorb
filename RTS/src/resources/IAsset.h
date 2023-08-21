#pragma once

class IAsset {
public:
    IAsset() = default;
    virtual ~IAsset() = default;

    VORB_MOVABLE(IAsset);

    vio::Path getDiskLocation() const { return mDiskLocation; }
    void setDiskLocation(vio::Path val) const { mDiskLocation = val; }

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }

    void incRef() const { ASSERT_GAME_THREAD(); ++mRefCount; }
    void decRef() const { ASSERT_GAME_THREAD(); --mRefCount; }
    int getRefCount() const { ASSERT_GAME_THREAD(); return mRefCount; }
protected:
    mutable vio::Path mDiskLocation;
    mutable int mRefCount;
    mutable bool mDirty = false;
};