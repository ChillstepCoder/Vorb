#pragma once

#include "world/ChunkState.h"
#include "world/ChunkID.h"

enum class DebugChunkFlags : ui8 {
    IS_NAVMESHING = 1 << 0,
};

enum class DebugChunkListIndex : ui8 {
    LOADING,
    ACTIVE,
    DESTROYING
};

struct DebugChunkRenderState {
    ChunkID mId;
    ChunkState mState;
    DebugChunkListIndex mList;
    ui8 mRefCount;
    ui8 mReadLockCount;
    BitFlags<DebugChunkFlags> mFlags;
};
static_assert(sizeof(DebugChunkRenderState) == 20);

class RenderState {
    friend class CliWorldInterface;
public:
    const f32v2& getWorldLoadCenter() const { return mWorldLoadCenter; }
    const f32v3& getCameraOwningEntityPos() const { return mCameraOwningEntityPos; }
    const std::vector<DebugChunkRenderState>& getDebugChunks() const { return mDebugChunks; }
private:
    // ======================== Game State  ========================
    f32v2 mWorldLoadCenter;
    f32v3 mCameraOwningEntityPos;

    // ======================== Debug state ========================
    std::vector<DebugChunkRenderState> mDebugChunks;
};

