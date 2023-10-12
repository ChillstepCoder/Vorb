#pragma once

#include "world/ChunkState.h"
#include "world/ChunkID.h"

#include "character/CharacterConst.h"
#include "rendering/renderstate/CharacterRenderState.h"

class IWorld;

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

class RenderState {
    friend class IWorld;
    friend class GameRenderStateManager;
public:
    const f32v2& getWorldLoadCenter() const { return mWorldLoadCenter; }
    const f32v3& getCameraOwningEntityPos() const { return mCameraOwningEntityPos; }
    bool isCameraOwned() const { return mIsCameraOwned; }
    const std::vector<DebugChunkRenderState>& getDebugChunks() const { return mDebugChunks; }
    const std::vector<DebugWireQuadState>& getDebugQuads() const { return mDebugQuads; }
    const std::vector<CharacterRenderState>& getCharacterRenderState() const { return mCharacters; }
    IWorld* getWorld() const { return mWorld; }
private:
    // ======================== Game State  ========================
    IWorld* mWorld;
    f32v2 mWorldLoadCenter;
    f32v3 mCameraOwningEntityPos;
    bool mIsCameraOwned;
    std::vector<CharacterRenderState> mCharacters;

    // ======================== Debug state ========================
    std::vector<DebugChunkRenderState> mDebugChunks;
    std::vector<DebugWireQuadState> mDebugQuads;
};

