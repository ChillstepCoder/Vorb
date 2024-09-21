#pragma once

#include "world/ChunkState.h"
#include "world/GridID.h"

#include "character/CharacterLocomotionMode.h"
#include "rendering/renderstate/CharacterRenderState.h"
#include "rendering/renderstate/DynamicModelInstanceState.h"
#include "rendering/renderstate/WorldTextRenderState.h"
#include "ecs/component/ThreadSharedComponent.h"

#include "interact/SelectedObjectData.h"

class World;

enum class DebugChunkFlags : ui8 {
    IS_NAVMESHING = 1 << 0,
};

enum class DebugChunkListIndex : ui8 {
    LOADING,
    ACTIVE,
    DESTROYING
};

struct DebugWireQuadState {
    f32v2 origin;
    f32v2 dims;
    color4 color;
};
static_assert(sizeof(DebugWireQuadState) == 20);

struct DebugChunkRenderState {
    ChunkID mId;
    i32v2 mWorldPos;
    ChunkState mState;
    DebugChunkListIndex mList;
    ui8 mRefCount;
    BitFlags<DebugChunkFlags> mFlags;
};

class WorldRenderState {
    friend class World;
    friend class GameRenderStateManager;
public:
    void onRenderThreadFinished() const {
        // Release handles
        mThreadSharedComponents.clear();
    }

    const f32v3& getCameraOwningEntityPos() const { return mCameraOwningEntityPos; }
    bool isCameraOwned() const { return mIsCameraOwned; }
    const std::vector<DebugChunkRenderState>& getDebugChunks() const { return mDebugChunks; }
    const std::vector<DebugWireQuadState>& getDebugQuads() const { return mDebugQuads; }
    const std::vector<CharacterRenderState>& getCharacterRenderState() const { return mCharacters; }
    DynamicModelInstanceStateContainer& getDynamicModels() { return mDynamicModels; }
    const std::vector<WorldTextRenderState>& getWorldText() const { return mWorldText; }
    WorldID getWorldId() const { return mWorldId; }
    const SelectedObjectData& getPlayerSelectedObject() const { return mPlayerSelectedObject; }
    std::vector<RenderThreadSharedComponentDataPtr>& getThreadSharedComponents() const { return mThreadSharedComponents; }
private:
    // ======================== Game State  ========================
    WorldID mWorldId = 0;
    f32v3 mCameraOwningEntityPos;
    bool mIsCameraOwned;
    std::vector<CharacterRenderState> mCharacters;
    DynamicModelInstanceStateContainer mDynamicModels;
    std::vector<WorldTextRenderState> mWorldText;
    mutable std::vector<RenderThreadSharedComponentDataPtr> mThreadSharedComponents;
    SelectedObjectData mPlayerSelectedObject;

    // ======================== Debug state ========================
    std::vector<DebugChunkRenderState> mDebugChunks;
    std::vector<DebugWireQuadState> mDebugQuads;
};

