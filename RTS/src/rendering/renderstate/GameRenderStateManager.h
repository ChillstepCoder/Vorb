#pragma once

#include "rendering/renderstate/WorldRenderState.h"

class World;

class GameRenderStateManager {
public:
    GameRenderStateManager() = default;
    ~GameRenderStateManager() = default;

    GameRenderStateManager(GameRenderStateManager& other) = delete;
    void operator=(const GameRenderStateManager&) = delete;

    static GameRenderStateManager& initInstance();
    static GameRenderStateManager& getInstance();
    static GameRenderStateManager* tryGetInstance() { return sInstance; }
    static bool exists() { return sInstance != nullptr; }

    void setActiveWorld(const World* activeWorld);

    bool isActiveWorld(const World* world);

    /// Gets the state for updating. Only call once per frame.
    WorldRenderState& getRenderStateForUpdate();
    /// Marks state as finished updating. Only call once per frame,
    /// must call for every call to getRenderStateForUpdate.
    void finishUpdating();
    /// Gets the state for rendering. Only call once per frame.
    const WorldRenderState& getRenderStateForRender();
private:
    const World* mActiveWorld = nullptr;
    int mUpdating = 0; ///< Currently updating state
    int mLastUpdated = 0; ///< Most recently updated state
    int mRendering = 0; ///< Currently rendering state
    WorldRenderState mRenderState[3]; ///< Triple-buffered state
    std::mutex mLock;
    std::mutex mWorldLock;

    static GameRenderStateManager* sInstance;
};
