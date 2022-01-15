#pragma once

class World;
class Camera3D;

#include "world/ChunkID.h"
#include "util/IntersectionHit.h"

class Brush;
class BrushRepository;

enum class WorldEditorEditMode {
    TERRAIN,
    GRASS,
    TILE,
    ENTITY,
    CITY,
    COUNT
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

enum class CityEditState {
    NONE,
    CREATE,
    COUNT
};

struct BrushSettings {
    const Brush* activeBrush;
    ui32 brushId;
    f32 brushSize;
    f32 brushStrength;
};

class WorldEditor {
public:
    WorldEditor(World& world, const f32v2& screenDims);

    void update(const Camera3D& camera);

	void renderBrushDecals(const Camera3D& camera) const;
    void renderUI() const;

private:
    void renderModeButtons() const;
    void tryRenderBrushSelect(const BrushRepository& brushRepo) const;
    void renderTerrainEditUI() const;
    void renderGrassEditUI() const;
    void renderTileEditUI() const;
    void renderEntityEditUI() const;
    void renderCityEditUI() const;

    void updateTerrainEdit();
    void updateGrassEdit();
    void updateTileEdit();
    void updateEntityEdit();
    void updateCityEdit();

    void editVertex(ChunkID id, const ui32v2& vertPos, const f32v2& offsetToVertex);
    void editGrass(ChunkID id, TileIndex tileIndex, const f32v2& offsetToTile);
    f32 getBrushStrengthAtPoint(const f32v2& brushOffsetToPoint);
    void setEditMode(WorldEditorEditMode mode) const;

    World& mWorld;
    f32v2 mScreenDims;

    mutable WorldEditorEditMode mEditMode = WorldEditorEditMode::TERRAIN;
    mutable TerrainEditState mTerrainEditState = TerrainEditState::RAISE_TERRAIN;
    mutable GrassEditState mGrassEditState = GrassEditState::ADD;
    mutable CityEditState mCityEditState = CityEditState::CREATE;
    mutable BrushSettings mTerrainBrushSettings = { nullptr, UINT32_MAX, 5.0f, 0.1f };
    mutable BrushSettings mGrassBrushSettings = { nullptr, UINT32_MAX, 5.0f, 1.0f };
    mutable BrushSettings* mCurrentBrushSettings = &mTerrainBrushSettings;
    mutable ui32 mSelectedTile = 0;
    mutable nString mSelectedEntity = "";
    TerrainPickData mPickData;
};

