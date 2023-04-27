#pragma once

#include "rendering/renderstate/RenderState.h"

class IWorld;

class RenderStateManager {
public:
    RenderStateManager() = default;
    ~RenderStateManager() = default;

    RenderStateManager(RenderStateManager& other) = delete;
    void operator=(const RenderStateManager&) = delete;

    static RenderStateManager& initInstance();
    static RenderStateManager& getInstance();
    static RenderStateManager* tryGetInstance() { return sInstance; }
    static bool exists() { return sInstance != nullptr; }

    void setActiveWorld(const IWorld* activeWorld);

    bool isActiveWorld(const IWorld* world);

    /// Gets the state for updating. Only call once per frame.
    RenderState& getRenderStateForUpdate();
    /// Marks state as finished updating. Only call once per frame,
    /// must call for every call to getRenderStateForUpdate.
    void finishUpdating();
    /// Gets the state for rendering. Only call once per frame.
    const RenderState& getRenderStateForRender();
private:
    const IWorld* mActiveWorld = nullptr;
    int mUpdating = 0; ///< Currently updating state
    int mLastUpdated = 0; ///< Most recently updated state
    int mRendering = 0; ///< Currently rendering state
    RenderState mRenderState[3]; ///< Triple-buffered state
    std::mutex mLock;
    std::mutex mWorldLock;

    static RenderStateManager* sInstance;
};

