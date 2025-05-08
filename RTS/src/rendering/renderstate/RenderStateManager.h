#pragma once
template <typename T>
class RenderStateManager {
public:
    /// Gets the state for updating. Only call once per frame.
    T& getRenderStateForUpdate() {
        ASSERT_GAME_THREAD();
        {
            std::lock_guard<std::mutex> lock(mLock);
            // Get the next free state
            incrementMod3(mUpdating);
            if (mUpdating == mRendering) {
                incrementMod3(mUpdating);
            }
        }
        return mRenderState[mUpdating];
    }
    /// Marks state as finished updating. Only call once per frame,
    /// must call for every call to getRenderStateForUpdate.
    void finishUpdating() {
        ASSERT_GAME_THREAD();
        std::lock_guard<std::mutex> lock(mLock);
        // Mark the currently updating buffer as the last updated
        mLastUpdated = mUpdating;
    }
    /// Gets the state for rendering. Only call once per frame.
    const T& getRenderStateForRender() {
        ASSERT_RENDER_THREAD();
        {
            std::lock_guard<std::mutex> lock(mLock);
            // Render the last updated state
            mRendering = mLastUpdated;
        }
        return mRenderState[mRendering];
    }
private:
    int mUpdating = 0; ///< Currently updating state
    int mLastUpdated = 0; ///< Most recently updated state
    int mRendering = 0; ///< Currently rendering state
    T mRenderState[3]; ///< Triple-buffered state
    std::mutex mLock;
};

