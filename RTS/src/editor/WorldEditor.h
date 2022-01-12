#pragma once

class World;
class Camera3D;

#include "world/ChunkID.h"
#include "util/IntersectionHit.h"
class Brush;

enum class WorldEditorEditState {
    NONE,
    RAISE_TERRAIN,
    LOWER_TERRAIN,
    FLATTEN_TERRAIN,
    COUNT
};

class WorldEditor {
public:
    WorldEditor(World& world, const f32v2& screenDims);

    void update(const Camera3D& camera);
    void renderBrushDecals(const Camera3D& camera) const;
    void renderUI() const;

private:
    void editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToVertex);

    World& mWorld;
    f32v2 mScreenDims;

    mutable WorldEditorEditState mEditState = WorldEditorEditState::NONE;
    mutable ui32 mActiveBrushID = UINT32_MAX;
    mutable const Brush* mActiveBrush = nullptr;
    mutable f32 mBrushSize = 5.0f;
    mutable f32 mBrushStrength = 0.1f;
    TerrainPickData mPickData;
};

