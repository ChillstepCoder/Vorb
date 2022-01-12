#pragma once

class World;
class Camera3D;

#include "world/ChunkID.h"
#include "util/IntersectionHit.h"
class Brush;

enum class WorldEditorEditMode {
    TERRAIN,
    GRASS,
};

enum class TerrainEditState {
    RAISE_TERRAIN,
    LOWER_TERRAIN,
    FLATTEN_TERRAIN,
    COUNT
};

enum class GrassEditState {
    ADD,
    REMOVE,
    COUNT
};

class WorldEditor {
public:
    WorldEditor(World& world, const f32v2& screenDims);

    void update(const Camera3D& camera);


	void renderBrushDecals(const Camera3D& camera) const;
    void renderUI() const;

private:
    void updateTerrainEdit();
    void updateGrassEdit();
    void editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToVertex);
    void editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile);
    f32 getBrushStrengthAtPoint(f32 brushSize, const f32v2& brushOffsetToPoint);


    World& mWorld;
    f32v2 mScreenDims;

    mutable WorldEditorEditMode mEditMode = WorldEditorEditMode::TERRAIN;
    mutable TerrainEditState mTerrainEditState = TerrainEditState::RAISE_TERRAIN;
    mutable GrassEditState mGrassEditState = GrassEditState::ADD;
    mutable ui32 mActiveBrushID = UINT32_MAX;
    mutable const Brush* mActiveBrush = nullptr;
    mutable f32 mBrushSizeTerrain = 5.0f;
    mutable f32 mBrushStrengthTerrain = 0.1f;
    mutable f32 mBrushSizeGrass = 5.0f;
    mutable f32 mBrushStrengthGrass = 1.0f;
    TerrainPickData mPickData;
};

